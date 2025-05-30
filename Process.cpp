
#include "stdafx.h"
#include "LiteRun.h"
#include <UserEnv.h>
#include <winsafer.h>
#include <Sddl.h>
#include <Psapi.h>
#include <WtsApi32.h>
#include <vector>
#include <string>
#include "Encryption.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool CheckTokenPrivileges(HANDLE hToken)
{
	DWORD length = 0;
	GetTokenInformation(hToken, TokenPrivileges, NULL, 0, &length);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		Log(L"Failed to get token information size", true);
		return false;
	}

	std::vector<BYTE> buffer(length);
	TOKEN_PRIVILEGES* privileges = reinterpret_cast<TOKEN_PRIVILEGES*>(buffer.data());

	if (!GetTokenInformation(hToken, TokenPrivileges, privileges, length, &length))
	{
		Log(L"Failed to get token privileges", true);
		return false;
	}

	for (DWORD i = 0; i < privileges->PrivilegeCount; i++)
	{
		LUID_AND_ATTRIBUTES& privilege = privileges->Privileges[i];
		if (privilege.Attributes & SE_PRIVILEGE_ENABLED)
		{
			TCHAR privilegeName[256] = { 0 };
			DWORD nameLength = sizeof(privilegeName) / sizeof(TCHAR);
			LookupPrivilegeName(NULL, &privilege.Luid, privilegeName, &nameLength);
			Log(StrFormat(L"Enabled privilege: %s", privilegeName), false);
		}
	}

	return true;
}


HANDLE GetLocalSystemProcessToken();
bool LimitRights(HANDLE& hUser);
bool ElevateUserToken(HANDLE& hEnvUser);

void Duplicate(HANDLE& h, LPCSTR file, int line)
{
	HANDLE hDupe = NULL;
	if(DuplicateTokenEx(h, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &hDupe))
	{
		CloseHandle(h);
		h = hDupe;
		hDupe = NULL;
	}
	else
	{
		DWORD gle = GetLastError();
		_ASSERT(0);
		Log(StrFormat(L"Error duplicating a user token (%S, %d)", file, line), GetLastError());
	}
}

#ifdef _DEBUG
void GrantAllPrivs(HANDLE h)
{
	Log(L"DEBUG: GrantAllPrivs", false);
	CString privs = L"SeCreateTokenPrivilege,SeAssignPrimaryTokenPrivilege,SeLockMemoryPrivilege,SeIncreaseQuotaPrivilege,SeMachineAccountPrivilege,"
					L"SeTcbPrivilege,SeSecurityPrivilege,SeTakeOwnershipPrivilege,SeLoadDriverPrivilege,SeSystemProfilePrivilege,SeSystemtimePrivilege,SeProfileSingleProcessPrivilege,"
					L"SeIncreaseBasePriorityPrivilege,SeCreatePagefilePrivilege,SeCreatePermanentPrivilege,SeBackupPrivilege,SeRestorePrivilege,SeShutdownPrivilege,SeDebugPrivilege,"
					L"SeAuditPrivilege,SeSystemEnvironmentPrivilege,SeChangeNotifyPrivilege,SeRemoteShutdownPrivilege,SeUndockPrivilege,SeSyncAgentPrivilege,SeEnableDelegationPrivilege,"
					L"SeManageVolumePrivilege,SeImpersonatePrivilege,SeCreateGlobalPrivilege,SeTrustedCredManAccessPrivilege,SeRelabelPrivilege,SeIncreaseWorkingSetPrivilege,"
					L"SeTimeZonePrivilege,SeCreateSymbolicLinkPrivilege";

	wchar_t* pC = wcstok(privs.LockBuffer(), L",");
	while(NULL != pC)
	{
		EnablePrivilege(pC, h); //needed to call CreateProcessAsUser
		pC = wcstok(NULL, L",");
	}
}
#endif


void GetUserDomain(LPCWSTR userIn, CString& user, CString& domain)
{
	//run as specified user
	CString tmp = userIn;
	LPCWSTR userStr = NULL, domainStr = NULL;
	if(NULL != wcschr(userIn, L'@'))
		userStr = userIn; //leave domain as NULL
	else
	{
		if(NULL != wcschr(userIn, L'\\'))
		{
			domainStr = wcstok(tmp.LockBuffer(), L"\\");
			userStr = wcstok(NULL, L"\\");
		}
		else
		{
			//no domain given
			userStr = userIn;
			domainStr = L".";
		}
	}
	user = userStr;
	domain = domainStr;
}

bool GetUserHandle(Settings& settings, BOOL& bLoadedProfile, PROFILEINFO& profile, HANDLE hCmdPipe)
{
	Log(L"Entering GetUserHandle function", false);

	HANDLE hCurrentProcessToken = NULL;
	Log(L"Attempting to open current process token", false);
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hCurrentProcessToken))
	{
		DWORD gle = GetLastError();
		Log(StrFormat(L"Failed to open current process token. Error: %d (%s)",
			gle, GetSystemErrorMessage(gle)), true);
		return false;
	}
	Log(StrFormat(L"Successfully opened current process token: %p", hCurrentProcessToken), false);

	HANDLE hDupToken = NULL;
	Log(L"Attempting to duplicate token", false);
	if (!DuplicateTokenEx(hCurrentProcessToken, TOKEN_ALL_ACCESS, NULL,
		SecurityImpersonation, TokenPrimary, &hDupToken))
	{
		DWORD gle = GetLastError();
		Log(StrFormat(L"Failed to duplicate token. Error: %d (%s)",
			gle, GetSystemErrorMessage(gle)), true);
		CloseHandle(hCurrentProcessToken);
		return false;
	}
	Log(StrFormat(L"Successfully duplicated token: %p", hDupToken), false);

	CloseHandle(hCurrentProcessToken);
	Log(L"Closed original process token", false);

	if (!settings.user.IsEmpty())
	{
		Log(L"User specified, attempting to modify token", false);
		CString user, domain;
		GetUserDomain(settings.user, user, domain);
		Log(StrFormat(L"User: %s, Domain: %s", user, domain), false);

		std::wstring decryptedPassword;
		if (!DecryptString(settings.encryptedPassword, decryptedPassword))
		{
			Log(L"Failed to decrypt password", true);
			CloseHandle(hDupToken);
			return false;
		}
		Log(L"Successfully decrypted password", false);

		HANDLE hUserToken = NULL;
		Log(L"Attempting to logon user", false);
		if (!LogonUser(user, domain.IsEmpty() ? NULL : domain, decryptedPassword.c_str(),
			LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hUserToken))
		{
			DWORD gle = GetLastError();
			Log(StrFormat(L"Failed to logon user %s. Error: %d (%s)",
				settings.user, gle, GetSystemErrorMessage(gle)), true);
			CloseHandle(hDupToken);
			return false;
		}
		Log(L"Successfully logged on user", false);

		CloseHandle(hDupToken);
		hDupToken = hUserToken;

		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		if (dwSessionId == 0xFFFFFFFF)
		{
			Log(L"Failed to get active console session ID", true);
			CloseHandle(hDupToken);
			return false;
		}
		Log(StrFormat(L"Active console session ID: %d", dwSessionId), false);

		Log(L"Attempting to set token session ID", false);
		if (!SetTokenInformation(hDupToken, TokenSessionId, &dwSessionId, sizeof(DWORD)))
		{
			DWORD gle = GetLastError();
			Log(StrFormat(L"Failed to set token session ID. Error: %d (%s)",
				gle, GetSystemErrorMessage(gle)), true);
			CloseHandle(hDupToken);
			return false;
		}
		Log(L"Successfully set token session ID", false);
	}

	settings.hUser = hDupToken;
	Log(StrFormat(L"Final user handle value: %p", settings.hUser), false);

	return true;
}


CString GetTokenUserSID(HANDLE hToken)
{
	DWORD tmp = 0;
	CString userName;
	DWORD sidNameSize = 64;
	std::vector<WCHAR> sidName;
	sidName.resize(sidNameSize);

	DWORD sidDomainSize = 64;
	std::vector<WCHAR> sidDomain;
	sidDomain.resize(sidNameSize);

	DWORD userTokenSize = 1024;
	std::vector<WCHAR> tokenUserBuf;
	tokenUserBuf.resize(userTokenSize);

	TOKEN_USER *userToken = (TOKEN_USER*)&tokenUserBuf.front();

	if(GetTokenInformation(hToken, TokenUser, userToken, userTokenSize, &tmp))
	{
		WCHAR *pSidString = NULL;
		if(ConvertSidToStringSid(userToken->User.Sid, &pSidString))
			userName = pSidString;
		if(NULL != pSidString)
			LocalFree(pSidString);
	}
	else
		_ASSERT(0);

	return userName;
}

bool EnsureNecessaryPrivileges(HANDLE hToken)
{
	const LPCTSTR requiredPrivileges[] = {
		SE_ASSIGNPRIMARYTOKEN_NAME,
		SE_INCREASE_QUOTA_NAME,
		SE_TCB_NAME
	};

	for (const auto& priv : requiredPrivileges)
	{
		Log(StrFormat(L"Attempting to enable privilege: %s", priv), false);
		if (!EnablePrivilege(priv, hToken))
		{
			DWORD gle = GetLastError();
			Log(StrFormat(L"Failed to enable privilege: %s. Error: %d (%s)",
				priv, gle, GetSystemErrorMessage(gle)), true);
			return false;
		}
		Log(StrFormat(L"Successfully enabled privilege: %s", priv), false);
	}
	return true;
}

HANDLE GetLocalSystemProcessToken()
{
	DWORD pids[1024*10] = {0}, cbNeeded = 0, cProcesses = 0;

	if ( !EnumProcesses(pids, sizeof(pids), &cbNeeded))
	{
		Log(L"Can't enumProcesses - Failed to get token for Local System.", true);
		return NULL;
	}

	// Calculate how many process identifiers were returned.
	cProcesses = cbNeeded / sizeof(DWORD);
	for(DWORD i = 0; i<cProcesses; ++i)
	{
		DWORD gle = 0;
		DWORD dwPid = pids[i];
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPid);
		if (hProcess)
		{
			HANDLE hToken = 0;
			if (OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_READ | TOKEN_IMPERSONATE | TOKEN_QUERY_SOURCE | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_EXECUTE, &hToken))
			{
				try
				{
					CString name = GetTokenUserSID(hToken);
					

					if(name == L"S-1-5-18") //Well known SID for Local System
					{
						CloseHandle(hProcess);
						return hToken;
					}
				}
				catch(...)
				{
					_ASSERT(0);
				}
			}
			else
				gle = GetLastError();
			CloseHandle(hToken);
		}
		else 
			gle = GetLastError();
		CloseHandle(hProcess);
	}
	Log(L"Failed to get token for Local System.", true);
	return NULL;
}

typedef BOOL (WINAPI *SaferCreateLevelProc)(DWORD dwScopeId, DWORD dwLevelId, DWORD OpenFlags, SAFER_LEVEL_HANDLE* pLevelHandle, LPVOID lpReserved);
typedef BOOL (WINAPI *SaferComputeTokenFromLevelProc)(SAFER_LEVEL_HANDLE LevelHandle, HANDLE InAccessToken, PHANDLE OutAccessToken, DWORD dwFlags, LPVOID lpReserved);
typedef BOOL (WINAPI *SaferCloseLevelProc)(SAFER_LEVEL_HANDLE hLevelHandle);

bool LimitRights(HANDLE& hUser)
{
	DWORD gle = 0;

	static SaferCreateLevelProc gSaferCreateLevel = NULL;
	static SaferComputeTokenFromLevelProc gSaferComputeTokenFromLevel = NULL;
	static SaferCloseLevelProc gSaferCloseLevel = NULL;

	if((NULL == gSaferCloseLevel) || (NULL == gSaferComputeTokenFromLevel) || (NULL == gSaferCreateLevel))
	{
		HMODULE hMod = LoadLibrary(L"advapi32.dll"); //GLOK
		if(NULL != hMod)
		{
			gSaferCreateLevel = (SaferCreateLevelProc)GetProcAddress(hMod, "SaferCreateLevel");
			gSaferComputeTokenFromLevel = (SaferComputeTokenFromLevelProc)GetProcAddress(hMod, "SaferComputeTokenFromLevel");
			gSaferCloseLevel = (SaferCloseLevelProc)GetProcAddress(hMod, "SaferCloseLevel");
		}
	}

	if((NULL == gSaferCloseLevel) || (NULL == gSaferComputeTokenFromLevel) || (NULL == gSaferCreateLevel))
	{
		Log(L"Safer... calls not supported on this OS -- can't limit rights", true);
		return false;
	}

	if(!BAD_HANDLE(hUser))
	{
		HANDLE hNew = NULL;
		SAFER_LEVEL_HANDLE safer = NULL;
		if(FALSE == gSaferCreateLevel(SAFER_SCOPEID_USER, SAFER_LEVELID_NORMALUSER, SAFER_LEVEL_OPEN, &safer, NULL))
		{
			gle = GetLastError();
			Log(L"Failed to limit rights (SaferCreateLevel).", gle);
			return false;
		}

		if(NULL != safer)
		{
			if(FALSE == gSaferComputeTokenFromLevel(safer, hUser, &hNew, 0, NULL))
			{
				gle = GetLastError();
				Log(L"Failed to limit rights (SaferComputeTokenFromLevel).", gle);
				VERIFY(gSaferCloseLevel(safer));
				return false;
			}
			VERIFY(gSaferCloseLevel(safer));
		}


		if(!BAD_HANDLE(hNew))
		{
			VERIFY(CloseHandle(hUser));
			hUser = hNew;
			Duplicate(hUser, __FILE__, __LINE__);
			return true;
		}
	}

	Log(L"Don't have a good user -- can't limit rights", true);
	return false;
}


bool ElevateUserToken(HANDLE& hEnvUser)
{
	TOKEN_ELEVATION_TYPE tet;
	DWORD needed = 0;
	DWORD gle = 0;

	if(GetTokenInformation(hEnvUser, TokenElevationType, (LPVOID)&tet, sizeof(tet), &needed))
	{
		if(tet == TokenElevationTypeLimited)
		{
			//get the associated token, which is the full-admin token
			TOKEN_LINKED_TOKEN tlt = {0};
			if(GetTokenInformation(hEnvUser, TokenLinkedToken, (LPVOID)&tlt, sizeof(tlt), &needed))
			{
				Duplicate(tlt.LinkedToken, __FILE__, __LINE__);
				hEnvUser = tlt.LinkedToken;
				return true;
			}
			else
			{
				gle = GetLastError();
				Log(L"Failed to get elevated token", gle);
				return false;
			}
		}
		else
			return true;
	}
	else
	{

		gle = GetLastError();
		switch(gle)
		{ 
		case ERROR_INVALID_PARAMETER: //expected on 32-bit XP
		case ERROR_INVALID_FUNCTION: //expected on 64-bit XP
			break;
		default:
			Log(L"Can't query token to run elevated - continuing anyway", gle);
			break;
		}

		return true;
	}
}

bool StartProcess(Settings& settings, HANDLE hCmdPipe)
{
	Log(StrFormat(L"User handle value: %p", settings.hUser), false);

	if (BAD_HANDLE(settings.hUser))
	{
		Log(L"Invalid user handle. Cannot start process.", true);
		return false;
	}

	if (!EnsureNecessaryPrivileges(settings.hUser))
	{
		Log(L"Failed to ensure necessary privileges", true);
		return false;
	}

	STARTUPINFO si = { sizeof(STARTUPINFO) };
	PROCESS_INFORMATION pi;

	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = settings.bInteractive ? SW_SHOW : SW_HIDE;

	if (!BAD_HANDLE(settings.hStdOut))
	{
		si.hStdOutput = settings.hStdOut;
		si.hStdError = settings.hStdErr;
		si.hStdInput = settings.hStdIn;
		si.dwFlags |= STARTF_USESTDHANDLES;
	}

	CString commandLine = L"\"" + settings.app + L"\"";
	if (!settings.appArgs.IsEmpty())
	{
		commandLine += L" " + settings.appArgs;
	}

	Log(StrFormat(L"Attempting to start process with command line: %s", commandLine), false);

	LPWSTR lpCommandLine = commandLine.GetBuffer();
	LPWSTR lpCurrentDirectory = settings.workingDir.IsEmpty() ? NULL : settings.workingDir.GetBuffer();

	BOOL result = CreateProcessAsUser(
		settings.hUser,
		NULL,
		lpCommandLine,
		NULL,
		NULL,
		TRUE,
		CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT,
		NULL,
		lpCurrentDirectory,
		&si,
		&pi
	);

	commandLine.ReleaseBuffer();
	if (!settings.workingDir.IsEmpty()) settings.workingDir.ReleaseBuffer();

	if (result)
	{
		settings.hProcess = pi.hProcess;
		settings.processID = pi.dwProcessId;
		CloseHandle(pi.hThread);

		Log(StrFormat(L"Successfully started process. PID: %d", pi.dwProcessId), false);

		if (!settings.bDontWaitForTerminate)
		{
			DWORD waitResult = WaitForSingleObject(pi.hProcess, settings.timeoutSeconds * 1000);
			if (waitResult == WAIT_TIMEOUT)
			{
				Log(L"Process timed out. Terminating.", true);
				TerminateProcess(pi.hProcess, (UINT)-1);
			}
			DWORD exitCode;
			GetExitCodeProcess(pi.hProcess, &exitCode);
			Log(StrFormat(L"Process exited with code: %d", exitCode), false);
		}

		return true;
	}
	else
	{
		DWORD error = GetLastError();
		Log(StrFormat(L"CreateProcessAsUser failed. Error: %d (%s)", error, GetSystemErrorMessage(error)), true);
		return false;
	}
}
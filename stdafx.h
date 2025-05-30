

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_WCSTOK
#define CUR_SERIALIZE_VERSION 1

#include "targetver.h"

#define _CRT_RAND_S
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
#include <atlstr.h>
#include <time.h>

#ifdef _DEBUG
	#define VERIFY(x) { BOOL b = x; _ASSERT(b); }
#else
	#define VERIFY(x) x
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#include <iostream>
#include <WinSvc.h>
#include <vector>
#include <string>
#include <sstream>
#include <atlstr.h>
#include "CmdLineParser.h"
#include "Encryption.h"

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <lm.h>
#include <winnetwk.h>
#include <NTSecAPI.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "mpr.lib")

extern bool gbODS;
extern bool gbStop;
extern bool gbInService;

#define BAD_HANDLE(x) ((INVALID_HANDLE_VALUE == x) || (NULL == x))

const WORD MSGID_SETTINGS			= 1; //local sending settings to remote service, content of this message scrambled
const WORD MSGID_RESP_SEND_FILES	= 2; //remote indicates file needs to be sent
const WORD MSGID_SENT_FILES			= 3; //local is sending file
const WORD MSGID_OK					= 4; //simple OK response
const WORD MSGID_START_APP			= 5; //remote tells local to start app
const WORD MSGID_FAILED				= 6; //request failed

class FileInfo
{
public:
	FileInfo() { bCopyFile = false; fileVersionMS = 0; fileVersionLS = 0; fileLastWrite.dwHighDateTime = 0; fileLastWrite.dwLowDateTime = 0; }
	FileInfo(const FileInfo& in) { *this = in; }
	FileInfo& operator=(const FileInfo& in)
	{
		filenameOnly = in.filenameOnly;
		fullFilePath = in.fullFilePath;
		bCopyFile = in.bCopyFile;
		fileVersionMS = in.fileVersionMS;
		fileVersionLS = in.fileVersionLS;
		fileLastWrite.dwHighDateTime = in.fileLastWrite.dwHighDateTime;
		fileLastWrite.dwLowDateTime = in.fileLastWrite.dwLowDateTime;
		return *this;
	}

	CString filenameOnly;
	CString fullFilePath; //not serialized to remote server
	FILETIME fileLastWrite;
	DWORD fileVersionMS;
	DWORD fileVersionLS;
	bool bCopyFile; //not serialized to remote server
};

class RemMsg
{
	RemMsg(const RemMsg&); //not implemented
	RemMsg& operator=(const RemMsg&); //not implemented

	DWORD GetUniqueID();


	public:

		RemMsg() { m_msgID = 0; m_pBuff = NULL; m_expectedLen = 0; m_uniqueProcessID = GetUniqueID(); }
		RemMsg(WORD msgID)
		{
			m_msgID = msgID;
			m_bResetReadItr = true;
			m_pBuff = NULL;
			m_expectedLen = 0;
			m_uniqueProcessID = GetUniqueID();

		}
		virtual ~RemMsg()
		{
			delete[] m_pBuff;
		}

		const BYTE* GetDataToSend(DWORD& totalLen);
		void SetFromReceivedData(BYTE* pData, DWORD dataLen);

		RemMsg& operator<<(LPCWSTR s);
		RemMsg& operator<<(bool b);
		RemMsg& operator<<(DWORD d);
		RemMsg& operator<<(__int64 d);
		RemMsg& operator<<(FILETIME d);
		RemMsg& operator<<(const std::vector<BYTE>& v)
		{
			*this << (DWORD)v.size();
			m_payload.insert(m_payload.end(), v.begin(), v.end());
			return *this;
		}

		RemMsg& operator>>(CString& s);
		RemMsg& operator>>(bool& b);
		RemMsg& operator>>(DWORD& d);
		RemMsg& operator>>(__int64& d);
		RemMsg& operator>>(FILETIME& d);
		RemMsg& operator>>(std::vector<BYTE>& v)
		{
			if (m_bResetReadItr)
			{
				m_readItr = m_payload.begin();
				m_bResetReadItr = false;
			}

			DWORD size;
			*this >> size;
			v.clear();
			v.insert(v.end(), m_readItr, m_readItr + size);
			m_readItr += size;
			return *this;
		}

		WORD                m_msgID;
		std::vector<BYTE>   m_payload;
		std::vector<BYTE>::iterator m_readItr;
		bool                m_bResetReadItr;
		DWORD               m_expectedLen;
		DWORD               m_uniqueProcessID;

	private:
		BYTE* m_pBuff;
		//DWORD GetUniqueID();
	};



class Settings
{
	Settings(const Settings&); //not implemented
	Settings& operator=(const Settings&); //not implemented
	
public:
	Settings() 
	{ 
		bCopyFiles = false; 
		bForceCopy = false; 
		bCopyIfNewerOrHigherVer = false; 
		bDontWaitForTerminate = false; 
		bDontLoadProfile = false; 
		bRunElevated = false;
		bRunLimited = false;
		remoteCompConnectTimeoutSec = 0;
		bUseSystemAccount = false;
		bShowUIOnWinLogon = false;
		priority = NORMAL_PRIORITY_CLASS;
		hProcess = INVALID_HANDLE_VALUE;
		hUserProfile = INVALID_HANDLE_VALUE; //call UnloadUserProfile when done
		hUser = INVALID_HANDLE_VALUE;
		hUserImpersonated = INVALID_HANDLE_VALUE;
		bDisableFileRedirection = false;
		bODS = false;
		hStdOut = INVALID_HANDLE_VALUE;
		hStdIn = INVALID_HANDLE_VALUE;
		hStdErr = INVALID_HANDLE_VALUE;
		bNeedToDetachFromAdmin = false;
		bNeedToDetachFromIPC = false;
		bNeedToDeleteServiceFile = false;
		bNeedToDeleteService = false;
		bInteractive = false;
		processID = 0;
		bNoDelete = false;
		timeoutSeconds = 0;
		bNoName = false;
		sessionToInteractWith = (DWORD)-1; //not initialized
		targetShare = L"ADMIN$";
		targetSharePath = L"%SYSTEMROOT%";     
	}
		std::vector<BYTE> encryptedPassword;
		void SetPassword(const std::wstring& password) {
			if (!EncryptString(password, encryptedPassword)) {
				// Handle encryption failure
				//Log(L"Failed to encrypt password", true);
				encryptedPassword.clear();
			}
		}

	void Serialize(RemMsg& msg, bool bSave)
	{
		if (bSave)
		{
			msg << (DWORD)CUR_SERIALIZE_VERSION;
			msg << (DWORD)allowedProcessors.size();
			for (std::vector<WORD>::iterator itr = allowedProcessors.begin(); allowedProcessors.end() != itr; itr++)
				msg << (DWORD)*itr;
			msg << bCopyFiles;
			msg << bForceCopy;
			msg << bCopyIfNewerOrHigherVer;
			msg << bDontWaitForTerminate;
			msg << bDontLoadProfile;
			msg << sessionToInteractWith;
			msg << bInteractive;
			msg << bRunElevated;
			msg << bRunLimited;
			msg << encryptedPassword;
			msg << user;
			msg << bUseSystemAccount;
			msg << workingDir;
			msg << bShowUIOnWinLogon;
			msg << (DWORD)priority;
			msg << app;
			msg << appArgs;
			msg << bDisableFileRedirection;
			msg << bODS;
			msg << remoteLogPath;
			msg << bNoDelete;
			msg << srcDir;
			msg << destDir;
			msg << (DWORD)srcFileInfos.size();
			for (std::vector<FileInfo>::iterator fItr = srcFileInfos.begin(); srcFileInfos.end() != fItr; fItr++)
			{
				msg << (*fItr).filenameOnly;
				msg << (*fItr).fileLastWrite;
				msg << (*fItr).fileVersionLS;
				msg << (*fItr).fileVersionMS;
			}
			msg << (DWORD)destFileInfos.size();
			for (std::vector<FileInfo>::iterator fItr = destFileInfos.begin(); destFileInfos.end() != fItr; fItr++)
			{
				msg << (*fItr).filenameOnly;
				msg << (*fItr).fileLastWrite;
				msg << (*fItr).fileVersionLS;
				msg << (*fItr).fileVersionMS;
			}
			msg << timeoutSeconds;
		}
		else
		{
			allowedProcessors.clear();

			DWORD version = 0;
			msg >> version;

			DWORD num = 0;
			msg >> num;
			for (DWORD i = 0; i < num; i++)
			{
				DWORD proc = 0;
				msg >> proc;
				allowedProcessors.push_back((WORD)proc);
			}
			msg >> bCopyFiles;
			msg >> bForceCopy;
			msg >> bCopyIfNewerOrHigherVer;
			msg >> bDontWaitForTerminate;
			msg >> bDontLoadProfile;
			msg >> sessionToInteractWith;
			msg >> bInteractive;
			msg >> bRunElevated;
			msg >> bRunLimited;
			msg >> encryptedPassword;
			msg >> user;
			msg >> bUseSystemAccount;
			msg >> workingDir;
			msg >> bShowUIOnWinLogon;
			msg >> (DWORD&)priority;
			msg >> app;
			msg >> appArgs;
			msg >> bDisableFileRedirection;
			msg >> bODS;
			msg >> remoteLogPath;
			msg >> bNoDelete;
			msg >> srcDir;
			msg >> destDir;

			msg >> num;
			for (DWORD i = 0; i < num; i++)
			{
				FileInfo fi;
				msg >> fi.filenameOnly;
				msg >> fi.fileLastWrite;
				msg >> fi.fileVersionLS;
				msg >> fi.fileVersionMS;
				fi.bCopyFile = false;
				srcFileInfos.push_back(fi);
			}

			msg >> num;
			for (DWORD i = 0; i < num; i++)
			{
				FileInfo fi;
				msg >> fi.filenameOnly;
				msg >> fi.fileLastWrite;
				msg >> fi.fileVersionLS;
				msg >> fi.fileVersionMS;
				fi.bCopyFile = false;
				destFileInfos.push_back(fi);
			}
			msg >> timeoutSeconds;
		}
	}

	bool ResolveFilePaths();

	std::vector<WORD> allowedProcessors; //empty means any
	bool bCopyFiles;
	bool bForceCopy;
	bool bCopyIfNewerOrHigherVer;
	bool bDontWaitForTerminate;
	bool bDontLoadProfile;
	DWORD sessionToInteractWith; 
	bool bInteractive;
	bool bRunElevated;
	bool bRunLimited;
	CString password;
	CString encryptedpassword;
	CString user;
	bool bUseSystemAccount;
	CString workingDir;
	bool bShowUIOnWinLogon;
	int priority;
	CString app;
	CString appArgs;
	bool bDisableFileRedirection;
	bool bODS;
	CString remoteLogPath;
	bool bNoDelete;
	CString srcDir;
	CString destDir;
	std::vector<FileInfo> srcFileInfos; //index 0 will be source of app, but may not match app if -csrc or -clist are being used
	std::vector<FileInfo> destFileInfos; //index 0 will be app
	DWORD timeoutSeconds;

	//NOT TRANSMITTED
	DWORD remoteCompConnectTimeoutSec;
	std::vector<CString> computerList; //run locally if empty
	HANDLE hProcess;
	DWORD processID;
	HANDLE hUserProfile; //call UnloadUserProfile when done
	HANDLE hUser;
	HANDLE hUserImpersonated;
	CString localLogPath;
	HANDLE hStdOut;
	HANDLE hStdIn;
	HANDLE hStdErr;
	bool bNeedToDetachFromAdmin;
	bool bNeedToDetachFromIPC;
	bool bNeedToDeleteServiceFile;
	bool bNeedToDeleteService;
	bool bNoName;
	CString serviceName;
	CString targetShare;
	CString targetSharePath;
};

class ListenParam
{
public:
	ListenParam() { pSettings = NULL; remoteServer = NULL; pid = GetCurrentProcessId(); hStop = CreateEvent(NULL, TRUE, FALSE, NULL); workerThreads = 0; InitializeCriticalSection(&cs); }
	~ListenParam() { CloseHandle(hStop); DeleteCriticalSection(&cs); }
	Settings* pSettings;
	LPCWSTR remoteServer;
	CString machineName;
	DWORD pid;
	HANDLE hStop;
	long workerThreads;
	CRITICAL_SECTION cs;
	std::vector<std::string> inputSentToSuppressInOutput;
};

const unsigned char UTF8_BOM[] = { unsigned char(0xEF), unsigned char(0xBB), unsigned char(0xBF) };


typedef struct 
{
	DWORD origSessionID;
	HANDLE hUser;
	bool bPreped;
}CleanupInteractive;


DWORD StartLocalService(CCmdLineParser& cmdParser);
void Log(LPCWSTR str, bool bForceODS);
void Log(LPCWSTR str, DWORD lastError);
CString GetSystemErrorMessage(DWORD lastErrorVal);
CString StrFormat(LPCTSTR pFormat, ...);
void PrintCopyright();
void PrintUsage();
bool GetComputerList(Settings& settings, LPCWSTR& cmdLine);
DWORD wtodw(const wchar_t* num);
bool StartProcess(Settings& settings, HANDLE remoteCmdPipe);
bool EnablePrivilege(LPCWSTR privilegeStr, HANDLE hToken = NULL);
BOOL PrepForInteractiveProcess(Settings& settings, CleanupInteractive* pCI, DWORD sessionID); 
void CleanUpInteractiveProcess(CleanupInteractive* pCI); 
bool EstablishConnection(Settings& settings, LPCTSTR lpszRemote, LPCTSTR lpszResource, bool bConnect);
bool CopyLiteRunToRemote(Settings& settings, LPCWSTR targetComputer);
bool InstallAndStartRemoteService(LPCWSTR remoteServer, Settings& settings);
bool SendSettings(LPCWSTR remoteServer, Settings& settings, HANDLE& hPipe, bool& bNeedToSendFile);
bool SendRequest(LPCWSTR remoteServer, HANDLE& hPipe, RemMsg& msgOut, RemMsg& msgReturned, Settings& settings);
bool SendFilesToRemote(LPCWSTR remoteServer, Settings& settings, HANDLE& hPipe);
void StopAndDeleteRemoteService(LPCWSTR remoteServer, Settings& settings);
void DeleteLiteRunFromRemote(LPCWSTR targetComputer, Settings& settings);
void HandleMsg(RemMsg& msg, RemMsg& response, HANDLE hPipe);
bool GetTargetFileInfo(Settings& settings);
bool ParseCommandLine(Settings& settings, LPCWSTR cmdLine);
void StartRemoteApp(LPCWSTR remoteServer, Settings& settings, HANDLE& hPipe, int& appExitCode);
void DisableFileRedirection();
void RevertFileRedirection();
bool CreateIOPipesInService(Settings& settings, LPCWSTR caller, DWORD pid);
BOOL ConnectToRemotePipes(ListenParam* pLP, DWORD dwRetryCount, DWORD dwRetryTimeOut);
CString LastLog();
void Duplicate(HANDLE& h, LPCSTR file, int line);
bool ReadTextFile(LPCWSTR fileName, CString& content);
CString ExpandToFullPath(LPCWSTR path);
BOOL IsLocalSystem();



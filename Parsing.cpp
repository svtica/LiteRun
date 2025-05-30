
#include "stdafx.h"
#include "Parsing.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


bool GetComputerList(Settings& settings, LPCWSTR& cmdLine)
{
	DWORD gle = 0;

	// [\\computer[,computer2[,...] | @file]]
	//@file LiteRun will execute the command on each of the computers listed	in the file.
	//a computer name of "\\*" LiteRun runs the applications on all computers in the current domain.

	//first part of command line is the executable itself, so skip past that
	if(L'"' == *cmdLine)
	{
		//path is quoted, so skip to end quote
		cmdLine = wcschr(cmdLine + 1, L'"');
		if(NULL != cmdLine)
			cmdLine++;
	}
	else
	{
		//no quotes, skip forward to whitespace
		while(!iswspace(*cmdLine) && *cmdLine)
			cmdLine++;
	}

	//now skip past white space
	while(iswspace(*cmdLine) && *cmdLine)
		cmdLine++;

	//if we see -accepteula or /accepteula, skip over it
	if( (0 == _wcsnicmp(cmdLine, L"/accepteula", 11)) || (0 == _wcsnicmp(cmdLine, L"-accepteula", 11)) )
	{
		cmdLine += 11; //get past Eula
		//now skip past white space
		while(iswspace(*cmdLine) && *cmdLine)
			cmdLine++;
	}

	if(0 == wcsncmp(cmdLine, L"\\\\*", 3))
	{
		cmdLine += 3; 
		//get server list from domain
		SERVER_INFO_100* pInfo = NULL;
		DWORD numServers = 0, total = 0;
		DWORD ignored = 0;
		NET_API_STATUS stat = NetServerEnum(NULL, 100, (LPBYTE*)&pInfo, MAX_PREFERRED_LENGTH, &numServers, &total, SV_TYPE_SERVER | SV_TYPE_WINDOWS, NULL, &ignored);
		if(NERR_Success == stat)
		{
			for(DWORD i = 0; i < numServers; i++)
				settings.computerList.push_back(pInfo[i].sv100_name);
		}
		else
			Log(L"Got error from NetServerEnum: ", (DWORD)stat);
		NetApiBufferFree(pInfo);
		pInfo = NULL;
		if(settings.computerList.empty())
			Log(L"No computers could be found", true);
		return !settings.computerList.empty();
	}
	else
	{
		if(L'@' == *cmdLine)
		{
			//read server list from file.  Assumes UTF8
			LPCWSTR fileStart = cmdLine + 1;
			while(!iswspace(*cmdLine) && *cmdLine)
				cmdLine++;
			CString file = CString(fileStart).Left(cmdLine - fileStart);
			
			file = ExpandToFullPath(file); //search on path if no path specified

			CString content;
			if(false == ReadTextFile(file, content))
				return false;

			wchar_t* pC = wcstok(content.LockBuffer(), L"\r\n");
			while(NULL != pC)
			{
				CString s = pC;
				s.Trim();
				if(FALSE == s.IsEmpty())
					settings.computerList.push_back(s);
				pC = wcstok(NULL, L"\r\n");
			}
			if(settings.computerList.empty())
				Log(L"Computer list file empty", true);
			return !settings.computerList.empty();
		}
		else
		{
			if (0 == wcsncmp(cmdLine, L"\\\\", 2))
			{
				// Get, possibly comma-delimited, computer list
				LPCWSTR compListStart = cmdLine + 2;
				LPCWSTR compListEnd = wcschr(compListStart, L' ');
				if (compListEnd == NULL)
				{
					// No space found, assume the entire remaining command line is the computer list
					compListEnd = compListStart + wcslen(compListStart);
				}

				CString compList(compListStart, compListEnd - compListStart);
				cmdLine = compListEnd;

				// Split the computer list and add to settings
				wchar_t* pC = wcstok(compList.LockBuffer(), L",");
				while (NULL != pC)
				{
					CString s = pC;
					s.Trim();
					if (!s.IsEmpty())
						settings.computerList.push_back(s);
					pC = wcstok(NULL, L",");
				}
				compList.UnlockBuffer();

				if (settings.computerList.empty())
				{
					Log(L"No computers specified", true);
					return false;
				}

				return true;
			}

			// No computer list specified
			return true;
		}
	}
	}

LPCWSTR SkipForward(LPCWSTR cmdLinePastCompList, LPCWSTR arg, bool bCanHaveArg)
{
	LPCWSTR pStart = cmdLinePastCompList;
Top:
	LPCWSTR pC = wcsstr(pStart, arg);
	if(NULL != pC)
	{
		pC += wcslen(arg);
		if(false == iswspace(*pC))
		{
			//we're in a larger delimiter, so skip past this
			pStart = pC + 1;
			goto Top;
		}			

		while(iswspace(*pC) && *pC)
			pC++;
		if(bCanHaveArg)
			if(*pC != L'-')
			{
				bool bInQuote = false;
				while((!iswspace(*pC) || (*pC == L'"') || bInQuote) && *pC)
				{
					if(*pC == L'"')
						bInQuote = !bInQuote;
					pC++;
				}
				//to end of arg now
				while(iswspace(*pC) && *pC)
					pC++;
			}
			if(pC > cmdLinePastCompList)
				cmdLinePastCompList = pC;
	}
	return cmdLinePastCompList;
}

typedef struct 
{
	LPCWSTR cmd;
	bool	bCanHaveArgs;
	bool	bMustHaveArgs;
}CommandList;

CommandList gSupportedCommands[] = 
{
	{L"u", true, true},
	{L"p", true, false},
	{L"p@", true, true},
	{L"p@d", false, false},
	{L"n", true, true},
	{L"l", false, false},
	{L"h", false, false},
	{L"s", false, false},
	{L"e", false, false},
	{L"x", false, false},
	{L"i", true, false},
	{L"c", false, false},
	{L"cnodel", false, false},
	{L"f", false, false},
	{L"v", false, false},
	{L"w", true, true},
	{L"d", false, false},
	{L"low", false, false},
	{L"belownormal", false, false},
	{L"abovenormal", false, false},
	{L"high", false, false},
	{L"realtime", false, false},
	{L"background", false, false},
	{L"a", true, true},
	{L"csrc", true, true},
	{L"clist", true, true},
	{L"dfr", false, false},
	{L"lo", true, true},
	{L"rlo", true, true},
	{L"dbg", false, false},
	{L"to", true, true},
	{L"noname", false, false},
	{L"sname", true, true},
	{L"share", true, true},
	{L"sharepath", true, true},
	{L"accepteula", false, false}
};

LPCWSTR EatWhiteSpace(LPCWSTR ptr)
{
	while(iswspace(*ptr) && *ptr)
		ptr++;

	return ptr;
}

bool SplitCommand(CString& restOfLine, LPCWSTR& LiteRunParams, LPCWSTR& appToRun)
{
	LPWSTR ptr = (LPWSTR)(LPCWSTR)restOfLine;
	while(true)
	{
		if((L'-' == *ptr) || (L'/' == *ptr))
		{
			if(NULL == LiteRunParams)
				LiteRunParams = ptr;

			ptr++;
			LPCWSTR startOfCmd = ptr;
			//skip to end of alpha chars which will signal the end of this command
			while( (iswalpha(*ptr) || (L'@' == *ptr)) && *ptr)
				ptr++;
			if(L'\0' == *ptr)
			{
				Log(L"Reached end of command before seeing expected parts", true);
				return false;
			}

			//ptr is beyond end of cmd
			size_t len = ptr - startOfCmd;

			bool bRecognized = false;
			//now see if this is a recognized command
			int i = 0;
			for(i = 0; i < sizeof(gSupportedCommands)/sizeof(gSupportedCommands[0]); i++)
			{
				if(wcslen(gSupportedCommands[i].cmd) != len)
					continue;
				CString lwrCmd = startOfCmd;
				lwrCmd.MakeLower();
				if(0 == wcsncmp(lwrCmd, gSupportedCommands[i].cmd, len))
				{
					bRecognized = true;
					break;
				}
			}

			if(false == bRecognized)
			{
				*ptr = L'\0';
				Log(StrFormat(L"%s is not a recognized option", startOfCmd), true);
				_ASSERT(0);
				return false;
			}				

			ptr = (LPWSTR)EatWhiteSpace(ptr);
			if(L'\0' == *ptr)
			{
				Log(L"Reached end of command before seeing expected parts", true);
				_ASSERT(0);
				return false;
			}

			if(gSupportedCommands[i].bCanHaveArgs)
			{
				if( (L'-' != *ptr) && (L'/' != *ptr))
				{
					//special handling for -i which may or may not have an argument, but if it does, it is numeric
					if(0 == wcscmp(gSupportedCommands[i].cmd, L"i"))
					{
						//wtodw(' ') and wtodw('0') both return 0
						if(L'0' != *ptr)
						{
							if(0 == wtodw(ptr))
								continue; //no argument
						}
					}
					
					bool bInQuote = false;
					while((!iswspace(*ptr) || (*ptr == L'"') || bInQuote) && *ptr)
					{
						if(*ptr == L'"')
							bInQuote = !bInQuote;
						ptr++;
					}
					//to end of arg now
					ptr = (LPWSTR)EatWhiteSpace(ptr);
					if((L'\0' == *ptr) && (gSupportedCommands[i].bMustHaveArgs))
					{
						Log(L"Reached end of command before seeing expected parts", true);
						_ASSERT(0);
						return false;
					}
				}
			}
		}
		else
		{
			//must have found the start of the app
			ptr[-1] = L'\0'; //terminate LiteRun part
			appToRun = ptr;
			return true;			
		}
	}
}

LPCWSTR SkipExecutableName(LPCWSTR cmdLine)
{
	if (L'"' == *cmdLine)
	{
		cmdLine = wcschr(cmdLine + 1, L'"');
		if (cmdLine)
			cmdLine++;
	}
	else
	{
		while (*cmdLine && !iswspace(*cmdLine))
			cmdLine++;
	}

	while (iswspace(*cmdLine))
		cmdLine++;

	return cmdLine;
}

bool ParseLiteRunOptions(CCmdLineParser& cmdParser, Settings& settings)
{
	// User and password
	if (cmdParser.HasKey(L"u"))
	{
		settings.user = cmdParser.GetVal(L"u");
		if (settings.user.IsEmpty())
		{
			Log(L"-u without user", true);
			return false;
		}
	}

	if (cmdParser.HasKey(L"p"))
	{
		CString tempPassword = cmdParser.GetVal(L"p");
		settings.SetPassword(std::wstring(tempPassword.GetString()));
		{
			Log(L"Failed to encrypt password", true);
			return false;
		}
	}

	// Timeout
	if (cmdParser.HasKey(L"to"))
	{
		settings.timeoutSeconds = wtodw(cmdParser.GetVal(L"to"));
		if (settings.timeoutSeconds == 0)
		{
			Log(L"Invalid or missing value for -to", true);
			return false;
		}
	}

	// Log file
	if (cmdParser.HasKey(L"lo"))
	{
		settings.localLogPath = cmdParser.GetVal(L"lo");
		if (settings.localLogPath.IsEmpty())
		{
			Log(L"-lo missing value", true);
			return false;
		}
	}

	// Remote log file
	if (cmdParser.HasKey(L"rlo"))
	{
		settings.remoteLogPath = cmdParser.GetVal(L"rlo");
		if (settings.remoteLogPath.IsEmpty())
		{
			Log(L"-rlo missing value", true);
			return false;
		}
	}

	// Copy options
	if (cmdParser.HasKey(L"c"))
	{
		settings.bCopyFiles = true;
		if (cmdParser.HasKey(L"f"))
			settings.bForceCopy = true;
		else if (cmdParser.HasKey(L"v"))
			settings.bCopyIfNewerOrHigherVer = true;
	}

	// Don't wait for termination
	if (cmdParser.HasKey(L"d"))
		settings.bDontWaitForTerminate = true;

	// Interactive mode
	if (cmdParser.HasKey(L"i"))
	{
		settings.bInteractive = true;
		settings.sessionToInteractWith = (DWORD)-1; // default, meaning interactive session
		if (cmdParser.HasVal(L"i"))
			settings.sessionToInteractWith = wtodw(cmdParser.GetVal(L"i"));
	}

	// Run elevated
	if (cmdParser.HasKey(L"h"))
	{
		if (settings.bUseSystemAccount || settings.bRunLimited)
		{
			Log(L"Can't use -h and -l together", true);
			return false;
		}
		settings.bRunElevated = true;
	}

	// Run limited
	if (cmdParser.HasKey(L"l"))
	{
		if (settings.bUseSystemAccount || settings.bRunElevated)
		{
			Log(L"Can't use -h, -s, or -l together", true);
			return false;
		}
		settings.bRunLimited = true;
	}

	// Use system account
	if (cmdParser.HasKey(L"s"))
	{
		if (settings.bRunLimited)
		{
			Log(L"Can't use -s and -l together", true);
			return false;
		}
		settings.bUseSystemAccount = true;
	}

	// Don't load profile
	if (cmdParser.HasKey(L"e"))
		settings.bDontLoadProfile = true;

	// Working directory
	if (cmdParser.HasKey(L"w"))
	{
		settings.workingDir = cmdParser.GetVal(L"w");
		if (settings.workingDir.IsEmpty())
		{
			Log(L"-w without value", true);
			return false;
		}
	}

	// Show UI on WinLogon
	if (cmdParser.HasKey(L"x"))
	{
		if (!settings.bUseSystemAccount)
		{
			Log(L"Specified -x without -s", true);
			return false;
		}
		settings.bShowUIOnWinLogon = true;
	}

	// Priority
	if (cmdParser.HasKey(L"low"))
		settings.priority = IDLE_PRIORITY_CLASS;
	if (cmdParser.HasKey(L"belownormal"))
		settings.priority = BELOW_NORMAL_PRIORITY_CLASS;
	if (cmdParser.HasKey(L"abovenormal"))
		settings.priority = ABOVE_NORMAL_PRIORITY_CLASS;
	if (cmdParser.HasKey(L"high"))
		settings.priority = HIGH_PRIORITY_CLASS;
	if (cmdParser.HasKey(L"realtime"))
		settings.priority = REALTIME_PRIORITY_CLASS;

	// Disable file redirection
	if (cmdParser.HasKey(L"dfr"))
		settings.bDisableFileRedirection = true;

	// No name
	if (cmdParser.HasKey(L"noname"))
		settings.bNoName = true;

	// Service name
	if (cmdParser.HasKey(L"sname"))
		settings.serviceName = cmdParser.GetVal(L"sname");

	// Share and share path
	if (cmdParser.HasKey(L"share"))
	{
		settings.targetShare = cmdParser.GetVal(L"share");
		if (cmdParser.HasKey(L"sharepath"))
			settings.targetSharePath = cmdParser.GetVal(L"sharepath");
	}

	// Debug mode
	if (cmdParser.HasKey(L"dbg"))
		settings.bODS = true;

	return true;
}

bool ParseApplicationAndArgs(const CCmdLineParser& cmdParser, Settings& settings)
{
	CString remaining = GetRemainingArgs(cmdParser);

	if (remaining.IsEmpty())
	{
		Log(L"No application specified", true);
		return false;
	}

	// Extract the application path
	if (remaining[0] == L'"')
	{
		int endQuote = remaining.Find(L'"', 1);
		if (endQuote == -1)
		{
			Log(L"Mismatched quotes in application path", true);
			return false;
		}
		settings.app = remaining.Mid(1, endQuote - 1);
		remaining = remaining.Mid(endQuote + 1).TrimLeft();
	}
	else
	{
		int spacePos = remaining.Find(L' ');
		if (spacePos == -1)
		{
			settings.app = remaining;
			remaining.Empty();
		}
		else
		{
			settings.app = remaining.Left(spacePos);
			remaining = remaining.Mid(spacePos + 1).TrimLeft();
		}
	}

	// Extract the arguments
	settings.appArgs.Empty();
	while (!remaining.IsEmpty())
	{
		if (remaining[0] == L'"')
		{
			int endQuote = remaining.Find(L'"', 1);
			if (endQuote == -1)
			{
				// Treat the rest as a single argument if the closing quote is missing
				settings.appArgs += L" " + remaining;
				remaining.Empty();
			}
			else
			{
				settings.appArgs += L" " + remaining.Mid(1, endQuote - 1);
				remaining = remaining.Mid(endQuote + 1).TrimLeft();
			}
		}
		else
		{
			int spacePos = remaining.Find(L' ');
			if (spacePos == -1)
			{
				settings.appArgs += L" " + remaining;
				remaining.Empty();
			}
			else
			{
				settings.appArgs += L" " + remaining.Left(spacePos);
				remaining = remaining.Mid(spacePos + 1).TrimLeft();
			}
		}
	}

	settings.appArgs = settings.appArgs.TrimLeft();

	Log(StrFormat(L"Application: %s", settings.app), false);
	Log(StrFormat(L"Arguments: %s", settings.appArgs), false);

	return true;
}

bool ParseCommandLine(Settings& settings, LPCWSTR cmdLine)
{
	CCmdLineParser cmdParser(cmdLine);

	settings.bCopyFiles = cmdParser.HasKey(L"c");
	settings.bForceCopy = cmdParser.HasKey(L"f");
	settings.bCopyIfNewerOrHigherVer = cmdParser.HasKey(L"v");
	settings.bDontWaitForTerminate = cmdParser.HasKey(L"d");
	settings.bDontLoadProfile = cmdParser.HasKey(L"noprofile");
	settings.bInteractive = cmdParser.HasKey(L"i");
	settings.bRunElevated = cmdParser.HasKey(L"e");
	settings.bRunLimited = cmdParser.HasKey(L"l");
	settings.bUseSystemAccount = cmdParser.HasKey(L"s");
	settings.bShowUIOnWinLogon = cmdParser.HasKey(L"logonui");
	settings.bDisableFileRedirection = cmdParser.HasKey(L"noredirect");
	settings.bNoDelete = cmdParser.HasKey(L"nodelete");
	settings.bNoName = cmdParser.HasKey(L"noname");

	if (cmdParser.HasKey(L"w"))
		settings.workingDir = cmdParser.GetVal(L"w");

	if (cmdParser.HasKey(L"session"))
		settings.sessionToInteractWith = wtodw(cmdParser.GetVal(L"session"));

	if (cmdParser.HasKey(L"remoteto"))
		settings.remoteCompConnectTimeoutSec = wtodw(cmdParser.GetVal(L"remoteto"));

	if (cmdParser.HasKey(L"csrc"))
		settings.srcDir = cmdParser.GetVal(L"csrc");

	if (cmdParser.HasKey(L"cdest"))
		settings.destDir = cmdParser.GetVal(L"cdest");

	if (cmdParser.HasKey(L"clist"))
	{
		CString listFile = cmdParser.GetVal(L"clist");
		CString content;
		if (ReadTextFile(listFile, content))
		{
			// Parse content and populate settings.srcFileInfos and settings.destFileInfos
			// This part depends on the format of your list file
		}
		else
		{
			Log(StrFormat(L"Failed to read copy list file %s", listFile), true);
			return false;
		}
	}

	if (cmdParser.HasKey(L"ro"))
		settings.remoteLogPath = cmdParser.GetVal(L"ro");

	if (cmdParser.HasKey(L"svcname"))
		settings.serviceName = cmdParser.GetVal(L"svcname");

	if (cmdParser.HasKey(L"share"))
		settings.targetShare = cmdParser.GetVal(L"share");

	if (cmdParser.HasKey(L"sharepath"))
		settings.targetSharePath = cmdParser.GetVal(L"sharepath");

	// Get the remaining arguments
	CString remainingArgs = GetRemainingArgs(cmdParser);
	Log(StrFormat(L"Remaining args: %s", remainingArgs), false);

	// Split the remaining args into tokens
	std::vector<CString> tokens;
	int curPos = 0;
	CString token = remainingArgs.Tokenize(L" ", curPos);
	while (!token.IsEmpty())
	{
		if (token[0] == L'"' && token[token.GetLength() - 1] != L'"')
		{
			// Start of a quoted string, continue until we find the end quote
			CString quotedToken = token;
			while (!token.IsEmpty() && token[token.GetLength() - 1] != L'"')
			{
				token = remainingArgs.Tokenize(L" ", curPos);
				quotedToken += L" " + token;
			}
			tokens.push_back(quotedToken);
		}
		else
		{
			tokens.push_back(token);
		}
		token = remainingArgs.Tokenize(L" ", curPos);
	}

	// Process tokens
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		if (tokens[i] == L"-u" && i + 1 < tokens.size())
		{
			settings.user = tokens[++i];
		}
		else if (tokens[i] == L"-p" && i + 1 < tokens.size())
		{
			settings.SetPassword(std::wstring(tokens[++i].GetString()));
		}
		else if (tokens[i] == L"-to" && i + 1 < tokens.size())
		{
			settings.timeoutSeconds = wtodw(tokens[++i]);
		}
		else if (tokens[i] == L"-lo" && i + 1 < tokens.size())
		{
			settings.localLogPath = tokens[++i];
		}
		else if (tokens[i][0] == L'"' || PathFileExists(tokens[i]))
		{
			// This should be the application path
			settings.app = tokens[i];
			settings.app.Trim(L"\"");  // Remove quotes if present
			// Any remaining tokens are app arguments
			if (i + 1 < tokens.size())
			{
				settings.appArgs = CString(remainingArgs.Mid(remainingArgs.Find(tokens[i + 1])));
			}
			break;
		}
	}

	Log(StrFormat(L"Application set: %s", settings.app), false);
	Log(StrFormat(L"Application args: %s", settings.appArgs), false);

	if (settings.app.IsEmpty())
	{
		Log(L"No application specified", true);
		return false;
	}

	// Validate the application path
	if (!PathFileExists(settings.app))
	{
		Log(StrFormat(L"Application path does not exist: %s", settings.app), true);
		return false;
	}

	// Additional validation
	if (settings.bUseSystemAccount && !settings.user.IsEmpty())
	{
		Log(L"Cannot specify both -s and -u", true);
		return false;
	}

	if (settings.bRunElevated && settings.bRunLimited)
	{
		Log(L"Cannot specify both -e and -l", true);
		return false;
	}

	// If copying files, ensure we have necessary information
	if (settings.bCopyFiles)
	{
		if (settings.srcFileInfos.empty())
		{
			FileInfo fi;
			fi.filenameOnly = PathFindFileName(settings.app);
			fi.fullFilePath = settings.app;
			settings.srcFileInfos.push_back(fi);
		}

		if (settings.destFileInfos.empty())
		{
			FileInfo fi;
			fi.filenameOnly = PathFindFileName(settings.app);
			settings.destFileInfos.push_back(fi);
		}

		if (!settings.ResolveFilePaths())
		{
			Log(L"Failed to resolve file paths for copying", true);
			return false;
		}
	}

	return true;
}

CString GetRemainingArgs(const CCmdLineParser& cmdParser)
{
	CString fullCmdLine = cmdParser.getCmdLine();
	CString result;

	// Find the last known option
	CString lastKnownOption;
	CCmdLineParser::POSITION pos = cmdParser.getFirst();
	CCmdLineParser::POSITION lastPos = pos;
	CString key, value;
	while (!cmdParser.isLast(pos))
	{
		lastPos = pos;
		cmdParser.getNext(pos, key, value);
		lastKnownOption = L"-" + key;
	}

	// Find the position of the last known option in the full command line
	int lastOptionPos = fullCmdLine.Find(lastKnownOption);
	if (lastOptionPos != -1)
	{
		// Find the end of this option
		int endOfLastOption = fullCmdLine.Find(L' ', lastOptionPos + lastKnownOption.GetLength());
		if (endOfLastOption != -1)
		{
			// Everything after this is our remaining args
			result = fullCmdLine.Mid(endOfLastOption + 1);
		}
	}
	else
	{
		// If we couldn't find the last option, return everything after the executable name
		int firstSpace = fullCmdLine.Find(L' ');
		if (firstSpace != -1)
		{
			result = fullCmdLine.Mid(firstSpace + 1);
		}
	}

	// Trim leading and trailing spaces
	result.Trim();

	return result;
}
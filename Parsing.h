#pragma once
#include "stdafx.h"
#include "LiteRun.h"
#include "CmdLineParser.h"
#include "Shlwapi.h"
#include <conio.h>
#include <vector>
#include <lm.h>
#include <UserEnv.h>
#include <WtsApi32.h>
#include "Encryption.h"

CString GetRemainingArgs(const CCmdLineParser& cmdParser);

#pragma comment(lib, "Shlwapi.lib")
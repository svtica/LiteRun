#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <vector>
#include <string>
#include <sstream>

#pragma comment(lib, "Crypt32.lib")

#ifdef __cplusplus
extern "C" {
#endif

	bool EncryptString(const std::wstring& input, std::vector<BYTE>& encrypted);
	bool DecryptString(const std::vector<BYTE>& encrypted, std::wstring& decrypted);

#ifdef __cplusplus
}
#endif
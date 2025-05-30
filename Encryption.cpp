#include "stdafx.h"
#include "Encryption.h"

bool EncryptString(const std::wstring& input, std::vector<BYTE>& encrypted)
{
    DATA_BLOB DataIn, DataOut;
    DataIn.pbData = (BYTE*)input.c_str();
    DataIn.cbData = (input.length() + 1) * sizeof(wchar_t);

    if (!CryptProtectData(&DataIn, NULL, NULL, NULL, NULL, 0, &DataOut))
    {
        return false;
    }

    encrypted.assign(DataOut.pbData, DataOut.pbData + DataOut.cbData);
    LocalFree(DataOut.pbData);
    return true;
}

bool DecryptString(const std::vector<BYTE>& encrypted, std::wstring& decrypted)
{
    DATA_BLOB DataIn, DataOut;
    DataIn.pbData = (BYTE*)encrypted.data();
    DataIn.cbData = encrypted.size();

    if (!CryptUnprotectData(&DataIn, NULL, NULL, NULL, NULL, 0, &DataOut))
    {
        return false;
    }

    decrypted = std::wstring((wchar_t*)DataOut.pbData, DataOut.cbData / sizeof(wchar_t) - 1);
    LocalFree(DataOut.pbData);
    return true;
}
#include "logger.h"

#include <Windows.h>
#include <cstdio>
#include <ctime>

namespace {
    constexpr wchar_t kLogFileName[] = L"logs.txt";
    constexpr LONGLONG kMaxLogSizeBytes = 1LL * 1024 * 1024; // 1 MB

    // Builds the full path to logs.txt in the same directory as the running exe
    std::wstring getLogFilePath() {
        wchar_t exePath[MAX_PATH] = { 0 };
        DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (len == 0 || len == MAX_PATH) {
            return kLogFileName; // fall back to relative path
        }

        std::wstring path(exePath);
        size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash == std::wstring::npos) {
            return kLogFileName;
        }

        return path.substr(0, lastSlash + 1) + kLogFileName;
    }
}

void writeConsoleOutputToLog(const std::string& message) {
    const std::wstring logFilePath = getLogFilePath();

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(logFilePath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        LARGE_INTEGER fileSize;
        fileSize.HighPart = fileInfo.nFileSizeHigh;
        fileSize.LowPart = fileInfo.nFileSizeLow;
        if (fileSize.QuadPart >= kMaxLogSizeBytes) {
            DeleteFileW(logFilePath.c_str());
        }
    }

    FILE* logFile = nullptr;
    if (_wfopen_s(&logFile, logFilePath.c_str(), L"a") != 0 || logFile == nullptr) {
        return;
    }

    std::time_t now = std::time(nullptr);
    struct tm localTime;
    char timeBuf[32] = { 0 };
    if (localtime_s(&localTime, &now) == 0) {
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &localTime);
    }

    fprintf(logFile, "[%s] %s", timeBuf, message.c_str());
    fclose(logFile);
}

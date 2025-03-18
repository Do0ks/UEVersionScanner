#include "pch.h"
#include <Windows.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <Psapi.h>
#include <tchar.h>
#include <algorithm>
#include <stdio.h>
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Version.lib")

std::string ReadEntireFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileExists(const std::string& filePath) {
    DWORD attrib = GetFileAttributesA(filePath.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::string GetVersionFromResource() {
    char modulePath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameA(NULL, modulePath, MAX_PATH))
        return "";
    DWORD dummy;
    DWORD size = GetFileVersionInfoSizeA(modulePath, &dummy);
    if (size == 0)
        return "";
    std::vector<char> data(size);
    if (!GetFileVersionInfoA(modulePath, 0, size, data.data()))
        return "";
    VS_FIXEDFILEINFO* fileInfo = nullptr;
    UINT len = 0;
    if (VerQueryValueA(data.data(), "\\", (LPVOID*)&fileInfo, &len) && fileInfo) {
        int major = HIWORD(fileInfo->dwFileVersionMS);
        int minor = LOWORD(fileInfo->dwFileVersionMS);
        int build = HIWORD(fileInfo->dwFileVersionLS);
        int revision = LOWORD(fileInfo->dwFileVersionLS);
        char versionStr[128];
        sprintf_s(versionStr, "%d.%d.%d.%d", major, minor, build, revision);
        return versionStr;
    }
    return "";
}

std::string GetVersionFromFiles() {
    char modulePath[MAX_PATH] = { 0 };
    if (!GetModuleFileNameA(NULL, modulePath, MAX_PATH))
        return "";
    std::string exePath(modulePath);
    size_t lastSlash = exePath.find_last_of("\\/");
    std::string exeDir;
    if (lastSlash != std::string::npos)
        exeDir = exePath.substr(0, lastSlash);
    std::vector<std::string> candidates;
    candidates.push_back(exeDir + "\\Engine\\Build\\Build.version");
    candidates.push_back(exeDir + "\\UE4Version.txt");
    candidates.push_back(exeDir + "\\UE5Version.txt");
    size_t parentSlash = exeDir.find_last_of("\\/");
    if (parentSlash != std::string::npos) {
        std::string parentDir = exeDir.substr(0, parentSlash);
        candidates.push_back(parentDir + "\\Engine\\Build\\Build.version");
        candidates.push_back(parentDir + "\\UE4Version.txt");
        candidates.push_back(parentDir + "\\UE5Version.txt");
    }
    for (auto& path : candidates) {
        if (FileExists(path)) {
            std::string content = ReadEntireFile(path);
            if (!content.empty()) {
                content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
                content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());
                return content;
            }
        }
    }
    return "";
}

std::string GetVersionFromMemoryScan() {
    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule)
        return "";
    MODULEINFO modInfo = { 0 };
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo)))
        return "";
    char* baseAddr = reinterpret_cast<char*>(modInfo.lpBaseOfDll);
    size_t moduleSize = modInfo.SizeOfImage;
    if (!baseAddr || moduleSize == 0)
        return "";
    std::vector<std::string> markers = { "Unreal Engine 4.", "Unreal Engine 5.", "FEngineVersion", "EngineVersion" };
    for (const auto& marker : markers) {
        size_t markerLen = marker.length();
        for (size_t i = 0; i < moduleSize - markerLen; i++) {
            if (memcmp(baseAddr + i, marker.c_str(), markerLen) == 0) {
                std::string found(marker);
                size_t maxExtra = 32;
                size_t j = i + markerLen;
                while (j < moduleSize && (j - (i + markerLen)) < maxExtra && isprint(baseAddr[j])) {
                    found.push_back(baseAddr[j]);
                    j++;
                }
                return found;
            }
        }
    }
    return "";
}

std::string GetUnrealEngineVersion() {
    std::string result;
    result = GetVersionFromResource();
    if (!result.empty())
        return result;
    result = GetVersionFromFiles();
    if (!result.empty())
        return result;
    result = GetVersionFromMemoryScan();
    if (!result.empty())
        return result;
    return "Unknown";
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        std::string versionInfo = GetUnrealEngineVersion();
        std::string msg = "Unreal Engine Version is: " + versionInfo;
        MessageBoxA(nullptr, msg.c_str(), "Unreal Engine Version", MB_OK | MB_ICONINFORMATION);
    }
    return TRUE;
}
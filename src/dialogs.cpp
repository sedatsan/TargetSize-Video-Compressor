#include "dialogs.hpp"
#include <shobjidl.h>
#include <windows.h>
#include <shellapi.h>
#include <fstream>

std::vector<std::filesystem::path> showFileOpenDialog() {
    std::vector<std::filesystem::path> paths;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog* pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pFileOpen);
        if (SUCCEEDED(hr)) {
            FILEOPENDIALOGOPTIONS options;
            hr = pFileOpen->GetOptions(&options);
            if (SUCCEEDED(hr)) {
                pFileOpen->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM);
            }
            
            COMDLG_FILTERSPEC fileTypes[] = {
                { L"Video Files", L"*.mp4;*.mkv;*.avi;*.mov;*.webm" },
                { L"All Files", L"*.*" }
            };
            pFileOpen->SetFileTypes(2, fileTypes);
            
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItemArray* pResults;
                hr = pFileOpen->GetResults(&pResults);
                if (SUCCEEDED(hr)) {
                    DWORD count = 0;
                    pResults->GetCount(&count);
                    for (DWORD i = 0; i < count; ++i) {
                        IShellItem* pItem;
                        hr = pResults->GetItemAt(i, &pItem);
                        if (SUCCEEDED(hr)) {
                            LPWSTR pszFilePath;
                            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                            if (SUCCEEDED(hr)) {
                                paths.push_back(pszFilePath);
                                CoTaskMemFree(pszFilePath);
                            }
                            pItem->Release();
                        }
                    }
                    pResults->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return paths;
}

std::optional<std::filesystem::path> showFolderSelectDialog() {
    std::optional<std::filesystem::path> path;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        IFileOpenDialog* pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&pFileOpen);
        if (SUCCEEDED(hr)) {
            FILEOPENDIALOGOPTIONS options;
            hr = pFileOpen->GetOptions(&options);
            if (SUCCEEDED(hr)) {
                pFileOpen->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            }
            
            hr = pFileOpen->Show(NULL);
            if (SUCCEEDED(hr)) {
                IShellItem* pResult;
                hr = pFileOpen->GetResult(&pResult);
                if (SUCCEEDED(hr)) {
                    LPWSTR pszFilePath;
                    hr = pResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        path = std::filesystem::path(pszFilePath);
                        CoTaskMemFree(pszFilePath);
                    }
                    pResult->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }
    return path;
}

void openFileInDefaultApp(const std::filesystem::path& path) {
    ShellExecuteW(NULL, L"open", path.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

std::filesystem::path getSettingsFilePath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::filesystem::path p(exePath);
    return p.parent_path() / L"settings.txt";
}

std::string loadCustomOutputDirSetting() {
    try {
        auto path = getSettingsFilePath();
        if (std::filesystem::exists(path)) {
            std::ifstream file(path);
            if (file.is_open()) {
                std::string savedPath;
                std::getline(file, savedPath);
                return savedPath;
            }
        }
    } catch (...) {}
    return "";
}

void saveCustomOutputDirSetting(const std::string& path) {
    try {
        auto settingsPath = getSettingsFilePath();
        std::ofstream file(settingsPath);
        if (file.is_open()) {
            file << path;
        }
    } catch (...) {}
}

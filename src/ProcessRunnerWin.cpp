#include "ProcessRunner.hpp"
#include <windows.h>
#include <vector>
#include <iostream>

class ProcessRunnerWin : public IProcessRunner {
private:
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;
    std::string pipeBuffer;

public:
    ProcessRunnerWin() = default;
    ~ProcessRunnerWin() override {
        terminate();
    }

    bool launch(const std::wstring& cmd, std::string& errorMsg) override {
        terminate();

        SECURITY_ATTRIBUTES saAttr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
            errorMsg = "Failed to create pipes";
            return false;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdError = hWritePipe;
        si.hStdOutput = hWritePipe;
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        std::wstring cmdCopy = cmd;
        if (!CreateProcessW(nullptr, cmdCopy.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            errorMsg = "Failed to create process";
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            hReadPipe = nullptr;
            hWritePipe = nullptr;
            return false;
        }

        hProcess = pi.hProcess;
        hThread = pi.hThread;
        CloseHandle(hWritePipe); // Close write end in parent
        hWritePipe = nullptr;
        return true;
    }

    bool readLine(std::string& line) override {
        if (!hReadPipe) return false;

        // Check if we already have a complete line in our buffer
        size_t pos = pipeBuffer.find('\n');
        if (pos != std::string::npos) {
            line = pipeBuffer.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            pipeBuffer.erase(0, pos + 1);
            return true;
        }

        char buffer[512];
        DWORD bytesRead = 0;
        DWORD bytesAvail = 0;

        // Non-blocking: Check if there is data in the pipe to avoid blocking
        BOOL peekOk = PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &bytesAvail, nullptr);
        if (!peekOk) {
            // Pipe might be broken (process exited and closed write handle).
            // If so, return any remaining data in pipeBuffer.
            if (!pipeBuffer.empty()) {
                line = pipeBuffer;
                pipeBuffer.clear();
                return true;
            }
            return false;
        }

        if (bytesAvail == 0) {
            return false;
        }

        DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : (sizeof(buffer) - 1);
        if (ReadFile(hReadPipe, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
            pipeBuffer.append(buffer, bytesRead);
            
            pos = pipeBuffer.find('\n');
            if (pos != std::string::npos) {
                line = pipeBuffer.substr(0, pos);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                pipeBuffer.erase(0, pos + 1);
                return true;
            }
        }
        return false;
    }

    void terminate() override {
        if (hProcess) {
            TerminateProcess(hProcess, 1);
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
        if (hThread) {
            CloseHandle(hThread);
            hThread = nullptr;
        }
        if (hReadPipe) {
            CloseHandle(hReadPipe);
            hReadPipe = nullptr;
        }
        if (hWritePipe) {
            CloseHandle(hWritePipe);
            hWritePipe = nullptr;
        }
        pipeBuffer.clear();
    }

    int getExitCode() override {
        if (!hProcess) return 0;
        DWORD exitCode = 0;
        WaitForSingleObject(hProcess, INFINITE);
        GetExitCodeProcess(hProcess, &exitCode);
        return static_cast<int>(exitCode);
    }

    bool isActive() override {
        if (!hProcess) return false;
        DWORD exitCode = 0;
        GetExitCodeProcess(hProcess, &exitCode);
        return exitCode == STILL_ACTIVE;
    }
};

std::unique_ptr<IProcessRunner> createProcessRunner() {
    return std::make_unique<ProcessRunnerWin>();
}

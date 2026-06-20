#include "FFmpegRunner.hpp"
#include <windows.h>
#include <sstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <print>

std::wstring FFmpegRunner::getFFmpegPath() {
    // Check if we are running in x64 Debug or Release to locate the relative vcpkg tool path
    if (std::filesystem::exists("vcpkg_installed/x64-windows/tools/ffmpeg/ffmpeg.exe")) {
        return L"vcpkg_installed/x64-windows/tools/ffmpeg/ffmpeg.exe";
    }
    if (std::filesystem::exists("../vcpkg_installed/x64-windows/tools/ffmpeg/ffmpeg.exe")) {
        return L"../vcpkg_installed/x64-windows/tools/ffmpeg/ffmpeg.exe";
    }
    if (std::filesystem::exists("../../vcpkg_installed/x64-windows/tools/ffmpeg/ffmpeg.exe")) {
        return L"../../vcpkg_installed/x64-windows/tools/ffmpeg/ffmpeg.exe";
    }
    // Fall back to PATH environment search
    return L"ffmpeg.exe";
}

float FFmpegRunner::getVideoDuration(const std::filesystem::path& path) {
    std::wstringstream cmd;
    cmd << L"\"" << getFFmpegPath() << L"\" -i \"" << path.wstring() << L"\"";
    
    SECURITY_ATTRIBUTES saAttr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hChildStdErrRd = nullptr;
    HANDLE hChildStdErrWr = nullptr;

    if (!CreatePipe(&hChildStdErrRd, &hChildStdErrWr, &saAttr, 0)) {
        return 0.0f;
    }
    SetHandleInformation(hChildStdErrRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStdErrWr;
    si.hStdOutput = hChildStdErrWr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdStr = cmd.str();
    if (!CreateProcessW(nullptr, cmdStr.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hChildStdErrRd);
        CloseHandle(hChildStdErrWr);
        return 0.0f;
    }

    CloseHandle(hChildStdErrWr);

    char buffer[4096];
    DWORD bytesRead = 0;
    std::string accum;
    float duration = 0.0f;

    std::regex durRegex(R"(Duration:\s*(\d+):(\d+):([0-9.]+))");

    while (ReadFile(hChildStdErrRd, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        accum += buffer;

        std::smatch match;
        if (std::regex_search(accum, match, durRegex)) {
            int hours = std::stoi(match[1].str());
            int minutes = std::stoi(match[2].str());
            float seconds = std::stof(match[3].str());
            duration = hours * 3600.0f + minutes * 60.0f + seconds;
            break;
        }
    }

    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdErrRd);

    return duration;
}

std::expected<void, std::string> FFmpegRunner::compressVideo(
    CompressionTask& task,
    std::stop_token stopToken
) {
    float duration = getVideoDuration(task.inputPath);
    if (duration <= 0.0f) {
        return std::unexpected("Failed to read input video duration or invalid video file");
    }
    // Target size leaves a small buffer for container metadata
    double limitMB = task.targetSizeMB - 0.5;
    if (limitMB < 0.5) limitMB = task.targetSizeMB * 0.9; // Safe ratio for very small target sizes
    double targetBits = limitMB * 1024.0 * 1024.0 * 8.0;
    double totalBitrate = targetBits / duration;
    double videoBitrate = totalBitrate - 128000.0; // Subtract 128kbps audio
    if (videoBitrate < 100000.0) videoBitrate = 100000.0; // Min video bitrate is 100kbps

    int vBitrateKb = static_cast<int>(videoBitrate / 1000.0);

    task.status = TaskStatus::Compressing;
    task.progress = 0.0f;
    std::wstringstream cmd;
    cmd << L"\"" << getFFmpegPath() << L"\" -y -i \"" << task.inputPath.wstring() 
         << L"\" -c:v h264_mf -b:v " << vBitrateKb << L"k -pix_fmt yuv420p -c:a aac -b:a 128k \"" 
         << task.outputPath.wstring() << L"\"";
    
    return executeCommand(cmd.str(), task, stopToken);
}

std::expected<void, std::string> FFmpegRunner::executeCommand(
    const std::wstring& cmd,
    CompressionTask& task,
    std::stop_token stopToken
) {
    float duration = getVideoDuration(task.inputPath);

    SECURITY_ATTRIBUTES saAttr{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE hChildStdErrRd = nullptr;
    HANDLE hChildStdErrWr = nullptr;

    if (!CreatePipe(&hChildStdErrRd, &hChildStdErrWr, &saAttr, 0)) {
        return std::unexpected("Failed to create pipeline pipes");
    }
    if (!SetHandleInformation(hChildStdErrRd, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStdErrRd);
        CloseHandle(hChildStdErrWr);
        return std::unexpected("Failed to configure pipeline pipe settings");
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStdErrWr;
    si.hStdOutput = hChildStdErrWr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring commandCopy = cmd;
    if (!CreateProcessW(nullptr, commandCopy.data(), nullptr, nullptr, TRUE, 
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hChildStdErrRd);
        CloseHandle(hChildStdErrWr);
        return std::unexpected("Failed to launch ffmpeg process");
    }

    CloseHandle(hChildStdErrWr); // Close write end in parent

    char buffer[4096];
    DWORD bytesRead = 0;
    std::string accum;

    std::regex fpsRegex(R"(fps=\s*([0-9.]+))");
    std::regex timeRegex(R"(time=\s*(\d+):(\d+):([0-9.]+))");

    while (ReadFile(hChildStdErrRd, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        if (stopToken.stop_requested() || task.status == TaskStatus::Cancelled) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hChildStdErrRd);
            if (task.status != TaskStatus::Cancelled) {
                task.status = TaskStatus::Cancelled;
            }
            return std::unexpected("Compression aborted by user");
        }

        buffer[bytesRead] = '\0';
        accum += buffer;

        size_t lineEnd;
        while ((lineEnd = accum.find('\r')) != std::string::npos || (lineEnd = accum.find('\n')) != std::string::npos) {
            std::string line = accum.substr(0, lineEnd);
            accum.erase(0, lineEnd + 1);

            std::smatch match;
            if (std::regex_search(line, match, fpsRegex)) {
                task.fps = std::stof(match[1].str());
            }
            
            if (std::regex_search(line, match, timeRegex)) {
                int hours = std::stoi(match[1].str());
                int minutes = std::stoi(match[2].str());
                float seconds = std::stof(match[3].str());
                float currentTime = hours * 3600.0f + minutes * 60.0f + seconds;
                
                if (duration > 0.0f) {
                    float calculated = (currentTime / duration) * 100.0f;
                    if (calculated > 100.0f) calculated = 100.0f;
                    if (calculated < 0.0f) calculated = 0.0f;
                    task.progress = calculated;
                }
            }
        }
    }

    DWORD exitCode = 0;
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdErrRd);

    if (exitCode != 0) {
        return std::unexpected("FFmpeg returned exit code " + std::to_string(exitCode));
    }

    return {};
}

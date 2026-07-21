#include "FFmpegRunner.hpp"
#include "ProcessRunner.hpp"
#include <sstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <print>
#include <thread>
#include <chrono>

std::wstring FFmpegRunner::getFFmpegPath() {
#ifdef _WIN32
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
#else
    return L"ffmpeg";
#endif
}

float FFmpegRunner::getVideoDuration(const std::filesystem::path& path) {
    std::wstringstream cmd;
    cmd << L"\"" << getFFmpegPath() << L"\" -i \"" << path.wstring() << L"\"";
    
    auto runner = createProcessRunner();
    std::string errorMsg;
    if (!runner->launch(cmd.str(), errorMsg)) {
        return 0.0f;
    }

    std::string line;
    float duration = 0.0f;
    std::regex durRegex(R"(Duration:\s*(\d+):(\d+):([0-9.]+))");

    while (true) {
        bool hasLine = runner->readLine(line);
        if (hasLine) {
            std::smatch match;
            if (std::regex_search(line, match, durRegex)) {
                int hours = std::stoi(match[1].str());
                int minutes = std::stoi(match[2].str());
                float seconds = std::stof(match[3].str());
                duration = hours * 3600.0f + minutes * 60.0f + seconds;
                break;
            }
        }
        
        if (!hasLine && !runner->isActive()) {
            break;
        }
        
        if (!hasLine) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    runner->terminate();
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
    
    double limitMB = task.targetSizeMB - 0.5;
    if (limitMB < 0.5) limitMB = task.targetSizeMB * 0.9;
    double targetBits = limitMB * 1024.0 * 1024.0 * 8.0;
    double totalBitrate = targetBits / duration;
    double videoBitrate = totalBitrate - 128000.0;
    if (videoBitrate < 100000.0) videoBitrate = 100000.0;

    int vBitrateKb = static_cast<int>(videoBitrate / 1000.0);

    task.status = TaskStatus::Compressing;
    task.progress = 0.0f;

    // Ensure output directory exists if path contains one
    if (!task.outputPath.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(task.outputPath.parent_path(), ec);
        if (ec) {
            return std::unexpected("Failed to create output directory: " + ec.message() + ": \"" + task.outputPath.parent_path().string() + "\"");
        }
    }

    std::wstringstream cmd;
    cmd << L"\"" << getFFmpegPath() << L"\" -y -i \"" << task.inputPath.wstring() 
#ifdef _WIN32
         << L"\" -c:v h264_mf -b:v " 
#else
         << L"\" -c:v libx264 -b:v " 
#endif
         << vBitrateKb << L"k -pix_fmt yuv420p -c:a aac -b:a 128k \"" 
         << task.outputPath.wstring() << L"\"";
    
    return executeCommand(cmd.str(), task, stopToken, duration);
}

std::expected<void, std::string> FFmpegRunner::executeCommand(
    const std::wstring& cmd,
    CompressionTask& task,
    std::stop_token stopToken,
    float duration
) {
    auto runner = createProcessRunner();
    std::string errorMsg;
    if (!runner->launch(cmd, errorMsg)) {
        return std::unexpected("Failed to launch ffmpeg process: " + errorMsg);
    }

    std::string line;
    std::regex fpsRegex(R"(fps=\s*([0-9.]+))");
    std::regex timeRegex(R"(time=\s*(\d+):(\d+):([0-9.]+))");

    while (true) {
        if (stopToken.stop_requested() || task.status == TaskStatus::Cancelled) {
            runner->terminate();
            if (task.status != TaskStatus::Cancelled) {
                task.status = TaskStatus::Cancelled;
            }
            return std::unexpected("Compression aborted by user");
        }

        bool hasLine = runner->readLine(line);
        if (hasLine) {
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

        if (!hasLine && !runner->isActive()) {
            break;
        }

        if (!hasLine) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    int exitCode = runner->getExitCode();
    if (exitCode != 0) {
        return std::unexpected("FFmpeg returned exit code " + std::to_string(exitCode));
    }

    return {};
}

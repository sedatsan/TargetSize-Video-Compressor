#pragma once
#include "TaskStatus.hpp"
#include "ProcessRunner.hpp"
#include <expected>
#include <string>
#include <stop_token>

class FFmpegRunner {
private:
    static std::wstring getFFmpegPath();
    static float getVideoDuration(const std::filesystem::path& path);
    static std::expected<void, std::string> executeCommand(
        const std::wstring& cmd,
        CompressionTask& task,
        std::stop_token stopToken,
        float duration
    );

public:
    static std::expected<void, std::string> compressVideo(
        CompressionTask& task,
        std::stop_token stopToken
    );
};

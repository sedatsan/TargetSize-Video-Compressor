#pragma once
#include <expected>
#include <string>
#include <stop_token>
#include "TaskStatus.hpp"

class FFmpegRunner {
public:
    static std::expected<void, std::string> compressVideo(
        CompressionTask& task,
        std::stop_token stopToken
    );
private:
    static std::expected<void, std::string> executeCommand(
        const std::wstring& cmd,
        CompressionTask& task,
        std::stop_token stopToken
    );
    static std::wstring getFFmpegPath();
    static float getVideoDuration(const std::filesystem::path& path);
};

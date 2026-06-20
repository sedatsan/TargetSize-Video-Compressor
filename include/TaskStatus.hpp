#pragma once

#include <filesystem>
#include <atomic>
#include <string>

enum class TaskStatus {
    Pending,
    Analyzing,
    Compressing,
    Completed,
    Failed,
    Cancelled
};

struct CompressionTask {
    int id;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;

    std::atomic<TaskStatus> status{ TaskStatus::Pending };
    std::atomic<float> progress{ 0.0f }; // 0.0f - 100.0f
    std::atomic<float> fps{ 0.0f };
    std::string errorMessage;
    double targetSizeMB{ 24.0 };
};
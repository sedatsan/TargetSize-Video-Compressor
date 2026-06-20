#include "raylib.h"
#include "imgui.h"
#include "rlImGui.h"
#include "TaskQueue.hpp"
#include "ThreadPool.hpp"

#ifdef _WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <vector>
#include <memory>
#include <print>
#include <filesystem>
#include <string>
#include <format>
#include <optional>
#include <algorithm>
#include "dialogs.hpp"

void addVideoTasks(const std::vector<std::filesystem::path>& paths, 
                  std::vector<std::shared_ptr<CompressionTask>>& tasks, 
                  TaskQueue& queue, 
                  int& taskCounter, 
                  const char* outputFolderBuffer,
                  double targetSizeMB) {
    for (const auto& inputPath : paths) {
        if (std::filesystem::is_regular_file(inputPath)) {
            std::string ext = inputPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".mp4" || ext == ".mkv" || ext == ".avi" || ext == ".mov" || ext == ".webm") {
                auto task = std::make_shared<CompressionTask>();
                task->id = ++taskCounter;
                task->inputPath = inputPath;
                task->targetSizeMB = targetSizeMB;
                
                std::filesystem::path outFolder = outputFolderBuffer[0] != '\0' 
                    ? std::filesystem::path(outputFolderBuffer) 
                    : inputPath.parent_path();
                
                std::wstring outName = inputPath.stem().wstring() + L"_compressed" + inputPath.extension().wstring();
                task->outputPath = outFolder / outName;
                
                tasks.push_back(task);
                queue.push(task);
            }
        }
    }
}


std::string formatBytes(uint64_t bytes) {
    double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        return std::format("{:.2f} GB", mb / 1024.0);
    }
    return std::format("{:.1f} MB", mb);
}

const char* getStatusString(TaskStatus status) {
    switch (status) {
        case TaskStatus::Pending: return "Pending";
        case TaskStatus::Analyzing: return "Analyzing (Pass 1)";
        case TaskStatus::Compressing: return "Compressing (Pass 2)";
        case TaskStatus::Completed: return "Completed";
        case TaskStatus::Failed: return "Failed";
        case TaskStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

ImVec4 getStatusColor(TaskStatus status) {
    switch (status) {
        case TaskStatus::Pending: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);     // Grey
        case TaskStatus::Analyzing: return ImVec4(0.2f, 0.7f, 1.0f, 1.0f);   // Bright Blue
        case TaskStatus::Compressing: return ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Amber
        case TaskStatus::Completed: return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);   // Green
        case TaskStatus::Failed: return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);      // Red
        case TaskStatus::Cancelled: return ImVec4(0.9f, 0.5f, 0.1f, 1.0f);   // Orange
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}

static void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

int main() {
    const int screenWidth = 1024;
    const int screenHeight = 768;

    // Set config flags for resizing support
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    std::println("Initializing Raylib window...");
    InitWindow(screenWidth, screenHeight, "TargetSize Video Compressor Dashboard");
    SetTargetFPS(60);
    std::println("Raylib window initialized successfully.");

    // Setup ImGui bridge
    std::println("Initializing ImGui context via rlImGui...");
    rlImGuiSetup(true); // Enable dark theme
    std::println("ImGui context initialized successfully.");

    TaskQueue queue;
    int maxThreads = 2;
    std::println("Starting thread pool with {} threads...", maxThreads);
    auto pool = std::make_unique<ThreadPool>(maxThreads, queue);
    pool->start();
    std::println("ThreadPool started successfully.");

    std::vector<std::shared_ptr<CompressionTask>> tasks;
    char outputFolderBuffer[512] = "";
    // Load custom output directory setting on start
    std::string savedOutputDir = loadCustomOutputDirSetting();
    if (!savedOutputDir.empty()) {
        strncpy_s(outputFolderBuffer, savedOutputDir.c_str(), sizeof(outputFolderBuffer) - 1);
    }
    int taskCounter = 0;

    // Presets configuration
    struct TargetSizePreset {
        const char* label;
        double sizeMB;
    };
    const TargetSizePreset presets[] = {
        { "10 MB (Discord Free / WeChat Video)", 10.0 },
        { "16 MB (WhatsApp Video Standard)", 16.0 },
        { "24 MB (Discord Classic / Email Attachment - Safe Limit)", 24.0 }, // default
        { "50 MB (Discord Nitro Basic / Telegram)", 50.0 },
        { "100 MB (Discord Nitro / WhatsApp HD / TikTok)", 100.0 },
        { "512 MB (X/Twitter Free Video Limit)", 512.0 }
    };
    int selectedPresetIdx = 2; // Default to 25 MB
    float customTargetSizeMB = 25.0f;
    bool useCustomSize = false;

    while (!WindowShouldClose()) {
        // Handle window resizing
        int currentWidth = GetScreenWidth();
        int currentHeight = GetScreenHeight();

        // Calculate current target size for tasks added in this frame
        double currentTargetSizeMB = useCustomSize ? static_cast<double>(customTargetSizeMB) : presets[selectedPresetIdx].sizeMB;

        // Check for drag & drop file inputs
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            std::vector<std::filesystem::path> paths;
            for (size_t i = 0; i < droppedFiles.count; i++) {
                paths.push_back(droppedFiles.paths[i]);
            }
            addVideoTasks(paths, tasks, queue, taskCounter, outputFolderBuffer, currentTargetSizeMB);
            UnloadDroppedFiles(droppedFiles);
        }

        // Render loop
        BeginDrawing();
        ClearBackground(DARKGRAY);

        rlImGuiBegin();

        // Create main UI panel spanning the full window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(currentWidth), static_cast<float>(currentHeight)));
        ImGui::Begin("TargetSize Video Compressor Utility Dashboard", nullptr, 
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "TargetSize Batch Video Compressor");
        ImGui::Separator();

        // Settings Section
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Compression Configuration Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Target File Size Limit Section
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Target File Size Limit:");
            ImGui::SameLine();
            if (ImGui::RadioButton("Use Preset Limit", !useCustomSize)) {
                useCustomSize = false;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Custom Target Limit", useCustomSize)) {
                useCustomSize = true;
            }

            if (!useCustomSize) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
                if (ImGui::BeginCombo("##preset_combo", presets[selectedPresetIdx].label)) {
                    for (int i = 0; i < IM_ARRAYSIZE(presets); i++) {
                        const bool isSelected = (selectedPresetIdx == i);
                        if (ImGui::Selectable(presets[i].label, isSelected)) {
                            selectedPresetIdx = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                HelpMarker("Select a standardized file limit for various social networks.");
            } else {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
                ImGui::SliderFloat("##custom_size_slider", &customTargetSizeMB, 1.0f, 1000.0f, "%.1f MB");
                ImGui::SameLine();
                HelpMarker("Specify any custom target compression limit between 1.0 MB and 1000.0 MB.");
            }

            ImGui::Spacing();

            // Custom Output Directory Picker Section
            ImGui::TextUnformatted("Custom Output Directory (Optional):");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
            if (ImGui::InputText("##output_dir_input", outputFolderBuffer, sizeof(outputFolderBuffer))) {
                saveCustomOutputDirSetting(outputFolderBuffer);
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse...##output_dir", ImVec2(100.0f, 0.0f))) {
                auto selected = showFolderSelectDialog();
                if (selected) {
                    std::string pathStr = selected->string();
                    strncpy_s(outputFolderBuffer, pathStr.c_str(), sizeof(outputFolderBuffer) - 1);
                    saveCustomOutputDirSetting(outputFolderBuffer);
                }
            }
            ImGui::SameLine();
            HelpMarker("If left blank, compressed files will be saved in the same directory as their input files.");
            
            ImGui::Spacing();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Active Processing Threads (Max Concurrent):");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderInt("##threads", &maxThreads, 1, 8)) {
                // Resize thread pool dynamically by destroying and recreating via unique_ptr
                pool->stop();
                pool = std::make_unique<ThreadPool>(maxThreads, queue);
                pool->start();
            }
            ImGui::SameLine();
            HelpMarker("Configures the number of worker threads compiling videos simultaneously. Bounding this protects CPU/GPU overload.");
        }

        ImGui::Spacing();

        // Button to browse video files explicitly
        if (ImGui::Button("Add Video Files...##browse_videos", ImVec2(200.0f, 30.0f))) {
            auto selectedFiles = showFileOpenDialog();
            if (!selectedFiles.empty()) {
                addVideoTasks(selectedFiles, tasks, queue, taskCounter, outputFolderBuffer, currentTargetSizeMB);
            }
        }
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("or drop them into the area below:");

        // Drag & Drop visual border area (also clickable to choose files)
        ImVec2 dragAreaSize(ImGui::GetContentRegionAvail().x, 80.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::BeginChild("DragDropTarget", dragAreaSize, true, ImGuiWindowFlags_None);
        
        // Dotted border simulator
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetWindowPos();
        ImVec2 p1 = ImVec2(p0.x + dragAreaSize.x, p0.y + dragAreaSize.y);
        drawList->AddRect(p0, p1, IM_COL32(255, 255, 255, 80), 5.0f, 0, 2.0f);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 25.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::SetWindowFontScale(1.2f);
        float textWidth = ImGui::CalcTextSize("Drag & Drop Video Files here or CLICK to browse files").x;
        ImGui::SetCursorPosX((dragAreaSize.x - textWidth) / 2.0f);
        ImGui::TextUnformatted("Drag & Drop Video Files here or CLICK to browse files");
        ImGui::SetWindowFontScale(1.0f);
        
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // If they click on the drag & drop area
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            auto selectedFiles = showFileOpenDialog();
            if (!selectedFiles.empty()) {
                addVideoTasks(selectedFiles, tasks, queue, taskCounter, outputFolderBuffer, currentTargetSizeMB);
            }
        }

        ImGui::Spacing();

        // Table Queue Section
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Processing Queue Manager");
        
        ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
        ImVec2 tableSize(0.0f, ImGui::GetContentRegionAvail().y - 45.0f);
        
        if (ImGui::BeginTable("QueueTable", 6, flags, tableSize)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Source Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Input Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Status & Speed", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Compression Progress", ImGuiTableColumnFlags_WidthStretch, 2.5f);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            for (const auto& t : tasks) {
                ImGui::TableNextRow();
                
                // Column 1: ID
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", t->id);

                // Column 2: File Name
                ImGui::TableSetColumnIndex(1);
                std::string filename = t->inputPath.filename().string();
                ImGui::TextUnformatted(filename.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", t->inputPath.string().c_str());
                }

                // Column 3: Input Size
                ImGui::TableSetColumnIndex(2);
                try {
                    if (std::filesystem::exists(t->inputPath)) {
                        uint64_t size = std::filesystem::file_size(t->inputPath);
                        ImGui::TextUnformatted(formatBytes(size).c_str());
                    } else {
                        ImGui::TextDisabled("N/A");
                    }
                } catch (...) {
                    ImGui::TextDisabled("N/A");
                }

                // Column 4: Status & Speed
                ImGui::TableSetColumnIndex(3);
                TaskStatus st = t->status.load();
                ImGui::TextColored(getStatusColor(st), "%s", getStatusString(st));
                if (st == TaskStatus::Compressing || st == TaskStatus::Analyzing) {
                    float fps = t->fps.load();
                    if (fps > 0.0f) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(@ %.1f fps)", fps);
                    }
                }
                if (st == TaskStatus::Failed) {
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", t->errorMessage.c_str());
                    }
                }

                // Column 5: Progress Bar
                ImGui::TableSetColumnIndex(4);
                float progVal = t->progress.load();
                std::string progText = std::format("{:.1f}%", progVal);
                ImGui::ProgressBar(progVal / 100.0f, ImVec2(-FLT_MIN, 0), progText.c_str());

                // Column 6: Actions
                ImGui::TableSetColumnIndex(5);
                if (st == TaskStatus::Pending || st == TaskStatus::Analyzing || st == TaskStatus::Compressing) {
                    std::string btnLabel = std::format("Cancel##{}", t->id);
                    if (ImGui::Button(btnLabel.c_str(), ImVec2(-FLT_MIN, 0))) {
                        t->status = TaskStatus::Cancelled;
                    }
                } else if (st == TaskStatus::Cancelled || st == TaskStatus::Failed) {
                    std::string btnLabel = std::format("Retry##{}", t->id);
                    if (ImGui::Button(btnLabel.c_str(), ImVec2(-FLT_MIN, 0))) {
                        t->status = TaskStatus::Pending;
                        t->progress = 0.0f;
                        t->fps = 0.0f;
                        queue.push(t);
                    }
                } else if (st == TaskStatus::Completed) {
                    std::string btnLabel = std::format("Open##{}", t->id);
                    if (ImGui::Button(btnLabel.c_str(), ImVec2(-FLT_MIN, 0))) {
                        openFileInDefaultApp(t->outputPath);
                    }
                }
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Spacing();
        
        // Footer buttons
        if (ImGui::Button("Clear All Completed", ImVec2(180, 0))) {
            tasks.erase(std::remove_if(tasks.begin(), tasks.end(), [](const auto& t) {
                TaskStatus st = t->status.load();
                return st == TaskStatus::Completed || st == TaskStatus::Cancelled || st == TaskStatus::Failed;
            }), tasks.end());
        }


        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
    }

    pool->stop();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}

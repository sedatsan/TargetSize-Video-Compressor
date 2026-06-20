#pragma once

#include <vector>
#include <filesystem>
#include <optional>
#include <string>

// Displays a native Windows file explorer dialog to select one or more video files.
std::vector<std::filesystem::path> showFileOpenDialog();

// Displays a native Windows file explorer dialog to select a folder.
std::optional<std::filesystem::path> showFolderSelectDialog();

// Opens a file using its default registered application on Windows.
void openFileInDefaultApp(const std::filesystem::path& path);

// Loads the saved custom output directory from the settings file next to the executable.
std::string loadCustomOutputDirSetting();

// Saves the custom output directory to the settings file next to the executable.
void saveCustomOutputDirSetting(const std::string& path);



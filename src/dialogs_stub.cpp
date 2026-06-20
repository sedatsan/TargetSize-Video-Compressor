#include "dialogs.hpp"

std::vector<std::filesystem::path> showFileOpenDialog() {
    return {}; // Stub returning empty
}

std::optional<std::filesystem::path> showFolderSelectDialog() {
    return std::nullopt; // Stub returning nullopt
}

void openFileInDefaultApp(const std::filesystem::path& path) {
    // Stub
}

std::string loadCustomOutputDirSetting() {
    return ""; // Stub returning empty
}

void saveCustomOutputDirSetting(const std::string& path) {
    // Stub
}

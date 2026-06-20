#include "ProcessRunner.hpp"

class ProcessRunnerPosix : public IProcessRunner {
public:
    bool launch(const std::wstring& cmd, std::string& errorMsg) override {
        errorMsg = "POSIX process runner not implemented yet";
        return false;
    }
    bool readLine(std::string& line) override {
        return false;
    }
    void terminate() override {}
    int getExitCode() override {
        return 1;
    }
    bool isActive() override {
        return false;
    }
};

// Note: Only used on non-Windows targets.
// Windows builds compile ProcessRunnerWin.cpp which defines createProcessRunner.
#ifndef _WIN32
std::unique_ptr<IProcessRunner> createProcessRunner() {
    return std::make_unique<ProcessRunnerPosix>();
}
#endif

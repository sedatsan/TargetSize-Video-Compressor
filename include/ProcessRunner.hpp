#pragma once
#include <string>
#include <memory>

struct ProcessResult {
    int exitCode = 0;
    std::string stdOut;
    std::string stdErr;
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    
    // Launches a process with a given command line.
    virtual bool launch(const std::wstring& cmd, std::string& errorMsg) = 0;
    
    // Reads a line from redirected output stream (non-blocking if possible).
    virtual bool readLine(std::string& line) = 0;
    
    // Force terminates the running process.
    virtual void terminate() = 0;
    
    // Gets process exit code.
    virtual int getExitCode() = 0;
    
    // Checks if process is active.
    virtual bool isActive() = 0;
};

// Factory function to create platform-specific process runners.
std::unique_ptr<IProcessRunner> createProcessRunner();

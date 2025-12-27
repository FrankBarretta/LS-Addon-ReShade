#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    static void Init(const std::wstring& logPath);
    static void Log(const std::string& message);
    static void Close();

private:
    static std::ofstream logFile;
    static std::mutex logMutex;
};

void Log(const std::string& message);
template<typename... Args>
void Log(const char* fmt, Args... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), fmt, args...);
    Log(std::string(buffer));
}

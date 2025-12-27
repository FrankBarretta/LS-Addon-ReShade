#include "logger.hpp"
#include <iostream>
#include <windows.h>

std::ofstream Logger::logFile;
std::mutex Logger::logMutex;

void Logger::Init(const std::wstring& logPath) {
    std::lock_guard<std::mutex> lock(logMutex);
    logFile.open(logPath, std::ios::out | std::ios::trunc);
    if (logFile.is_open()) {
        logFile << "LS_ReShade Log Initialized" << std::endl;
    }
}

void Logger::Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.flush();
    }
    // Also print to debug console
    OutputDebugStringA((message + "\n").c_str());
}

void Logger::Close() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Log(const std::string& message) {
    Logger::Log(message);
}

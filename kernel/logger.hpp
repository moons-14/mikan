#pragma once

enum LogLevel
{
    kError = 3,
    kWarn = 4,
    kInfo = 6,
    kDebug = 7,
};

// 表示するログの閾値を変更する
void SetLogLevel(LogLevel level);

// ログを記録する
int Log(LogLevel level, const char *format, ...);
#pragma once

#include <string_view>

void logMessage(std::string_view message);
void throwSystemError(std::string_view message);

int readFull(int fd, char* buffer, std::size_t bytesToRead);

int writeAll(int fd, const char* buffer, std::size_t bytesToWrite) ;
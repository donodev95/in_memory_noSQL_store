#pragma once

#include <string_view>

void logMessage(std::string_view message);
void throwSystemError(std::string_view message);
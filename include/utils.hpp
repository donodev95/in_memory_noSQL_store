#pragma once

#include <string_view>
#include <iostream>
using Buffer = std::vector<uint8_t>;

void logMessage(std::string_view message);
void throwSystemError(std::string_view message);

int readFull(int fd, uint8_t* buffer, std::size_t bytesToRead);

int writeAll(int fd, const uint8_t* buffer, std::size_t bytesToWrite) ;

// Generic buffer helpers
void bufAppend(Buffer& buffer, const uint8_t* data, std::size_t length);
void bufAppendU8(Buffer& buffer, uint8_t value);
void bufAppendU32(Buffer& buffer, uint32_t value);
void bufAppendI64(Buffer& buffer, int64_t value);

// Serialization helpers
enum {
        TAG_NIL = 0,
        TAG_ERR = 1,
        TAG_STR = 2,
        TAG_INT = 3,
        TAG_DBL = 4,
        TAG_ARR = 5,
    };
    enum {
        ERR_UNKNOWN = 1,
        ERR_TOO_BIG = 2,
    };

void outNil(Buffer& out);
void outStr(Buffer& out, const char* data, std::size_t size);
void outInt(Buffer& out, int64_t value);
void outArr(Buffer& out, uint32_t count);
void outErr(Buffer& out, uint32_t code, const std::string& message);
#pragma once

#define NOMINMAX

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>
#include <functional>
#include <stdexcept>
#include <cstdio>

using GPA = uint64_t;
using HVA = uint8_t*;

constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kPageMask = ~(kPageSize - 1);

inline uint64_t AlignUp(uint64_t val, uint64_t align) {
    return (val + align - 1) & ~(align - 1);
}

inline uint64_t AlignDown(uint64_t val, uint64_t align) {
    return val & ~(align - 1);
}

#define LOG_INFO(fmt, ...)  fprintf(stdout, "[INFO]  " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#ifdef _DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stdout, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

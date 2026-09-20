#pragma once

#include <windows.h>
#include "rotating_log.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace taxi_camera::diagnostics {

inline std::wstring log_directory() noexcept {
  wchar_t buffer[32768]{};
  const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, 32768);
  if (!n || n >= 32700)
    return {};
  std::wstring path(buffer);
  path += L"\\TaxiCam-Fixed";
  CreateDirectoryW(path.c_str(), nullptr);
  return path;
}

inline void write(std::string_view module, std::string_view level, std::string_view code,
                  const char* file, const char* function, int line,
                  std::string_view message) noexcept {
  try {
    const auto directory = log_directory();
    if (directory.empty())
      return;

    SYSTEMTIME utc{};
    GetSystemTime(&utc);

    const char* basename = file ? file : "";
    if (basename) {
      if (const char* slash = std::strrchr(basename, '\\'))
        basename = slash + 1;
      if (const char* slash = std::strrchr(basename, '/'))
        basename = slash + 1;
    }

    char record[4096]{};
    const int length = std::snprintf(
        record, sizeof(record),
        "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ "
        "pid=%lu tid=%lu module=%.*s level=%.*s code=%.*s "
        "file=%s function=%s line=%d | %.*s\r\n",
        utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, utc.wMilliseconds,
        static_cast<unsigned long>(GetCurrentProcessId()),
        static_cast<unsigned long>(GetCurrentThreadId()),
        static_cast<int>(module.size()), module.data(),
        static_cast<int>(level.size()), level.data(),
        static_cast<int>(code.size()), code.data(),
        basename ? basename : "", function ? function : "", line,
        static_cast<int>(message.size()), message.data());

    if (length > 0 && static_cast<std::size_t>(length) < sizeof(record)) {
      standalone::append_rotating_log(directory + L"\\diagnostics.log",
                                       std::string_view(record, static_cast<std::size_t>(length)),
                                       8ull * 1024ull * 1024ull);
    }
  } catch (...) {
    // Diagnostics must never affect simulator operation.
  }
}

inline void status(std::string_view module, std::string_view code,
                   const char* file, const char* function, int line,
                   std::string_view message) noexcept {
  write(module, "INFO", code, file, function, line, message);
}

inline void warning(std::string_view module, std::string_view code,
                    const char* file, const char* function, int line,
                    std::string_view message) noexcept {
  write(module, "WARN", code, file, function, line, message);
}

inline void error(std::string_view module, std::string_view code,
                  const char* file, const char* function, int line,
                  std::string_view message) noexcept {
  write(module, "ERROR", code, file, function, line, message);
}


inline std::uint64_t session_id() noexcept {
  static const std::uint64_t id = (static_cast<std::uint64_t>(GetCurrentProcessId()) << 32) ^ GetTickCount64();
  return id;
}

inline void event(std::string_view module, std::string_view code, std::string_view classification,
                  std::string_view message, const char* file, const char* function, int line) noexcept {
  char detail[3072]{};
  const int n = std::snprintf(detail, sizeof(detail),
                              "kind=EVENT session=%llu classification=%.*s message=%.*s",
                              static_cast<unsigned long long>(session_id()),
                              static_cast<int>(classification.size()), classification.data(),
                              static_cast<int>(message.size()), message.data());
  if (n > 0)
    write(module, "INFO", code, file, function, line,
          std::string_view(detail, static_cast<std::size_t>(n < static_cast<int>(sizeof(detail)) ? n : sizeof(detail) - 1)));
}

inline void watchdog(std::string_view module, std::string_view code, std::string_view state,
                     std::uint64_t heartbeat_age_ms, std::string_view action,
                     const char* file, const char* function, int line) noexcept {
  char detail[2048]{};
  const int n = std::snprintf(detail, sizeof(detail),
                              "kind=WATCHDOG session=%llu state=%.*s heartbeat_age_ms=%llu action=%.*s",
                              static_cast<unsigned long long>(session_id()),
                              static_cast<int>(state.size()), state.data(),
                              static_cast<unsigned long long>(heartbeat_age_ms),
                              static_cast<int>(action.size()), action.data());
  if (n > 0)
    write(module, "INFO", code, file, function, line,
          std::string_view(detail, static_cast<std::size_t>(n < static_cast<int>(sizeof(detail)) ? n : sizeof(detail) - 1)));
}

}  // namespace taxi_camera::diagnostics

#define TAXI_DIAG_INFO(module, code, message) \
  ::taxi_camera::diagnostics::status((module), (code), __FILE__, __func__, __LINE__, (message))
#define TAXI_DIAG_WARN(module, code, message) \
  ::taxi_camera::diagnostics::warning((module), (code), __FILE__, __func__, __LINE__, (message))
#define TAXI_DIAG_ERROR(module, code, message) \
  ::taxi_camera::diagnostics::error((module), (code), __FILE__, __func__, __LINE__, (message))
#define TAXI_DIAG_EVENT(module, code, classification, message) \
  ::taxi_camera::diagnostics::event((module), (code), (classification), (message), __FILE__, __func__, __LINE__)
#define TAXI_DIAG_WATCHDOG(module, code, state, age_ms, action) \
  ::taxi_camera::diagnostics::watchdog((module), (code), (state), (age_ms), (action), __FILE__, __func__, __LINE__)

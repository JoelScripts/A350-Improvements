#pragma once

#include <windows.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include "protocol.hpp"
#include "rotating_log.hpp"

namespace taxi_camera::standalone::advanced_diagnostics {

inline constexpr std::uint64_t EventLogBytes = 16ull * 1024ull * 1024ull;
inline constexpr std::uint64_t StateLogBytes = 2ull * 1024ull * 1024ull;
inline constexpr std::size_t MaxDetailBytes = 4096;

struct Snapshot {
  std::uint64_t heartbeat{};
  std::uint64_t captures{};
  std::uint64_t composed{};
  std::uint64_t stamps{};
  std::uint64_t left_id{};
  std::uint64_t right_id{};
  std::uint64_t hook_failures{};
  std::uint32_t graphics_ready{};
  std::uint32_t scene_ready{};
  std::uint32_t taxi_mask{};
  std::uint32_t speed_inhibited{};
  std::uint32_t candidate_count{};
  std::uint32_t active_profile{};
  std::uint32_t detected_profile{};
  std::uint64_t identity_sample_ms{};
  std::uint64_t aircraft_session_epoch{};
  float speed{};
  float exposure{};
  double probe_cpu_ms{};
  double probe_max_ms{};
  std::array<double, 10> stage_ms{};
  char aircraft_type[256]{};
  char aircraft_path[260]{};
  char message[384]{};
};

inline std::atomic<std::uint64_t> sequence{};
inline std::uint64_t session{};
inline std::wstring directory;
inline std::wstring events_path;
inline std::wstring state_path;
inline std::wstring readable_path;
inline std::wstring summary_path;
inline std::atomic<bool> ready{};
inline std::atomic<std::uint64_t> error_count{};
inline std::atomic<std::uint64_t> warning_count{};
inline std::atomic<std::uint64_t> recovery_count{};

inline std::string json_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 16);
  constexpr char hex[] = "0123456789ABCDEF";
  for (const unsigned char c : value) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out += hex[c >> 4];
          out += hex[c & 15];
        } else out += static_cast<char>(c);
    }
  }
  return out;
}

inline std::string narrow(const wchar_t* value) {
  if (!value) return {};
  const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (count <= 1) return {};
  std::string out(static_cast<size_t>(count - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), count - 1, nullptr, nullptr);
  return out;
}

inline std::string timestamp_utc() {
  SYSTEMTIME t{};
  GetSystemTime(&t);
  char out[64];
  std::snprintf(out, sizeof(out), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                t.wSecond, t.wMilliseconds);
  return out;
}

inline void snapshot(const Status& s, Snapshot& out) noexcept {
  out.heartbeat=s.heartbeat; out.captures=s.captures; out.composed=s.composed; out.stamps=s.stamps;
  out.left_id=s.left_id; out.right_id=s.right_id; out.hook_failures=s.hook_failures;
  out.graphics_ready=s.graphics_ready; out.scene_ready=s.scene_ready; out.taxi_mask=s.taxi_mask;
  out.speed_inhibited=s.speed_inhibited; out.candidate_count=s.candidate_count;
  out.active_profile=s.active_profile; out.detected_profile=s.detected_profile;
  out.identity_sample_ms=s.identity_sample_ms; out.aircraft_session_epoch=s.aircraft_session_epoch;
  out.speed=s.speed; out.exposure=s.exposure; out.probe_cpu_ms=s.probe_cpu_ms; out.probe_max_ms=s.probe_max_ms;
  out.stage_ms=s.stage_ms;
  std::snprintf(out.aircraft_type,sizeof(out.aircraft_type),"%s",s.aircraft_type);
  std::snprintf(out.aircraft_path,sizeof(out.aircraft_path),"%s",s.aircraft_path);
  std::snprintf(out.message,sizeof(out.message),"%s",s.message);
}

inline const char* severity_for(std::string_view detail) noexcept {
  if (detail.find("failed") != std::string_view::npos || detail.find("error") != std::string_view::npos ||
      detail.find("refused") != std::string_view::npos || detail.find("fault") != std::string_view::npos)
    return "error";
  if (detail.find("stale") != std::string_view::npos || detail.find("recovery") != std::string_view::npos ||
      detail.find("waiting") != std::string_view::npos || detail.find("retry") != std::string_view::npos)
    return "warning";
  return "info";
}

inline const char* classify(std::string_view detail) noexcept {
  if (detail.find("graphics") != std::string_view::npos || detail.find("Graphics") != std::string_view::npos) return "graphics";
  if (detail.find("PFD") != std::string_view::npos || detail.find("pfd") != std::string_view::npos || detail.find("stamp") != std::string_view::npos) return "pfd";
  if (detail.find("capture") != std::string_view::npos || detail.find("Scene") != std::string_view::npos || detail.find("scene") != std::string_view::npos) return "capture";
  if (detail.find("camera") != std::string_view::npos || detail.find("Camera") != std::string_view::npos || detail.find("view") != std::string_view::npos) return "camera";
  if (detail.find("aircraft") != std::string_view::npos || detail.find("Aircraft") != std::string_view::npos || detail.find("identity") != std::string_view::npos) return "aircraft";
  if (detail.find("heartbeat") != std::string_view::npos || detail.find("Bridge") != std::string_view::npos) return "watchdog";
  if (detail.find("fault") != std::string_view::npos || detail.find("exception") != std::string_view::npos) return "crash_evidence";
  return "runtime";
}

inline void write_readable(std::string_view utc, std::string_view severity, std::string_view category,
                           std::string_view component, std::string_view name, std::string_view detail) noexcept {
  try {
    char line[8192]{};
    const char* marker = severity == "error" ? "[ERROR]" : severity == "warning" ? "[WARNING]" :
                         severity == "recovery" ? "[RECOVERY]" : "[INFO]";
    std::snprintf(line, sizeof(line), "%.*s %s [%.*s] [%.*s] %.*s: %.*s\r\n",
      static_cast<int>(utc.size()), utc.data(), marker,
      static_cast<int>(category.size()), category.data(),
      static_cast<int>(component.size()), component.data(),
      static_cast<int>(name.size()), name.data(),
      static_cast<int>(detail.size()), detail.data());
    append_rotating_log(readable_path, line, EventLogBytes);
  } catch (...) {}
}

inline void write_summary(const Snapshot& snap, std::string_view reason) noexcept {
  try {
    char text[8192]{};
    const auto errors = error_count.load();
    const auto warnings = warning_count.load();
    const auto recoveries = recovery_count.load();
    const char* overall = errors ? "ATTENTION REQUIRED" : warnings ? "WARNINGS DETECTED" : "OK";
    std::snprintf(text, sizeof(text),
      "TAXI CAM DIAGNOSTIC SUMMARY\r\n"
      "==========================\r\n"
      "STATUS: %s\r\n\r\n"
      "Aircraft: %s\r\n"
      "Aircraft path: %s\r\n"
      "Graphics: %s\r\n"
      "Scene: %s\r\n"
      "Left PFD: %llu\r\n"
      "Right PFD: %llu\r\n"
      "Captures: %llu\r\n"
      "Compositions: %llu\r\n"
      "PFD stamps: %llu\r\n"
      "Hook failures: %llu\r\n"
      "Warnings: %llu\r\n"
      "Errors: %llu\r\n"
      "Recoveries: %llu\r\n"
      "Heartbeat: %llu\r\n"
      "Last update: %s\r\n"
      "Reason: %.*s\r\n\r\n"
      "LOG FILES\r\n"
      "----------\r\n"
      "Readable events: advanced-diagnostics.log\r\n"
      "Machine-readable events: advanced-diagnostics.jsonl\r\n"
      "Current state: advanced-state.txt\r\n",
      overall, snap.aircraft_type[0] ? snap.aircraft_type : "Unknown", snap.aircraft_path,
      snap.graphics_ready ? "READY" : "WAITING", snap.scene_ready ? "READY" : "WAITING",
      (unsigned long long)snap.left_id, (unsigned long long)snap.right_id,
      (unsigned long long)snap.captures, (unsigned long long)snap.composed, (unsigned long long)snap.stamps,
      (unsigned long long)snap.hook_failures, (unsigned long long)warnings, (unsigned long long)errors,
      (unsigned long long)recoveries, (unsigned long long)snap.heartbeat, timestamp_utc().c_str(),
      (int)reason.size(), reason.data());
    append_rotating_log(summary_path, text, 2ull * 1024ull * 1024ull);
  } catch (...) {}
}

inline bool initialize(std::string_view component) noexcept {
  if (ready.load(std::memory_order_acquire)) return true;
  try {
    wchar_t local[32768]{};
    const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if (!n || n>=32700) return false;
    directory=std::wstring(local)+L"\\Taxi Cam";
    CreateDirectoryW(directory.c_str(),nullptr);
    events_path=directory+L"\\advanced-diagnostics.jsonl";
    readable_path=directory+L"\\advanced-diagnostics.log";
    summary_path=directory+L"\\diagnostics-summary.txt";
    state_path=directory+L"\\advanced-state.txt";
    session=(static_cast<std::uint64_t>(GetCurrentProcessId())<<32)^GetTickCount64()^static_cast<std::uint64_t>(GetCurrentThreadId());
    if (!session) session=1;
    ready.store(true,std::memory_order_release);
    char detail[256];
    std::snprintf(detail,sizeof(detail),"Advanced diagnostics initialized; component=%s",std::string(component).c_str());
    // Avoid recursion through event(); write the bootstrap record directly.
    char line[1024];
    const auto seq=sequence.fetch_add(1)+1;
    std::snprintf(line,sizeof(line),"{\"utc\":\"%s\",\"sequence\":%llu,\"session\":%llu,\"pid\":%lu,\"tid\":%lu,\"severity\":\"info\",\"category\":\"diagnostics\",\"event\":\"diagnostics.initialized\",\"detail\":\"%s\"}\r\n",
      timestamp_utc().c_str(),(unsigned long long)seq,(unsigned long long)session,GetCurrentProcessId(),GetCurrentThreadId(),json_escape(detail).c_str());
    append_rotating_log(events_path,line,EventLogBytes);
    write_readable(timestamp_utc(), "info", "diagnostics", component, "diagnostics.initialized", detail);
    Snapshot empty{};
    write_summary(empty, "startup");
    append_rotating_log(state_path, "Taxi Cam advanced diagnostic state\r\nState file initialized.\r\n", StateLogBytes);
    return true;
  } catch (...) { return false; }
}

inline void event(std::string_view name,std::string_view detail,std::string_view component="runtime",const char* file=nullptr,int line=0,const char* function=nullptr,const Status* status=nullptr) noexcept {
  try {
    if (!ready.load(std::memory_order_acquire) && !initialize(component)) return;
    std::string d(detail.substr(0,MaxDetailBytes));
    const auto seq=sequence.fetch_add(1)+1;
    char linebuf[8192];
    const auto category=classify(d);
    const auto severity=severity_for(d);
    if (std::strcmp(severity, "error") == 0) error_count.fetch_add(1);
    else if (std::strcmp(severity, "warning") == 0) warning_count.fetch_add(1);
    else if (d.find("recovery") != std::string::npos || d.find("recovered") != std::string::npos) recovery_count.fetch_add(1);
    const auto utc = timestamp_utc();
    Snapshot snap{};
    if (status) snapshot(*status,snap);
    std::snprintf(linebuf,sizeof(linebuf),
      "{\"utc\":\"%s\",\"sequence\":%llu,\"session\":%llu,\"pid\":%lu,\"tid\":%lu,\"severity\":\"%s\",\"category\":\"%s\",\"component\":\"%s\",\"event\":\"%s\",\"source\":\"%s\",\"line\":%d,\"function\":\"%s\",\"detail\":\"%s\",\"heartbeat\":%llu,\"graphics_ready\":%u,\"scene_ready\":%u,\"left_id\":%llu,\"right_id\":%llu,\"captures\":%llu,\"composed\":%llu,\"stamps\":%llu,\"hook_failures\":%llu,\"candidate_count\":%u,\"taxi_mask\":%u,\"speed\":%.3f,\"probe_cpu_ms\":%.3f,\"probe_max_ms\":%.3f,\"aircraft_type\":\"%s\",\"aircraft_path\":\"%s\",\"message\":\"%s\"}\r\n",
      timestamp_utc().c_str(),(unsigned long long)seq,(unsigned long long)session,GetCurrentProcessId(),GetCurrentThreadId(),severity,category,
      json_escape(component).c_str(),json_escape(name).c_str(),json_escape(file?file:"").c_str(),line,json_escape(function?function:"").c_str(),json_escape(d).c_str(),
      (unsigned long long)snap.heartbeat,snap.graphics_ready,snap.scene_ready,(unsigned long long)snap.left_id,(unsigned long long)snap.right_id,
      (unsigned long long)snap.captures,(unsigned long long)snap.composed,(unsigned long long)snap.stamps,(unsigned long long)snap.hook_failures,
      snap.candidate_count,snap.taxi_mask,snap.speed,snap.probe_cpu_ms,snap.probe_max_ms,json_escape(snap.aircraft_type).c_str(),json_escape(snap.aircraft_path).c_str(),json_escape(snap.message).c_str());
    append_rotating_log(events_path,linebuf,EventLogBytes);
    write_readable(utc, severity, category, component, name, d);
    write_summary(snap, d);
  } catch (...) {}
}

inline void heartbeat(const Status& status,const char* component="bridge",const char* file=nullptr,int line=0,const char* function=nullptr) noexcept {
  event("runtime.heartbeat","Bridge heartbeat observed",component,file,line,function,&status);
}

inline void state_snapshot(const Status& status,std::string_view reason="periodic") noexcept {
  try {
    if (!ready.load(std::memory_order_acquire) && !initialize("runtime")) return;
    char text[4096];
    std::snprintf(text,sizeof(text),"Taxi Cam advanced diagnostic state\r\nUTC=%s\r\nSequence=%llu\r\nSession=%llu\r\nPID=%lu\r\nReason=%s\r\nHeartbeat=%llu\r\nGraphicsReady=%u\r\nSceneReady=%u\r\nTaxiMask=%u\r\nLeftPFD=%llu\r\nRightPFD=%llu\r\nCaptures=%llu\r\nComposed=%llu\r\nStamps=%llu\r\nHookFailures=%llu\r\nCandidates=%u\r\nSpeed=%.3f\r\nProbeCPUms=%.3f\r\nProbeMaxMs=%.3f\r\nAircraftType=%s\r\nAircraftPath=%s\r\nMessage=%s\r\n",
      timestamp_utc().c_str(),(unsigned long long)sequence.load(),(unsigned long long)session,GetCurrentProcessId(),std::string(reason).c_str(),
      (unsigned long long)status.heartbeat,status.graphics_ready,status.scene_ready,status.taxi_mask,(unsigned long long)status.left_id,(unsigned long long)status.right_id,
      (unsigned long long)status.captures,(unsigned long long)status.composed,(unsigned long long)status.stamps,(unsigned long long)status.hook_failures,status.candidate_count,
      status.speed,status.probe_cpu_ms,status.probe_max_ms,status.aircraft_type,status.aircraft_path,status.message);
    append_rotating_log(state_path,text,StateLogBytes);
    Snapshot snap{}; snapshot(status, snap); write_summary(snap, reason);
  } catch (...) {}
}

} // namespace taxi_camera::standalone::advanced_diagnostics

#define TAXI_ADV_DIAG_EVENT(name, detail, component, status_ptr) \
  ::taxi_camera::standalone::advanced_diagnostics::event((name), (detail), (component), __FILE__, __LINE__, __func__, (status_ptr))

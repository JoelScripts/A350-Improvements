#include <windows.h>
#include <winhttp.h>
#include <commctrl.h>
#include <commdlg.h>
#include "../camera/aircraft_identity.hpp"
#include <dwmapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>
#include "launcher.hpp"
#include "launcher_log.hpp"
#include "connection_recoverability.hpp"
#include "../shared/protocol.hpp"
#include "settings_store.hpp"
#include "startup_state.hpp"
#include "camera_hotkeys.hpp"
#include "bug_report.hpp"
#include "../shared/manual_camera_intent.hpp"
#include "../shared/profile_selection.hpp"
#include "../graphics/target_assignment.hpp"
#include "updater.hpp"
#include "../shared/diagnostics.hpp"
#include "../shared/advanced_diagnostics.hpp"

namespace {
using namespace taxi_camera;
namespace win = standalone;
constexpr UINT TrayMessage = WM_APP + 1, StatusMessage = WM_APP + 2;
constexpr wchar_t WindowClass[] = L"380TaxiCamera.Settings";
constexpr wchar_t DonationUrl[] = L"https://www.paypal.com/donate/?hosted_button_id=EPVELD44P6NXW";
constexpr wchar_t GithubUrl[] = L"https://github.com/JoelScripts/A350-Improvements";
constexpr COLORREF Background = RGB(6, 12, 19), Sidebar = RGB(9, 17, 27), Card = RGB(16, 27, 40), Border = RGB(39, 59, 78),
                   Text = RGB(232, 240, 247), Muted = RGB(132, 154, 174), Accent = RGB(57, 211, 224);
constexpr COLORREF Amber = RGB(245, 181, 70), Panel2 = RGB(12, 22, 34), Deep = RGB(4, 9, 15);
HINSTANCE instance{};
HWND window{}, sidebar_tooltip{}, shortcut_window{};
HFONT normal{}, small{}, title_font{}, heading{}, version_font{};
HBRUSH background_brush{}, card_brush{};
HICON icon{};
UINT dpi = 96, taskbar_created{};
int page = 0;
std::vector<HWND> controls;
std::vector<HWND> navigation;
std::vector<std::uint64_t> combo_ids;
native_camera::AutoProfileSelection profile_selection;
std::wstring installation, expected_simulator, notice = L"Changes are saved for this aircraft.";
std::mutex app_mutex;
win::Settings current;
win::Status status;
bool received_bridge_status{};  // Guarded by app_mutex; retained across simulator sessions.
std::wstring connection = L"Waiting for Microsoft Flight Simulator 2024";
std::atomic<bool> running{true};
std::atomic<DWORD> simulator_pid{};
HANDLE worker{}, show_event{}, singleton{};
bool dirty = false, refreshing = false, background_start = false, preview_ui = false;
std::atomic<bool> auto_connect{true};
struct GithubIssue {
  int number{};
  bool open{};
  bool pull_request{};
  bool in_progress{};
  std::string title;
  std::string url;
  std::string labels;
  std::string updated;
  std::string body;
};
std::vector<GithubIssue> github_issues;
std::wstring github_issue_error;
std::vector<RECT> github_issue_cards;
constexpr int GithubRefreshId = 701;
constexpr int GithubOpenId = 702;
constexpr int GithubReportId = 703;
std::atomic<bool> connection_requested{}, connection_disconnected{};
win::ConnectCommandQueue connect_commands;
win::CameraHotkeys hotkey_draft = win::DefaultCameraHotkeys, hotkey_saved = win::DefaultCameraHotkeys;
win::CameraHotkeyRegistration hotkey_registration;
bool hotkey_editor_focused{}, hotkeys_closing{};
win::Updater updater;
struct DiagnosticWatchState {
  std::uint64_t last_heartbeat{};
  std::uint64_t last_event_ms{};
  std::uint64_t last_progress_event_ms{};
  std::uint64_t last_taxi_mask{};
  std::uint64_t last_captures{};
  std::uint64_t last_stamps{};
  bool initialized{};
  bool stale_reported{};
};
DiagnosticWatchState diagnostic_watch;
void service_diagnostics_watch(const win::Status& sample) noexcept;
ULONGLONG next_update_check{};
bool update_prompt{};
int scale(int v) {
  return MulDiv(v, static_cast<int>(dpi), 96);
}
RECT rectangle(int x, int y, int w, int h) {
  return {scale(x), scale(y), scale(x + w), scale(y + h)};
}
void text(HDC dc,
          const wchar_t* value,
          int x,
          int y,
          int w,
          int h,
          HFONT font,
          COLORREF color = Text,
          UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
  auto r = rectangle(x, y, w, h);
  SelectObject(dc, font);
  SetTextColor(dc, color);
  SetBkMode(dc, TRANSPARENT);
  DrawTextW(dc, value, -1, &r, flags);
}
void panel(HDC dc, int x, int y, int w, int h, COLORREF color = Card) {
  HBRUSH brush = CreateSolidBrush(color);
  HPEN pen = CreatePen(PS_SOLID, 1, Border);
  const auto oldb = SelectObject(dc, brush), oldp = SelectObject(dc, pen);
  RoundRect(dc, scale(x), scale(y), scale(x + w), scale(y + h), scale(14), scale(14));
  SelectObject(dc, oldb);
  SelectObject(dc, oldp);
  DeleteObject(brush);
  DeleteObject(pen);
}
HWND child(const wchar_t* type, const wchar_t* label, int id, int x, int y, int w, int h, DWORD style = 0) {
  HWND value = CreateWindowExW(0, type, label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, scale(x), scale(y), scale(w), scale(h), window,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
  SendMessageW(value, WM_SETFONT, reinterpret_cast<WPARAM>(normal), TRUE);
  SetWindowTheme(value, L"DarkMode_Explorer", nullptr);
  controls.push_back(value);
  return value;
}
HWND button(const wchar_t* label, int id, int x, int y, int w = 130, int h = 36) {
  return child(L"BUTTON", label, id, x, y, w, h, BS_OWNERDRAW);
}
void draw_bug_icon(HDC dc, const RECT& bounds, COLORREF color) {
  const int x = (bounds.left + bounds.right) / 2 - scale(20), y = (bounds.top + bounds.bottom) / 2 - scale(20);
  HPEN pen = CreatePen(PS_SOLID, scale(2), color);
  const auto old_pen = SelectObject(dc, pen), old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
  const auto line = [&](int x1, int y1, int x2, int y2) {
    MoveToEx(dc, x + scale(x1), y + scale(y1), nullptr);
    LineTo(dc, x + scale(x2), y + scale(y2));
  };
  line(17, 10, 14, 6);
  line(23, 10, 26, 6);
  Ellipse(dc, x + scale(16), y + scale(9), x + scale(24), y + scale(17));
  Ellipse(dc, x + scale(13), y + scale(14), x + scale(27), y + scale(31));
  line(20, 15, 20, 30);
  for (const bool right : {false, true}) {
    const auto side = [&](int coordinate) { return right ? 40 - coordinate : coordinate; };
    line(side(13), 18, side(8), 15);
    line(side(13), 23, side(6), 23);
    line(side(14), 28, side(8), 32);
  }
  SelectObject(dc, old_brush);
  SelectObject(dc, old_pen);
  DeleteObject(pen);
}
void edit(double value, int id, int x, int y, int w = 110) {
  wchar_t buffer[64];
  std::swprintf(buffer, 64, id >= 360 && id <= 367 ? L"%.1f" : id == 201 || id == 202 ? L"%.4g" : L"%.10g", value);
  auto h = child(L"EDIT", buffer, id, x, y, w, 30, ES_AUTOHSCROLL | ES_LEFT | WS_BORDER);
  SendMessageW(h, EM_SETLIMITTEXT, 32, 0);
}
void toggle(const wchar_t* label, int id, bool enabled, int x, int y, int width = 125) {
  const auto text = std::wstring(label) + (enabled ? L": On" : L": Off");
  button(text.c_str(), id, x, y, width);
}
void make_fonts() {
  for (HFONT f : {normal, small, title_font, heading, version_font})
    if (f)
      DeleteObject(f);
  auto make = [](int size, int weight, bool underline = false) {
    return CreateFontW(-scale(size), 0, 0, 0, weight, FALSE, underline, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable");
  };
  normal = make(15, FW_NORMAL);
  small = make(13, FW_NORMAL);
  title_font = make(30, FW_SEMIBOLD);
  heading = make(18, FW_SEMIBOLD);
  version_font = make(13, FW_NORMAL, true);
}
std::wstring widen(const char* input) {
  if (!input)
    return {};
  const size_t count = strnlen(input, 384);
  if (!count)
    return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, input, static_cast<int>(count), nullptr, 0);
  std::wstring output(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, input, static_cast<int>(count), output.data(), n);
  return output;
}
win::Settings draft() {
  const std::lock_guard lock(app_mutex);
  return current;
}
void publish(const win::Settings& value) {
  const std::lock_guard lock(app_mutex);
  const auto enabled = current.enabled;
  const auto profile_request = std::max(current.profile_request, value.profile_request);
  current = value;
  current.enabled = enabled;  // Profile/settings edits cannot override connection state.
  current.profile_request = profile_request;
}
void refresh_connection_button() {
  SetDlgItemTextW(window, 241, win::connection_button_label(connection_requested.load(std::memory_order_acquire)));
}
void request_connection(win::ConnectCommand command) {
  {
    const std::lock_guard lock(app_mutex);
    const bool disconnect = command == win::ConnectCommand::disconnect;
    if (!disconnect && !win::begin_connection(current)) {
      notice = L"Restart Taxi Cam to begin a new connection.";
      InvalidateRect(window, nullptr, FALSE);
      return;
    }
    connection_disconnected.store(disconnect, std::memory_order_release);
    connection_requested.store(!disconnect, std::memory_order_release);
    if (disconnect)
      win::apply_connection_command(current, command);
    if (disconnect) {
      status = {};
      connection = L"Disconnected. Choose Connect to enable the cameras again.";
      // Publish the stop immediately, including when the attach worker is still
      // waiting for a remote load. The worker uses the same settings lock.
      win::Mailbox mailbox;
      if (const auto pid = simulator_pid.load(); pid && mailbox.open(pid, false) && mailbox.lock(200)) {
        mailbox.data()->settings = current;
        mailbox.data()->owner_heartbeat = 0;
        mailbox.unlock();
      }
    }
  }
  connect_commands.request(command);
  notice = command == win::ConnectCommand::disconnect ? L"Disconnected. Camera output and temporary requests are off."
                                                      : L"Connect requested. Cameras will enable when the bridge is ready.";
  refresh_connection_button();
  if (command == win::ConnectCommand::disconnect) {
    SetDlgItemTextW(window, 224, L"Left preview: Off");
    SetDlgItemTextW(window, 225, L"Right preview: Off");
    SetDlgItemTextW(window, 226, L"Calibrate left: Off");
    SetDlgItemTextW(window, 227, L"Calibrate right: Off");
    SetDlgItemTextW(window, 229, L"Scene test: Off");
  }
  InvalidateRect(window, nullptr, FALSE);
}
void toggle_connection() {
  request_connection(connection_requested.load(std::memory_order_acquire) ? win::ConnectCommand::disconnect : win::ConnectCommand::connect);
}
bool exchange_control(win::Mailbox& mailbox, win::Status* sample = nullptr) {
  // Serialize publication with Connect/Disconnect so a previously copied
  // enabled setting cannot be written after the user has disconnected.
  const std::lock_guard lock(app_mutex);
  if (!mailbox.data() || !mailbox.lock(100))
    return false;
  mailbox.data()->settings = current;
  mailbox.data()->owner_pid = GetCurrentProcessId();
  mailbox.data()->owner_heartbeat = connection_requested.load(std::memory_order_acquire) ? GetTickCount64() : 0;
  if (sample)
    *sample = mailbox.data()->status;
  mailbox.unlock();
  return true;
}

std::string json_number_field(std::string_view object, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const auto pos = object.find(needle);
  if (pos == std::string_view::npos) return {};
  size_t i = pos + needle.size();
  while (i < object.size() && (object[i] == ' ' || object[i] == '\n' || object[i] == '\r' || object[i] == '\t')) ++i;
  const size_t start = i;
  while (i < object.size() && ((object[i] >= '0' && object[i] <= '9') || object[i] == '-')) ++i;
  return std::string(object.substr(start, i - start));
}

std::string json_string_field(std::string_view object, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const auto pos = object.find(needle);
  if (pos == std::string_view::npos) return {};
  size_t i = pos + needle.size();
  while (i < object.size() && (object[i] == ' ' || object[i] == '\n' || object[i] == '\r' || object[i] == '\t')) ++i;
  if (i >= object.size() || object[i] != '\"') return {};
  ++i;
  std::string out;
  bool escape = false;
  for (; i < object.size(); ++i) {
    const char c = object[i];
    if (escape) {
      switch (c) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case '\\': out += '\\'; break;
        case '\"': out += '\"'; break;
        default: out += c; break;
      }
      escape = false;
    } else if (c == '\\') {
      escape = true;
    } else if (c == '\"') {
      break;
    } else {
      out += c;
    }
  }
  return out;
}

bool json_has_key(std::string_view object, std::string_view key) {
  return object.find("\"" + std::string(key) + "\":") != std::string_view::npos;
}

std::vector<std::string_view> json_issue_objects(std::string_view json) {
  std::vector<std::string_view> result;
  bool in_string = false, escape = false;
  int depth = 0;
  size_t start = std::string_view::npos;
  for (size_t i = 0; i < json.size(); ++i) {
    const char c = json[i];
    if (in_string) {
      if (escape) escape = false;
      else if (c == '\\') escape = true;
      else if (c == '\"') in_string = false;
      continue;
    }
    if (c == '\"') { in_string = true; continue; }
    if (c == '{') {
      if (depth++ == 0) start = i;
    } else if (c == '}' && depth > 0) {
      if (--depth == 0 && start != std::string_view::npos) {
        result.push_back(json.substr(start, i - start + 1));
        start = std::string_view::npos;
      }
    }
  }
  return result;
}

std::string github_get_issues(std::wstring* error) {
  if (error) error->clear();
  const std::wstring host = L"api.github.com";
  const std::wstring path = L"/repos/JoelScripts/A350-Improvements/issues?state=open&per_page=50&sort=updated&direction=desc";
  HINTERNET session = WinHttpOpen(L"TaxiCam-Fixed/0.9.37", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) { if (error) *error = L"GitHub connection could not be started."; return {}; }
  WinHttpSetTimeouts(session, 2500, 2500, 5000, 5000);
  HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!connect) { if (error) *error = L"Could not connect to GitHub."; WinHttpCloseHandle(session); return {}; }
  HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!request) { if (error) *error = L"Could not create the GitHub request."; WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return {}; }
  const wchar_t headers[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2026-03-10\r\nUser-Agent: TaxiCam-Fixed\r\n";
  const bool sent = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(request, nullptr);
  DWORD status_code = 0, status_size = sizeof(status_code);
  if (sent) WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);
  std::string body;
  if (sent && status_code == 200) {
    for (;;) {
      DWORD available = 0;
      if (!WinHttpQueryDataAvailable(request, &available) || !available) break;
      std::string chunk(available, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(request, chunk.data(), available, &read) || !read) break;
      chunk.resize(read);
      body += chunk;
      if (body.size() > 2 * 1024 * 1024) break;
    }
  }
  WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
  if (!sent || status_code != 200 || body.empty()) {
    if (error) {
      wchar_t message[128];
      std::swprintf(message, 128, L"GitHub returned HTTP %lu.", static_cast<unsigned long>(status_code));
      *error = sent ? message : L"GitHub could not be reached. Check your internet connection.";
    }
    return {};
  }
  return body;
}

void refresh_github_issues(bool notify = true) {
  std::wstring error;
  const auto json = github_get_issues(&error);
  if (json.empty()) {
    github_issue_error = error.empty() ? L"No issue data was returned by GitHub." : error;
    if (notify) notice = L"Could not refresh GitHub bug reports.";
    InvalidateRect(window, nullptr, FALSE);
    return;
  }
  std::vector<GithubIssue> next;
  for (const auto object : json_issue_objects(json)) {
    if (json_has_key(object, "pull_request")) continue;
    GithubIssue issue;
    const auto number = json_number_field(object, "number");
    try { issue.number = std::stoi(number); } catch (...) { issue.number = 0; }
    issue.open = json_string_field(object, "state") == "open";
    issue.title = json_string_field(object, "title");
    issue.url = json_string_field(object, "html_url");
    issue.updated = json_string_field(object, "updated_at");
    issue.body = json_string_field(object, "body");
    const auto labels_start = object.find("\"labels\":[");
    if (labels_start != std::string_view::npos) {
      const auto end = object.find(']', labels_start);
      if (end != std::string_view::npos) {
        const auto labels_json = object.substr(labels_start, end - labels_start + 1);
        for (const auto label : json_issue_objects(labels_json)) {
          const auto name = json_string_field(label, "name");
          if (!name.empty()) {
            if (!issue.labels.empty()) issue.labels += " • ";
            issue.labels += name;
            std::string lower = name;
            for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lower.find("progress") != std::string::npos || lower == "in-progress" || lower == "in progress") issue.in_progress = true;
          }
        }
      }
    }
    if (issue.number > 0 && !issue.title.empty() && !issue.url.empty()) next.push_back(std::move(issue));
  }
  std::stable_sort(next.begin(), next.end(), [](const GithubIssue& a, const GithubIssue& b) {
    if (a.open != b.open) return a.open > b.open;
    return a.updated > b.updated;
  });
  github_issues = std::move(next);
  github_issue_error.clear();
  if (notify) notice = L"GitHub bug reports refreshed.";
  InvalidateRect(window, nullptr, FALSE);
}
void open_github_issue(int index) {
  if (index < 0 || index >= static_cast<int>(github_issues.size())) return;
  const auto result = reinterpret_cast<INT_PTR>(ShellExecuteA(window, "open", github_issues[index].url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  if (result <= 32) notice = L"Could not open the GitHub issue in your browser.";
}

void open_diagnostic_file(const std::wstring& path, const wchar_t* label) {
  // Re-initialize defensively so the companion always has the real diagnostics
  // directory/path set before a button is used, even if the bridge has not
  // created any diagnostics files yet.
  win::advanced_diagnostics::initialize("companion");
  const auto& target = path.empty() ? win::advanced_diagnostics::summary_path : path;
  if (target.empty()) {
    notice = std::wstring(L"Could not locate ") + label + L". Diagnostics path is empty.";
    InvalidateRect(window, nullptr, FALSE);
    return;
  }

  DWORD attributes = GetFileAttributesW(target.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    // The diagnostics bootstrap normally creates these files. If an older
    // installation has not done so yet, force a fresh state snapshot where
    // possible and check once more before reporting failure.
    win::Status sample{};
    win::advanced_diagnostics::state_snapshot(sample, "companion_file_open");
    attributes = GetFileAttributesW(target.c_str());
  }

  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
    notice = std::wstring(L"The ") + label + L" does not exist yet. Log folder: " +
             win::advanced_diagnostics::directory;
    InvalidateRect(window, nullptr, FALSE);
    return;
  }

  // First try the user's normal file association. If Windows has no
  // association (common for .jsonl), fall back to Explorer selecting the
  // exact file so the button always produces a visible result.
  auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  if (result <= 32) {
    std::wstring args = L"/select,\"" + target + L"\"";
    result = reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL));
  }

  if (result <= 32) {
    notice = std::wstring(L"Could not open ") + label + L". Try Open log folder.";
    win::advanced_diagnostics::event("diagnostics.file_open_failed", std::string("Failed to open diagnostic file"), "companion");
  } else {
    notice = std::wstring(L"Opened ") + label + L".";
    win::advanced_diagnostics::event("diagnostics.file_opened", std::string("Opened diagnostic file"), "companion");
  }
  InvalidateRect(window, nullptr, FALSE);
}

void donate() {
  const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", DonationUrl, nullptr, nullptr, SW_SHOWNORMAL));
  if (result <= 32)
    MessageBoxW(window, L"Could not open your browser. You can also find the PayPal donation link in the Taxi Cam README.",
                L"Donate to Taxi Cam", MB_OK | MB_ICONWARNING);
}
void report_bug() {
  win::BugReportContext context;
  {
    const std::lock_guard lock(app_mutex);
    context.settings = current;
    context.status = status;
    context.bridge_seen = received_bridge_status;
    context.simulator_running = simulator_pid.load() != 0;
  }
  context.now = GetTickCount64();
  context.ui_preview = preview_ui;
  context.unsaved_edits = dirty;
  const auto url = win::bug_report_url(context);
  const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  if (result <= 32) {
    MessageBoxW(window,
                L"Could not open your browser. Open github.com/rthoms334/taxi-cam/issues and choose Bug report. "
                L"Attach logs from Diagnostics > Open log folder and include your Taxi Cam version.",
                L"Taxi Cam bug report", MB_OK | MB_ICONWARNING);
    return;
  }
  notice = L"Bug report opened. Review the details and attach logs before submitting on GitHub.";
  InvalidateRect(window, nullptr, FALSE);
}
void dirty_notice() {
  dirty = true;
  notice = L"Unsaved changes";
  InvalidateRect(window, nullptr, FALSE);
}
bool apply_color_selection(const win::Settings& expected, bool markings, COLORREF color) {
  {
    const std::lock_guard lock(app_mutex);
    // The modal picker pumps status messages, which can select another aircraft
    // or restart this profile. Its result belongs only to the opening session.
    if (current.profile != expected.profile || current.profile_request != expected.profile_request)
      return false;
    auto& target = markings ? current.guide_color : current.speed_color;
    target = {GetRValue(color) / 255.f, GetGValue(color) / 255.f, GetBValue(color) / 255.f};
  }
  dirty_notice();
  return true;
}
double number(int id, double previous, bool& ok) {
  auto control = GetDlgItem(window, id);
  if (!control)
    return previous;
  wchar_t value[64];
  GetWindowTextW(control, value, 64);
  wchar_t* end{};
  const double n = std::wcstod(value, &end);
  if (end == value || *end || !std::isfinite(n)) {
    ok = false;
    return previous;
  }
  return n;
}
bool read_fields(win::Settings& settings, const wchar_t** error = nullptr) {
  if (error)
    *error = nullptr;
  bool ok = true;
  const double rate = number(200, settings.camera_rate, ok);
  if (rate < kMinimumCameraRate || rate > kMaximumCameraRate || std::floor(rate) != rate)
    ok = false;
  if (ok)
    settings.camera_rate = static_cast<UINT>(rate);
  const double budget = number(203, settings.calibration_budget, ok);
  if (budget < 64 || budget > 16384 || std::floor(budget) != budget)
    ok = false;
  if (ok)
    settings.calibration_budget = static_cast<UINT>(budget);
  settings.exposure = static_cast<float>(number(201, settings.exposure, ok));
  settings.night_boost = static_cast<float>(number(202, settings.night_boost, ok));
  for (unsigned i = 0; i < 2; ++i)
    for (unsigned j = 0; j < 6; ++j)
      settings.mounts[i][j] = number(300 + static_cast<int>(i * 10 + j), settings.mounts[i][j], ok);
  std::array<float, 2>* guides[]{&settings.nose_dot, &settings.tail_upper, &settings.tail_corner, &settings.tail_inner};
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned axis = 0; axis < 2; ++axis) {
      const auto value = number(360 + static_cast<int>(i * 2 + axis), (*guides[i])[axis] * 100., ok);
      if (value < 0 || value > (axis ? 100 : 50))
        ok = false;
      else
        (*guides[i])[axis] = static_cast<float>(value / 100.);
    }
  if (page == 3) {
    std::array<std::uint64_t, 2> selected_ids{settings.left_id, settings.right_id};
    for (unsigned i = 0; i < 2; ++i) {
      const LRESULT selected = SendDlgItemMessageW(window, 400 + i, CB_GETCURSEL, 0, 0);
      if (selected == 0)
        selected_ids[i] = 0;
      else if (selected > 0 && static_cast<size_t>(selected - 1) < combo_ids.size())
        selected_ids[i] = combo_ids[selected - 1];
    }
    const auto result =
        win::update_target_assignment(settings.left_id, settings.right_id, settings.route_request, selected_ids[0], selected_ids[1]);
    if (result == win::TargetAssignmentResult::duplicate || result == win::TargetAssignmentResult::sequence_exhausted) {
      if (error)
        *error = result == win::TargetAssignmentResult::duplicate
                     ? L"Choose different textures for left and right, or Automatic assignment."
                     : L"Display assignment request limit reached. Restart Taxi Cam.";
      return false;
    }
  }
  return ok && win::valid_settings(settings);
}
void build_controls();
void refresh_shortcut_status() {
  if (!shortcut_window)
    return;
  for (unsigned i = 0; i < 3; ++i) {
    const auto state = hotkey_draft[i] != hotkey_saved[i] ? std::wstring(L"Unsaved — select Save changes to apply")
                       : hotkey_editor_focused            ? std::wstring(L"Editing — shortcuts paused until you leave the field")
                                                          : hotkey_registration.status(i);
    SetDlgItemTextW(shortcut_window, 650 + i, state.c_str());
  }
}
void register_camera_hotkeys() {
  if (hotkey_editor_focused || hotkeys_closing)
    hotkey_registration.clear();
  else
    hotkey_registration.configure(window, hotkey_saved, preview_ui);
}
LRESULT CALLBACK shortcut_editor(HWND control, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR) {
  if (message == WM_SETFOCUS) {
    // RegisterHotKey consumes its chord before the native editor sees it. Release
    // our bindings while editing so existing shortcuts can be captured as well.
    hotkey_editor_focused = true;
    hotkey_registration.clear();
    refresh_shortcut_status();
  } else if (message == WM_KILLFOCUS) {
    const auto next = reinterpret_cast<HWND>(w);
    const int next_id = next && GetParent(next) == shortcut_window ? GetDlgCtrlID(next) : 0;
    hotkey_editor_focused = next_id >= 620 && next_id <= 622;
    register_camera_hotkeys();
    refresh_shortcut_status();
  } else if (message == WM_NCDESTROY) {
    RemoveWindowSubclass(control, shortcut_editor, id);
  }
  return DefSubclassProc(control, message, w, l);
}
struct ShortcutDialogTemplate {
  DLGTEMPLATE dialog{WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME, WS_EX_DLGMODALFRAME, 0, 0, 0, 450, 260};
  WORD menu{}, window_class{}, title{};
};
INT_PTR CALLBACK shortcut_dialog(HWND hwnd, UINT message, WPARAM w, LPARAM) {
  if (message == WM_INITDIALOG) {
    shortcut_window = hwnd;
    hotkey_draft = hotkey_saved;
    SetWindowTextW(hwnd, L"Taxi Cam — Flight-deck keyboard shortcuts");
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    RECT bounds{0, 0, scale(680), scale(460)}, owner{};
    AdjustWindowRectExForDpi(&bounds, WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME, FALSE, WS_EX_DLGMODALFRAME, dpi);
    GetWindowRect(window, &owner);
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    SetWindowPos(hwnd, nullptr, owner.left + (owner.right - owner.left - width) / 2, owner.top + (owner.bottom - owner.top - height) / 2,
                 width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    const auto make = [&](const wchar_t* type, const wchar_t* label, int id, int x, int y, int width, int height, DWORD style = 0,
                          HFONT font = nullptr) {
      HWND control = CreateWindowExW(0, type, label, WS_CHILD | WS_VISIBLE | style, scale(x), scale(y), scale(width), scale(height), hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
      SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : normal), TRUE);
      SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
      return control;
    };
    make(L"STATIC", L"Flight-deck keyboard shortcuts", -1, 20, 17, 640, 28, 0, heading);
    make(L"STATIC", L"Use Ctrl or Alt with a letter, number or function key. Clear disables a shortcut.", -1, 20, 51, 640, 27, 0, small);
    for (unsigned i = 0; i < 3; ++i) {
      const int y = 90 + static_cast<int>(i) * 90;
      make(L"STATIC", win::CameraHotkeyNames[i], -1, 20, y + 5, 190, 26);
      auto field = make(HOTKEY_CLASSW, L"", 620 + i, 220, y, 330, 32, WS_TABSTOP | WS_BORDER);
      SendMessageW(field, HKM_SETHOTKEY, win::hotkey_control_value(hotkey_draft[i]), 0);
      SetWindowSubclass(field, shortcut_editor, 1, 0);
      make(L"BUTTON", L"Clear", 630 + i, 568, y, 92, 32, WS_TABSTOP | BS_PUSHBUTTON);
      make(L"STATIC", L"", 650 + i, 20, y + 40, 640, 25, 0, small);
    }
    make(L"STATIC",
         L"Both turns both displays on; press again to turn both off.\nShortcuts apply to all aircraft and work while Taxi Cam is hidden.",
         660, 20, 350, 640, 45, 0, small);
    make(L"BUTTON", L"Reset shortcuts", 640, 20, 407, 165, 34, WS_TABSTOP | BS_PUSHBUTTON);
    make(L"BUTTON", L"Save changes", IDOK, 400, 407, 145, 34, WS_TABSTOP | BS_DEFPUSHBUTTON);
    make(L"BUTTON", L"Close", IDCANCEL, 562, 407, 98, 34, WS_TABSTOP | BS_PUSHBUTTON);
    refresh_shortcut_status();
    return TRUE;
  }
  if (message == WM_CTLCOLORDLG || message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT) {
    const auto dc = reinterpret_cast<HDC>(w);
    SetTextColor(dc, Text);
    SetBkColor(dc, Card);
    return reinterpret_cast<INT_PTR>(card_brush);
  }
  if (message == WM_COMMAND) {
    const int id = LOWORD(w);
    if (id >= 620 && id <= 622 && HIWORD(w) == EN_CHANGE) {
      hotkey_draft[id - 620] = win::hotkey_from_control(static_cast<WORD>(SendDlgItemMessageW(hwnd, id, HKM_GETHOTKEY, 0, 0)));
      refresh_shortcut_status();
      return TRUE;
    }
    if ((id >= 630 && id <= 632) || id == 640) {
      if (id == 640)
        hotkey_draft = win::DefaultCameraHotkeys;
      else
        hotkey_draft[id - 630] = {};
      for (unsigned i = 0; i < 3; ++i)
        SendDlgItemMessageW(hwnd, 620 + i, HKM_SETHOTKEY, win::hotkey_control_value(hotkey_draft[i]), 0);
      refresh_shortcut_status();
      return TRUE;
    }
    if (id == IDOK) {
      std::wstring error;
      if (!win::valid_camera_hotkeys(hotkey_draft, &error)) {
        SetDlgItemTextW(hwnd, 660, error.c_str());
        return TRUE;
      }
      if (!win::save_camera_hotkeys(hotkey_draft, win::settings_directory())) {
        SetDlgItemTextW(hwnd, 660, L"Could not save shortcuts. Check access to the local settings folder.");
        return TRUE;
      }
      hotkey_saved = hotkey_draft;
      register_camera_hotkeys();
      refresh_shortcut_status();
      SetDlgItemTextW(hwnd, 660,
                      hotkey_registration.conflicts()
                          ? L"Saved. Unavailable shortcuts need a different combination. The other shortcuts remain active."
                          : L"Shortcuts saved for all aircraft. Camera settings and unfinished edits are unchanged.");
      return TRUE;
    }
    if (id == IDCANCEL) {
      EndDialog(hwnd, IDCANCEL);
      return TRUE;
    }
  }
  if (message == WM_CLOSE) {
    EndDialog(hwnd, IDCANCEL);
    return TRUE;
  }
  if (message == WM_DESTROY) {
    shortcut_window = nullptr;
    hotkey_editor_focused = false;
    hotkey_draft = hotkey_saved;
    register_camera_hotkeys();
  }
  return FALSE;
}
void edit_camera_hotkeys() {
  const ShortcutDialogTemplate layout;
  if (DialogBoxIndirectParamW(instance, &layout.dialog, window, shortcut_dialog, 0) == -1) {
    notice = L"Could not open the keyboard shortcut editor.";
    InvalidateRect(window, nullptr, FALSE);
  }
}
bool apply(bool save = true) {
  auto settings = draft();
  const wchar_t* field_error{};
  if (!read_fields(settings, &field_error)) {
    notice = field_error ? field_error
             : page == 5 ? L"Guide X must be 0–50%; Y must be 0–100%. Enter finite numbers."
                         : L"Check the values: rate 5–60 (min 5), EV −16 to +4, lens 0.05–1.55.";
    InvalidateRect(window, nullptr, FALSE);
    return false;
  }
  if (save && !win::save_settings(settings)) {
    notice = L"Could not save settings. Check access to your local settings folder.";
    InvalidateRect(window, nullptr, FALSE);
    return false;
  }
  publish(settings);
  dirty = false;
  notice = save ? L"Saved. Adjustments apply while the cameras are running." : L"Settings updated.";
  if (save && hotkey_registration.conflicts())
    notice = L"Saved. Some shortcuts are unavailable; check Overview > Flight-deck control.";
  InvalidateRect(window, nullptr, FALSE);
  return true;
}
void target_combos(const win::Settings& s) {
  win::Status sample;
  {
    const std::lock_guard lock(app_mutex);
    sample = status;
  }
  // A dropped list belongs to the user until it closes. Do not replace its
  // item order while it is being selected or turn a redraw into a selection.
  for (unsigned side = 0; side < 2; ++side)
    if (SendDlgItemMessageW(window, 400 + side, CB_GETDROPPEDSTATE, 0, 0))
      return;
  std::vector<std::uint64_t> next;
  for (UINT i = 0; i < std::min(sample.candidate_count, 16u); ++i)
    next.push_back(sample.candidates[i].id);
  std::sort(next.begin(), next.end());
  const bool existing = GetDlgItem(window, 400) != nullptr;
  if (existing && next == combo_ids)
    return;
  std::array<std::uint64_t, 2> selected{s.left_id, s.right_id};
  if (existing) {
    for (unsigned side = 0; side < 2; ++side) {
      const auto index = SendDlgItemMessageW(window, 400 + side, CB_GETCURSEL, 0, 0);
      selected[side] = index > 0 && size_t(index - 1) < combo_ids.size() ? combo_ids[index - 1] : 0;
    }
  }
  combo_ids = std::move(next);
  const bool was_refreshing = refreshing;
  refreshing = true;
  for (unsigned side = 0; side < 2; ++side) {
    HWND combo = GetDlgItem(window, 400 + side);
    if (!combo)
      combo = child(L"COMBOBOX", L"", 400 + side, 260 + static_cast<int>(side) * 375, 237, 315, 240, CBS_DROPDOWNLIST | WS_VSCROLL);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Automatic assignment"));
    SendMessageW(combo, CB_SETDROPPEDWIDTH, scale(420), 0);
    int selection = 0;
    for (size_t i = 0; i < combo_ids.size(); ++i) {
      const win::Candidate* candidate = nullptr;
      for (UINT j = 0; j < std::min(sample.candidate_count, 16u); ++j)
        if (sample.candidates[j].id == combo_ids[i])
          candidate = &sample.candidates[j];
      wchar_t name[96];
      std::swprintf(name, 96, L"#%llu | %ux%u | %u mips | format %u", static_cast<unsigned long long>(combo_ids[i]),
                    candidate ? candidate->width : 0, candidate ? candidate->height : 0, candidate ? candidate->mips : 0,
                    candidate ? candidate->format : 0);
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
      if (combo_ids[i] == selected[side])
        selection = static_cast<int>(i + 1);
    }
    SendMessageW(combo, CB_SETCURSEL, selection, 0);
  }
  refreshing = was_refreshing;
}
// Runs on the UI thread, including when hidden to the tray.
void sync_aircraft_session() {
  auto s = draft();
  win::Status sample;
  {
    const std::lock_guard lock(app_mutex);
    sample = status;
  }
  const auto now = GetTickCount64();
  if (!sample.heartbeat || now < sample.heartbeat || now - sample.heartbeat > 3000 ||
      sample.aircraft_session_epoch == s.aircraft_session_epoch)
    return;
  win::reset_aircraft_session(s, sample.aircraft_session_epoch);
  publish(s);
  profile_selection = {};
  // Do not rebuild numeric edits when a flight changes in the background.
  for (unsigned side = 0; side < 2; ++side)
    SendDlgItemMessageW(window, 400 + side, CB_SETCURSEL, 0, 0);
  for (const auto& label : std::array<std::pair<int, const wchar_t*>, 5>{{{224, L"Left preview: Off"},
                                                                          {225, L"Right preview: Off"},
                                                                          {226, L"Calibrate left: Off"},
                                                                          {227, L"Calibrate right: Off"},
                                                                          {229, L"Scene test: Off"}}})
    SetDlgItemTextW(window, label.first, label.second);
}
void toggle_camera_from_hotkey(unsigned action) {
  // Keep unfinished numeric edits in their controls. A global shortcut must not
  // run Apply or rebuild the page, even when the companion is hidden.
  sync_aircraft_session();
  auto s = draft();
  win::Status sample;
  {
    const std::lock_guard lock(app_mutex);
    sample = status;
  }
  const auto now = GetTickCount64();
  const auto result = win::request_camera_hotkey(s, action, sample, now);
  if (result == win::CameraHotkeyResult::unavailable) {
    notice = s.enabled ? L"Waiting for current aircraft TAXI-button state. Try the shortcut again when connected."
                       : L"Choose Connect before using aircraft camera shortcuts.";
    InvalidateRect(window, nullptr, FALSE);
    return;
  }
  publish(s);
  dirty_notice();
  notice = L"Camera request: left " + std::wstring(s.manual_mask & 1 ? L"on" : L"off") + L", right " +
           (s.manual_mask & 2 ? L"on" : L"off") + L".";
  if (!s.enabled)
    notice = L"Camera request updated. Choose Connect to enable camera output.";
  for (unsigned side = 0; side < 2; ++side) {
    const auto label = std::wstring(side ? L"Right preview: " : L"Left preview: ") + (s.manual_mask & (1u << side) ? L"On" : L"Off");
    SetDlgItemTextW(window, 224 + side, label.c_str());
    SetDlgItemTextW(window, 226 + side, side ? L"Calibrate right: Off" : L"Calibrate left: Off");
  }
  SetDlgItemTextW(window, 221, s.follow_taxi ? L"TAXI buttons: On" : L"TAXI buttons: Off");
  SetDlgItemTextW(window, 229, L"Scene test: Off");
  InvalidateRect(window, nullptr, FALSE);
}
void auto_profile() {
  const auto s = draft();
  if (!s.auto_profile) {
    profile_selection = {};
    return;
  }
  win::Status sample;
  {
    const std::lock_guard lock(app_mutex);
    sample = status;
  }
  const auto now = GetTickCount64();
  if (!sample.heartbeat || now < sample.heartbeat || now - sample.heartbeat > 3000)
    return;
  const auto detected = profile_selection.observe(sample.detected_profile, sample.identity_sample_ms);
  if (!detected || detected == s.profile)
    return;
  // Preserve edits on the departing profile; invalid unfinished fields delay
  // switching instead of silently discarding the user's calibration.
  if (!apply())
    return;
  win::Settings next;
  if (!win::load_settings(next, installation, detected))
    return;
  next.enabled = s.enabled;
  if (!win::prepare_profile_selection(next, s, true) || !win::save_settings(next))
    return;
  publish(next);
  notice = L"Aircraft detected. Its saved calibration is active.";
  build_controls();
}
void build_controls() {
  win::advanced_diagnostics::initialize("companion");
  refreshing = true;
  if (sidebar_tooltip) {
    DestroyWindow(sidebar_tooltip);
    sidebar_tooltip = nullptr;
  }
  for (HWND h : controls)
    DestroyWindow(h);
  controls.clear();
  navigation.clear();
  const auto s = draft();
  const wchar_t* names[]{L"Overview", L"Camera views", L"Display", L"PFD routing", L"Diagnostics", L"Reference guides", L"Bug Reports"};
  for (int i = 0; i < 7; ++i)
    navigation.push_back(button(names[i], 100 + i, 235 + i * 108, 86, 100, 34));
  const auto donate_button = button(L"Donate", 513, 1115, 725, 110, 34);
  const auto report_button = button(L"Report a bug", 512, 1235, 725, 145, 34);
  const auto version_link = button(L"v" TAXI_CAM_VERSION_WIDE, 514, 1115, 765, 100, 22);
  SendMessageW(version_link, WM_SETFONT, reinterpret_cast<WPARAM>(version_font), TRUE);
  sidebar_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, window, nullptr, instance, nullptr);
  if (sidebar_tooltip) {
    TOOLINFOW tip{};
    tip.cbSize = sizeof(tip);
    tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tip.hwnd = window;
    tip.uId = reinterpret_cast<UINT_PTR>(report_button);
    tip.lpszText = const_cast<wchar_t*>(L"Report a bug");
    SendMessageW(sidebar_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
    tip.uId = reinterpret_cast<UINT_PTR>(donate_button);
    tip.lpszText = const_cast<wchar_t*>(L"Donate via PayPal");
    SendMessageW(sidebar_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
    tip.uId = reinterpret_cast<UINT_PTR>(version_link);
    tip.lpszText = const_cast<wchar_t*>(L"Open Taxi Cam on GitHub");
    SendMessageW(sidebar_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tip));
  }
  button(L"Menu", 602, 1310, 86, 80, 34);
  button(L"Save changes", 500, 1115, 670, 135, 42);
  button(L"Hide to tray", 501, 1258, 670, 137, 42);
  if (page == 0) {
    HWND combo = child(L"COMBOBOX", L"", 210, 260, 326, 420, 220, CBS_DROPDOWNLIST | WS_VSCROLL);
    for (const auto* profile : profiles::Catalog)
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(profile->name));
    for (size_t i = 0; i < profiles::Catalog.size(); ++i)
      if (profiles::Catalog[i]->id == s.profile)
        SendMessageW(combo, CB_SETCURSEL, i, 0);
    toggle(L"Auto aircraft", 230, s.auto_profile, 707, 318, 140);
    toggle(L"Auto-connect", 240, auto_connect.load(std::memory_order_acquire), 707, 236, 155);
    button(win::connection_button_label(connection_requested.load(std::memory_order_acquire)), 241, 260, 236, 155, 34);
    const auto* profile = profiles::find(s.profile);
    const bool manual = profile && profile->taxi_control == profiles::TaxiControl::manual_only;
    toggle(L"TAXI buttons", 221, s.follow_taxi && !manual, 800, 412, 180);
    EnableWindow(GetDlgItem(window, 221), !manual);
    button(L"Keyboard shortcuts…", 645, 580, 412, 205);
    edit(s.camera_rate, 200, 855, 528, 100);
  } else if (page == 1) {
    for (int i = 0; i < 2; ++i) {
      const int x = 260 + i * 375;
      for (int j = 0; j < 6; ++j)
        edit(s.mounts[i][j], 300 + i * 10 + j, x + (j % 3) * 106, 245 + (j / 3) * 108, 88);
      button(L"Lower 0.25 m", 330 + i * 10, x, 439, 146);
      button(L"Raise 0.25 m", 331 + i * 10, x + 160, 439, 146);
      button(L"Aft 1 m", 332 + i * 10, x, 488, 146);
      button(L"Forward 1 m", 333 + i * 10, x + 160, 488, 146);
    }
    button(L"Reset camera mounts", 350, 260, 594, 240);
  } else if (page == 2) {
    edit(s.exposure, 201, 840, 210, 120);
    toggle(L"Auto exposure", 222, s.automatic_exposure, 785, 318, 190);
    edit(s.night_boost, 202, 840, 430, 120);
    edit(s.camera_rate, 200, 840, 547, 120);
    button(L"Ground-speed colour", 231, 740, 630, 235);
  } else if (page == 3) {
    toggle(L"Auto detect", 223, s.auto_detect, 795, 126, 180);
    target_combos(s);
    button(L"Refresh textures", 402, 260, 294, 190);
    button(L"Swap left / right", 403, 475, 294, 190);
    toggle(L"Left preview", 224, (s.manual_mask & 1) != 0, 260, 429, 200);
    toggle(L"Right preview", 225, (s.manual_mask & 2) != 0, 505, 429, 200);
    toggle(L"Calibrate left", 226, (s.calibration_mask & 1) != 0, 260, 540, 200);
    toggle(L"Calibrate right", 227, (s.calibration_mask & 2) != 0, 505, 540, 200);
  } else if (page == 4) {
    toggle(L"Scene test", 229, s.scene_test, 260, 449, 200);
    toggle(L"First camera only", 228, s.single_camera, 505, 449, 200);
    edit(s.calibration_budget, 203, 840, 548, 120);
    button(L"Open log folder", 510, 260, 591, 210);
    button(L"Stop camera tests", 511, 500, 591, 210);
    button(L"Open summary", 515, 260, 635, 185, 34);
    button(L"Open readable log", 516, 465, 635, 185, 34);
    button(L"Open JSONL", 517, 670, 635, 185, 34);
    button(L"Open state", 518, 875, 635, 185, 34);
  } else if (page == 5) {
    const std::array<float, 2> guides[]{s.nose_dot, s.tail_upper, s.tail_corner, s.tail_inner};
    for (unsigned i = 0; i < 4; ++i) {
      const int y = i ? 338 + static_cast<int>(i - 1) * 64 : 204;
      edit(guides[i][0] * 100., 360 + static_cast<int>(i * 2), 505, y, 113);
      edit(guides[i][1] * 100., 361 + static_cast<int>(i * 2), 655, y, 113);
    }
    button(L"Apply live", 370, 260, 608, 185);
    button(L"Reset guides", 371, 467, 608, 250);
    button(L"Marking colour", 372, 740, 608, 235);
  } else if (page == 6) {
    button(L"Refresh issues", GithubRefreshId, 760, 126, 120, 36);
    button(L"Open GitHub", GithubOpenId, 888, 126, 122, 36);
    button(L"Report a bug", GithubReportId, 760, 690, 250, 42);
  }
  refreshing = false;
  InvalidateRect(window, nullptr, TRUE);
}
HICON make_icon() {
  // LoadIcon returns shared handles, including the fallback: do not destroy them.
  const auto resource = LoadIconW(instance, MAKEINTRESOURCEW(101));
  return resource ? resource : LoadIconW(nullptr, IDI_APPLICATION);
}
void tray(bool add) {
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = window;
  data.uID = 1;
  data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  data.uCallbackMessage = TrayMessage;
  data.hIcon = icon;
  wcscpy_s(data.szTip, L"Taxi Cam — Settings");
  Shell_NotifyIconW(add ? NIM_ADD : NIM_DELETE, &data);
  if (add) {
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
  }
}
void update_balloon() {
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = window;
  data.uID = 1;
  data.uFlags = NIF_INFO;
  data.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
  wcscpy_s(data.szInfoTitle, L"Taxi Cam update downloaded");
  wcscpy_s(data.szInfo, L"Close Microsoft Flight Simulator, then use Check for updates in the tray menu to install.");
  Shell_NotifyIconW(NIM_MODIFY, &data);
}
void show() {
  ShowWindow(window, SW_SHOW);
  ShowWindow(window, SW_RESTORE);
  SetForegroundWindow(window);
}
void draw_page(HDC dc) {
  RECT client{};
  GetClientRect(window, &client);
  FillRect(dc, &client, background_brush);
  auto fill = [&](int x, int y, int w, int h, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT r = rectangle(x, y, w, h);
    FillRect(dc, &r, brush);
    DeleteObject(brush);
  };
  auto line = [&](int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, scale(1), color);
    auto old = SelectObject(dc, pen);
    MoveToEx(dc, scale(x1), scale(y1), nullptr);
    LineTo(dc, scale(x2), scale(y2));
    SelectObject(dc, old);
    DeleteObject(pen);
  };
  auto badge = [&](const wchar_t* value, int x, int y, int w, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(Deep);
    HPEN pen = CreatePen(PS_SOLID, scale(1), color);
    auto ob = SelectObject(dc, brush), op = SelectObject(dc, pen);
    RoundRect(dc, scale(x), scale(y), scale(x + w), scale(y + 26), scale(13), scale(13));
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(brush); DeleteObject(pen);
    text(dc, value, x + 8, y, w - 16, 26, small, color, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  };
  auto section = [&](const wchar_t* kicker, const wchar_t* title, const wchar_t* sub) {
    text(dc, kicker, 34, 148, 180, 18, small, Accent);
    text(dc, title, 34, 169, 730, 42, title_font, Text);
    text(dc, sub, 36, 211, 730, 26, small, Muted);
    line(34, 246, 1068, 246, Border);
  };
  auto metric = [&](const wchar_t* label, const wchar_t* value, int x, int y, COLORREF color) {
    panel(dc, x, y, 210, 82, Panel2);
    text(dc, label, x + 14, y + 10, 180, 18, small, Muted);
    text(dc, value, x + 14, y + 34, 180, 30, heading, color);
  };

  // New shell: no sidebar. A full-width flight-deck command strip replaces the old navigation model.
  fill(0, 0, 1440, 72, Deep);
  fill(0, 72, 1440, 58, Sidebar);
  line(0, 71, 1440, 71, Border);
  line(0, 129, 1440, 129, Border);
  DrawIconEx(dc, scale(24), scale(18), icon, scale(38), scale(38), 0, nullptr, DI_NORMAL);
  text(dc, L"TAXICAM", 76, 12, 150, 28, heading, Text);
  text(dc, L"FLIGHT DECK SYSTEM", 76, 40, 220, 18, small, Muted);
  const wchar_t* tabs[]{L"COMMAND", L"CAMERAS", L"DISPLAY", L"PFD", L"DIAGNOSTICS", L"GUIDES", L"ISSUES"};
  for (int i = 0; i < 7; ++i) {
    const int x = 235 + i * 108;
    const bool selected = page == i;
    if (selected) fill(x, 82, 100, 34, RGB(16, 45, 55));
    text(dc, tabs[i], x, 84, 100, 30, small, selected ? Accent : Muted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (selected) fill(x + 18, 122, 64, 2, Accent);
  }
  badge(L"MSFS 2024", 1035, 22, 100, Muted);
  win::Status sample;
  std::wstring live;
  { const std::lock_guard lock(app_mutex); sample = status; live = connection; }
  const bool has_displays = sample.candidate_count != 0 || sample.left_id != 0 || sample.right_id != 0;
  const bool connected = connection_requested.load(std::memory_order_acquire) && sample.graphics_ready;
  const bool healthy = connected && sample.scene_ready && has_displays;
  const COLORREF health = healthy ? Accent : connected ? Amber : Muted;
  badge(healthy ? L"SYSTEM READY" : connected ? L"CONNECTING" : L"OFFLINE", 1148, 22, 120, health);

  // Right telemetry rail.
  fill(1090, 145, 320, 620, RGB(8, 16, 25));
  line(1090, 145, 1090, 765, Border);
  text(dc, L"LIVE TELEMETRY", 1112, 165, 250, 24, small, Accent);
  text(dc, L"CURRENT SESSION", 1112, 193, 250, 18, small, Muted);
  metric(L"SIMULATOR", connected ? L"ONLINE" : L"WAITING", 1112, 225, connected ? Accent : Muted);
  metric(L"GRAPHICS", sample.graphics_ready ? L"D3D12" : L"WAITING", 1112, 318, sample.graphics_ready ? Accent : Muted);
  metric(L"PFD TARGETS", has_displays ? L"LOCKED" : L"SEARCH", 1112, 411, has_displays ? Accent : Amber);
  metric(L"CAPTURE", sample.captures ? L"RUNNING" : L"IDLE", 1112, 504, sample.captures ? Accent : Muted);
  metric(L"PFD OUTPUT", sample.stamps ? L"ACTIVE" : L"IDLE", 1112, 597, sample.stamps ? Accent : Muted);
  const auto* rail_profile = profiles::find(draft().profile);
  text(dc, rail_profile ? rail_profile->name : L"Automatic profile", 1112, 700, 280, 24, normal, Text, DT_LEFT | DT_END_ELLIPSIS | DT_SINGLELINE);
  text(dc, L"ACTIVE AIRCRAFT", 1112, 725, 250, 18, small, Muted);
  text(dc, connected ? L"Live bridge heartbeat received" : live.c_str(), 1112, 744, 280, 38, small, health, DT_LEFT | DT_WORDBREAK);

  // Main content area.
  if (page == 0) {
    section(L"01 / COMMAND", L"Mission control", L"Connect, select the active aircraft profile and control the flight-deck camera system.");
    panel(dc, 34, 272, 1034, 126);
    text(dc, L"SESSION", 54, 289, 180, 18, small, Muted);
    text(dc, connected ? L"CONNECTED TO MICROSOFT FLIGHT SIMULATOR" : L"WAITING FOR MICROSOFT FLIGHT SIMULATOR", 54, 315, 700, 28, heading, health);
    text(dc, connected ? L"Bridge and simulator state are available." : L"Start MSFS 2024, then use Connect when the simulator is ready.", 54, 351, 700, 22, small, Muted);
    badge(auto_connect.load() ? L"AUTO CONNECT ON" : L"AUTO CONNECT OFF", 815, 310, 205, auto_connect.load() ? Accent : Muted);
    panel(dc, 34, 416, 500, 208);
    text(dc, L"AIRCRAFT PROFILE", 54, 435, 260, 20, small, Accent);
    const auto* p = profiles::find(draft().profile);
    text(dc, p ? p->name : L"Automatic profile", 54, 465, 430, 32, heading);
    text(dc, L"Settings are isolated per aircraft and persist between sessions.", 54, 507, 420, 42, small, Muted, DT_LEFT | DT_WORDBREAK);
    text(dc, L"TAXI BUTTON ROUTING", 54, 568, 220, 20, small, Muted);
    const bool manual = p && p->taxi_control == profiles::TaxiControl::manual_only;
    text(dc, manual ? L"MANUAL" : (draft().follow_taxi ? L"ACTIVE" : L"OFF"), 280, 564, 160, 28, heading, manual ? Amber : draft().follow_taxi ? Accent : Muted);
    panel(dc, 558, 416, 510, 208);
    text(dc, L"FLIGHT-DECK STATUS", 578, 435, 300, 20, small, Accent);
    text(dc, sample.scene_ready ? L"CAMERA ENGINE READY" : L"CAMERA ENGINE WAITING", 578, 466, 440, 30, heading, sample.scene_ready ? Accent : Muted);
    text(dc, L"NOSE", 578, 515, 100, 18, small, Muted);
    text(dc, sample.captures ? L"CAPTURING" : L"STANDBY", 578, 537, 150, 25, normal, sample.captures ? Accent : Muted);
    text(dc, L"TAIL", 760, 515, 100, 18, small, Muted);
    text(dc, sample.captures ? L"CAPTURING" : L"STANDBY", 760, 537, 150, 25, normal, sample.captures ? Accent : Muted);
    text(dc, L"PFD", 930, 515, 70, 18, small, Muted);
    text(dc, has_displays ? L"LOCKED" : L"SEARCH", 930, 537, 100, 25, normal, has_displays ? Accent : Amber);
    text(dc, L"10 FPS DEFAULT  •  AUTO DISABLE ABOVE 60 KNOTS", 578, 583, 430, 22, small, Muted);
  } else if (page == 1) {
    section(L"02 / CAMERAS", L"Camera geometry", L"Tune nose-wheel and tail camera mounting geometry for the active aircraft profile.");
    panel(dc, 34, 272, 500, 350, Panel2); panel(dc, 558, 272, 510, 350, Panel2);
    text(dc, L"NOSE-WHEEL CAMERA", 54, 292, 420, 28, heading);
    text(dc, L"UPPER FLIGHT-DECK VIEW", 54, 326, 400, 20, small, Muted);
    text(dc, L"TAIL CAMERA", 578, 292, 420, 28, heading);
    text(dc, L"LOWER FLIGHT-DECK VIEW", 578, 326, 400, 20, small, Muted);
    text(dc, L"GEOMETRY", 54, 374, 150, 18, small, Accent); text(dc, L"GEOMETRY", 578, 374, 150, 18, small, Accent);
    text(dc, L"Right / Up / Forward", 54, 405, 210, 22, normal, Text); text(dc, L"Right / Up / Forward", 578, 405, 210, 22, normal, Text);
    text(dc, L"Pitch / Yaw / Lens", 54, 505, 210, 22, normal, Text); text(dc, L"Pitch / Yaw / Lens", 578, 505, 210, 22, normal, Text);
    text(dc, L"Use the controls above to make precise adjustments.", 54, 575, 450, 24, small, Muted);
    text(dc, L"Positive pitch looks up  •  positive yaw looks right", 578, 575, 450, 24, small, Muted);
  } else if (page == 2) {
    section(L"03 / DISPLAY", L"Optical profile", L"Control exposure, night visibility and camera update rate without touching the camera engine.");
    const int ys[]{272, 360, 448, 536};
    const wchar_t* names[]{L"DAYTIME EXPOSURE", L"AUTOMATIC NIGHT EXPOSURE", L"MAXIMUM NIGHT BOOST", L"CAMERA FRAME RATE"};
    const wchar_t* desc[]{L"Exposure compensation in EV.", L"Gradually brighten the camera feed as ambient light drops.", L"Additional exposure available at night.", L"Activation limit per camera; default is 10 FPS."};
    for (int i=0;i<4;++i) { panel(dc,34,ys[i],1034,72,Panel2); text(dc,names[i],54,ys[i]+10,330,22,heading); text(dc,desc[i],395,ys[i]+12,420,40,small,Muted,DT_LEFT|DT_WORDBREAK); }
    wchar_t value[96]; std::swprintf(value,96,L"APPLIED EXPOSURE %.2f EV",sample.exposure); badge(value,54,630,260,Accent);
  } else if (page == 3) {
    section(L"04 / PFD", L"Display routing", L"Discover, verify and route the camera feeds to the correct flight-deck PFD targets.");
    panel(dc,34,272,500,190,Panel2); panel(dc,558,272,510,190,Panel2);
    text(dc,L"LEFT PFD",54,292,250,24,small,Muted); text(dc,has_displays?L"TARGET DETECTED":L"SEARCHING",54,328,420,32,heading,has_displays?Accent:Amber);
    text(dc,L"Target ID",54,374,120,18,small,Muted); wchar_t ids[64]; std::swprintf(ids,64,L"%llu",static_cast<unsigned long long>(sample.left_id)); text(dc,ids,54,397,220,28,normal,Text);
    text(dc,L"RIGHT PFD",578,292,250,24,small,Muted); text(dc,has_displays?L"TARGET DETECTED":L"SEARCHING",578,328,420,32,heading,has_displays?Accent:Amber);
    text(dc,L"Target ID",578,374,120,18,small,Muted); std::swprintf(ids,64,L"%llu",static_cast<unsigned long long>(sample.right_id)); text(dc,ids,578,397,220,28,normal,Text);
    panel(dc,34,484,1034,138,Panel2); text(dc,L"SAFE ROUTING",54,505,250,22,small,Accent); text(dc,L"Automatic discovery is recommended.",54,536,400,26,heading); text(dc,L"Manual preview and calibration remain available through the controls.",54,573,760,22,small,Muted);
  } else if (page == 4) {
    section(L"05 / DIAGNOSTICS", L"System diagnostics", L"Technical state remains available here without cluttering the normal flight-deck workflow.");
    metric(L"BRIDGE", sample.graphics_ready?L"READY":L"WAITING",34,272,sample.graphics_ready?Accent:Muted);
    metric(L"CAMERAS", sample.scene_ready?L"READY":L"WAITING",258,272,sample.scene_ready?Accent:Muted);
    metric(L"CAPTURE", sample.captures?L"ACTIVE":L"IDLE",482,272,sample.captures?Accent:Muted);
    metric(L"PFD OUTPUT", sample.stamps?L"ACTIVE":L"IDLE",706,272,sample.stamps?Accent:Muted);
    panel(dc,34,370,1034,252,Panel2);
    text(dc,L"LIVE BRIDGE TELEMETRY",54,390,360,24,small,Accent);
    wchar_t data[1024]; std::swprintf(data,1024,L"Frames captured       %llu\nCompositions          %llu\nPFD writes            %llu\nHook failures         %llu\nProbe CPU             %.2f ms\nProbe maximum         %.2f ms",static_cast<unsigned long long>(sample.captures),static_cast<unsigned long long>(sample.composed),static_cast<unsigned long long>(sample.stamps),static_cast<unsigned long long>(sample.hook_failures),sample.probe_cpu_ms,sample.probe_max_ms);
    text(dc,data,54,430,450,160,normal,Text,DT_LEFT|DT_WORDBREAK);
    text(dc,live.empty()?L"No bridge message yet.":live.c_str(),540,430,470,120,small,Muted,DT_LEFT|DT_WORDBREAK);
  } else if (page == 5) {
    section(L"06 / GUIDES", L"Composition guides", L"Tune the visual guide markers used by the camera composition system.");
    panel(dc,34,272,1034,350,Panel2); text(dc,L"LIVE COMPOSITION",54,292,300,26,heading); text(dc,L"NOSE-WHEEL VIEW",54,338,250,22,small,Accent); text(dc,L"TAIL VIEW",54,408,250,22,small,Accent); text(dc,L"X 0–50%   •   Y 0–100%",54,566,400,22,small,Muted);
  } else if (page == 6) {
    section(L"07 / ISSUES", L"Active bug reports", L"Live view of currently open issues from JoelScripts/A350-Improvements.");
    panel(dc,34,272,1034,350,Panel2); text(dc,L"GITHUB ISSUE TRACKER",54,292,420,26,heading); text(dc,L"JoelScripts / A350-Improvements",54,326,450,20,small,Muted);
    wchar_t count_label[64]; std::swprintf(count_label,64,L"%zu ACTIVE",github_issues.size()); badge(count_label,850,292,170,Accent);
    if (github_issues.empty() && !github_issue_error.empty()) text(dc,github_issue_error.c_str(),54,380,900,60,normal,Muted,DT_LEFT|DT_WORDBREAK);
    else if (github_issues.empty()) text(dc,L"Loading public bug reports…",54,380,600,28,normal,Muted);
    else { github_issue_cards.clear(); int y=370, shown=0; for(int i=0;i<(int)github_issues.size()&&shown<5;++i){const auto& issue=github_issues[i]; panel(dc,54,y,990,46,issue.open?Card:Panel2); github_issue_cards.push_back(rectangle(54,y,990,46)); wchar_t n[32]; std::swprintf(n,32,L"#%d",issue.number); text(dc,n,66,y+7,50,24,small,Muted); auto t=widen(issue.title.c_str()); text(dc,t.c_str(),125,y+5,600,28,normal,Text,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS); text(dc,issue.open?(issue.in_progress?L"IN PROGRESS":L"OPEN"):L"RESOLVED",770,y+7,130,24,small,issue.open?(issue.in_progress?Amber:Text):Accent,DT_RIGHT); text(dc,L"VIEW",920,y+7,90,24,small,Accent,DT_RIGHT); y+=56; ++shown; } }
  }
  // Bottom command/status strip.
  fill(0, 790, 1440, 70, Deep); line(0,790,1440,790,Border);
  text(dc,L"TAXICAM-FIXED",34,806,180,20,small,Accent); text(dc,L"D3D12  •  MSFS 2024  •  PRIVATE FLIGHT-DECK TOOL",34,829,430,18,small,Muted);
  text(dc,notice.empty()?L"Ready":notice.c_str(),560,810,500,38,small,Muted,DT_RIGHT|DT_WORDBREAK);
}

DWORD WINAPI connection_worker(void*) {
  win::Mailbox mailbox;
  DWORD attached{};
  bool attempted = false;
  bool load_started_this_session = false;
  bool bridge_ok = false;
  bool manual_armed = false;
  std::uint64_t ignore_heartbeat_through = 0;
  win::LaunchRetry startup_retry;
  HANDLE process{};
  while (running.load()) {
    if (preview_ui) {
      Sleep(100);
      continue;
    }
    const bool auto_on = auto_connect.load(std::memory_order_acquire);
    const auto command = connect_commands.take();
    if (command == win::ConnectCommand::disconnect) {
      exchange_control(mailbox);
      mailbox.close();
      attempted = bridge_ok = manual_armed = false;
      ignore_heartbeat_through = 0;
      startup_retry.reset();
      PostMessageW(window, StatusMessage, 0, 0);
      Sleep(200);
      continue;
    }
    if (command == win::ConnectCommand::connect || command == win::ConnectCommand::reset)
      manual_armed = true;
    if (command == win::ConnectCommand::connect || command == win::ConnectCommand::reset) {
      attempted = false;
      bridge_ok = false;
      ignore_heartbeat_through = 0;
      startup_retry.reset();
      mailbox.close();
      {
        const std::lock_guard lock(app_mutex);
        if (!connection_disconnected.load(std::memory_order_acquire))
          connection =
              attached ? L"Connect requested. Retrying bridge attach." : L"Connect requested. Waiting for Microsoft Flight Simulator 2024.";
      }
      PostMessageW(window, StatusMessage, 0, 0);
    }
    const auto attach = win::find_simulator_attach(expected_simulator);
    const DWORD pid = attach.pid;
    if (attached && (pid != attached || (process && WaitForSingleObject(process, 0) == WAIT_OBJECT_0))) {
      mailbox.close();
      if (process)
        CloseHandle(process);
      process = nullptr;
      attached = 0;
      attempted = false;
      load_started_this_session = false;
      bridge_ok = false;
      manual_armed = false;
      ignore_heartbeat_through = 0;
      startup_retry.reset();
      simulator_pid = 0;
      {
        const std::lock_guard lock(app_mutex);
        status = {};
        current.enabled = 0;
        connection_requested.store(false, std::memory_order_release);
        connection = connection_disconnected.load(std::memory_order_acquire) ? L"Disconnected. Choose Connect to enable the cameras again."
                                                                             : L"Simulator closed. Waiting for the next session.";
      }
      PostMessageW(window, StatusMessage, 0, 0);
      // Stay in the tray so a later manual or auto connect can attach without
      // depending on MSFS auto-launch of Taxi Cam.
    }
    if (pid && pid != attached) {
      win::log_attach(win::settings_directory(), attach, expected_simulator);
      attached = pid;
      simulator_pid = pid;
      attempted = false;
      load_started_this_session = false;
      bridge_ok = false;
      manual_armed = manual_armed || command == win::ConnectCommand::connect || command == win::ConnectCommand::reset;
      ignore_heartbeat_through = 0;
      startup_retry = {};
      process = OpenProcess(SYNCHRONIZE, FALSE, pid);
      if (!auto_on && !manual_armed) {
        {
          const std::lock_guard lock(app_mutex);
          connection = L"MSFS detected. Auto-connect is off — choose Connect when ready.";
        }
        PostMessageW(window, StatusMessage, 0, 0);
      }
    }
    const bool want_connect =
        win::should_attempt_connect(auto_on, attempted, command, manual_armed, connection_disconnected.load(std::memory_order_acquire));
    if (attached && want_connect && startup_retry.ready(GetTickCount64())) {
      {
        const std::lock_guard lock(app_mutex);
        if (connection_disconnected.load(std::memory_order_acquire))
          continue;
        if (!connection_requested.load(std::memory_order_acquire) && !win::begin_connection(current)) {
          attempted = true;
          connection = L"Restart Taxi Cam to begin a new connection.";
          continue;
        }
        connection_requested.store(true, std::memory_order_release);
        win::apply_connection_command(current, win::ConnectCommand::connect);
      }
      PostMessageW(window, StatusMessage, 0, 0);
      attempted = true;
      if (!mailbox.data() && !mailbox.open(attached, true)) {
        const bool retrying = startup_retry.schedule({false, GetLastError(), L"mailbox", true}, GetTickCount64()) ||
                              startup_retry.schedule_recovery(GetTickCount64());
        attempted = !retrying;
        if (!retrying)
          manual_armed = false;
        {
          const std::lock_guard lock(app_mutex);
          if (!connection_disconnected.load(std::memory_order_acquire)) {
            connection = L"Could not open the camera control channel.";
            if (retrying)
              connection += L" Retrying.";
            else
              connection += L" Choose Disconnect, then Connect to try again.";
          }
        }
        PostMessageW(window, StatusMessage, 0, 0);
      } else {
        exchange_control(mailbox);
        win::LaunchDiagnostics launch_diagnostics;
        const bool allow_load = win::fresh_load_allowed(load_started_this_session);
        const auto loaded = win::load_bridge(attached, attach.path.empty() ? expected_simulator : attach.path,
                                             installation + L"\\taxi-camera-bridge.dll", &running, &launch_diagnostics, allow_load);
        if (launch_diagnostics.load_started)
          load_started_this_session = true;
        win::log_launch(win::settings_directory(), attached, loaded, launch_diagnostics);
        bool retrying = startup_retry.schedule(loaded, GetTickCount64());
        if (!retrying && !loaded.ok && loaded.retry_before_load)
          retrying = startup_retry.schedule_recovery(GetTickCount64());
        attempted = !retrying;
        if (loaded.ok) {
          manual_armed = false;
          // Confirm health from a beat newer than any stale-recovery watermark.
          bridge_ok = ignore_heartbeat_through == 0;
        } else {
          bridge_ok = false;
          if (!retrying)
            manual_armed = false;
        }
        {
          const std::lock_guard lock(app_mutex);
          if (!connection_disconnected.load(std::memory_order_acquire)) {
            connection = loaded.message;
            if (!loaded.ok)
              connection += L" (Windows " + std::to_wstring(loaded.error) + L")";
            if (retrying && loaded.retry_before_load && startup_retry.retries() == 0)
              connection += L" Automatic recovery scheduled.";
            else if (retrying)
              connection += L" Retrying startup preflight.";
            else if (loaded.retry_before_load && !loaded.ok)
              connection += L" Automatic retries paused; choose Disconnect, then Connect to retry.";
            else if (!loaded.ok)
              connection += L" Choose Disconnect, then Connect to try again.";
          }
        }
        PostMessageW(window, StatusMessage, 0, 0);
      }
    }
    // If the bridge previously started but heartbeats went stale while MSFS is
    // still running, request a safe rescan/start without a second LoadLibrary.
    if (attached && bridge_ok && attempted && mailbox.data() && !connection_disconnected.load(std::memory_order_acquire)) {
      win::Status sample;
      {
        const std::lock_guard lock(app_mutex);
        sample = status;
      }
      const auto now = GetTickCount64();
      if (sample.heartbeat && now > sample.heartbeat + 15000) {
        {
          const std::lock_guard lock(app_mutex);
          if (!connection_disconnected.load(std::memory_order_acquire)) {
            ignore_heartbeat_through = sample.heartbeat;
            bridge_ok = false;
            attempted = false;
            manual_armed = true;
            startup_retry.reset();
            // Retry through the existing authorization, preserving the stale
            // heartbeat watermark and any newer explicit Disconnect command.
            status.heartbeat = 0;
            connection = L"Bridge status went stale. Retrying attach automatically.";
          }
        }
        PostMessageW(window, StatusMessage, 0, 0);
      }
    }
    win::Status sample;
    if (running.load() && exchange_control(mailbox, &sample)) {
      {
        const std::lock_guard lock(app_mutex);
        if (!connection_disconnected.load(std::memory_order_acquire)) {
          status = sample;
          if (!win::heartbeat_confirms_bridge(sample.heartbeat, ignore_heartbeat_through))
            status.heartbeat = 0;
          else {
            bridge_ok = true;
            ignore_heartbeat_through = 0;
          }
          received_bridge_status = received_bridge_status || sample.heartbeat != 0;
        }
      }
      service_diagnostics_watch(sample);
      PostMessageW(window, StatusMessage, 0, 0);
    }
    Sleep(200);
  }
  if (mailbox.data() && mailbox.lock(100)) {
    mailbox.data()->settings.enabled = 0;
    mailbox.data()->owner_heartbeat = 0;
    mailbox.unlock();
  }
  if (process)
    CloseHandle(process);
  return 0;
}
void stop_service() {
  running = false;
  if (const auto pid = simulator_pid.load()) {
    win::Mailbox mailbox;
    if (mailbox.open(pid, false) && mailbox.lock(200)) {
      mailbox.data()->settings.enabled = 0;
      mailbox.data()->owner_heartbeat = 0;
      mailbox.unlock();
    }
  }
}
void check_updates(bool manual) {
  if (preview_ui || update_prompt)
    return;
  if (updater.begin(installation, manual)) {
    next_update_check = GetTickCount64() + 24ULL * 60 * 60 * 1000;
    if (manual) {
      notice = L"Checking for updates in the background...";
      InvalidateRect(window, nullptr, FALSE);
    }
  }
}
void service_diagnostics_watch(const win::Status& sample) noexcept {
  const auto now = GetTickCount64();
  if (!diagnostic_watch.initialized) {
    diagnostic_watch = {};
    diagnostic_watch.initialized = true;
    diagnostic_watch.last_heartbeat = sample.heartbeat;
    diagnostic_watch.last_taxi_mask = sample.taxi_mask;
    diagnostic_watch.last_captures = sample.captures;
    diagnostic_watch.last_stamps = sample.stamps;
    diagnostic_watch.last_event_ms = now;
    TAXI_DIAG_EVENT("[TaxiCam-Fixed::Watchdog]", "WATCH-SESSION-START", "OBSERVED",
                    "Companion diagnostics watchdog started.");
    return;
  }
  if (sample.taxi_mask != diagnostic_watch.last_taxi_mask) {
    char message[256]{};
    std::snprintf(message, sizeof(message), "Taxi intent mask changed from %llu to %llu while the companion was connected.",
                  static_cast<unsigned long long>(diagnostic_watch.last_taxi_mask),
                  static_cast<unsigned long long>(sample.taxi_mask));
    TAXI_DIAG_EVENT("[TaxiCam-Fixed::Camera]", "CAMERA-INTENT-CHANGED", "DETECTED", message);
    diagnostic_watch.last_taxi_mask = sample.taxi_mask;
  }
  if ((sample.captures != diagnostic_watch.last_captures || sample.stamps != diagnostic_watch.last_stamps) &&
      now - diagnostic_watch.last_progress_event_ms >= 1000) {
    char message[256]{};
    std::snprintf(message, sizeof(message), "Pipeline progress observed: captures=%llu stamps=%llu.",
                  static_cast<unsigned long long>(sample.captures), static_cast<unsigned long long>(sample.stamps));
    TAXI_DIAG_EVENT("[TaxiCam-Fixed::Pipeline]", "PIPELINE-PROGRESS", "OBSERVED", message);
    diagnostic_watch.last_captures = sample.captures;
    diagnostic_watch.last_stamps = sample.stamps;
    diagnostic_watch.last_progress_event_ms = now;
  }
  if (sample.heartbeat) {
    const auto age = now >= sample.heartbeat ? now - sample.heartbeat : 0;
    if (age <= 5000) {
      if (diagnostic_watch.stale_reported) {
        TAXI_DIAG_WATCHDOG("[TaxiCam-Fixed::Watchdog]", "WATCHDOG-RECOVERED", "RECOVERED", age,
                           "Heartbeat is fresh again; normal monitoring resumed.");
        diagnostic_watch.stale_reported = false;
      }
    } else if (age > 5000 && !diagnostic_watch.stale_reported) {
      TAXI_DIAG_WATCHDOG("[TaxiCam-Fixed::Watchdog]", "WATCHDOG-STALE", "DETECTED", age,
                         "Record stale heartbeat and preserve the last known runtime state.");
      diagnostic_watch.stale_reported = true;
    }
    diagnostic_watch.last_heartbeat = sample.heartbeat;
  }
  if (now - diagnostic_watch.last_event_ms >= 10000) {
    const auto age = sample.heartbeat && now >= sample.heartbeat ? now - sample.heartbeat : 0;
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "Health snapshot: heartbeat_age_ms=%llu taxi_mask=%llu captures=%llu stamps=%llu bridge=%s.",
                  static_cast<unsigned long long>(age), static_cast<unsigned long long>(sample.taxi_mask),
                  static_cast<unsigned long long>(sample.captures), static_cast<unsigned long long>(sample.stamps),
                  sample.heartbeat ? "present" : "missing");
    TAXI_DIAG_EVENT("[TaxiCam-Fixed::Watchdog]", "WATCHDOG-SNAPSHOT", "OBSERVED", message);
    diagnostic_watch.last_event_ms = now;
  }
}

void poll_updates() {
  if (preview_ui || update_prompt)
    return;
  win::UpdateResult result;
  if (updater.take(result)) {
    if (!result.available) {
      if (result.manual) {
        notice = result.error.empty() ? L"Taxi Cam is up to date." : result.error;
        MessageBoxW(window, notice.c_str(), L"Taxi Cam updates", MB_OK | MB_ICONINFORMATION);
      }
    } else {
      update_prompt = true;
      if (win::simulator_blocks_update()) {
        notice = L"Update downloaded. Close Microsoft Flight Simulator, then choose Check for updates to install.";
        if (result.manual)
          MessageBoxW(window, notice.c_str(), L"Taxi Cam updates", MB_OK | MB_ICONINFORMATION);
        else
          update_balloon();
      } else {
        const auto prompt =
            L"Taxi Cam " + result.tag + L" has been downloaded and verified.\n\nClose Taxi Cam and start the installer now?";
        if (MessageBoxW(window, prompt.c_str(), L"Taxi Cam update ready", MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2) == IDYES) {
          bool proceed = true;
          if (dirty) {
            const int choice = MessageBoxW(window,
                                           L"Save your unsaved settings before installing?\n\nYes: save and continue.\nNo: discard "
                                           L"changes.\nCancel: keep the app open.",
                                           L"Unsaved settings", MB_YESNOCANCEL | MB_ICONQUESTION);
            proceed = choice == IDNO || (choice == IDYES && apply());
          }
          if (proceed) {
            std::wstring error;
            if (updater.launch(result, installation, error)) {
              stop_service();
              DestroyWindow(window);
              update_prompt = false;
              return;
            }
            MessageBoxW(window, error.c_str(), L"Taxi Cam updates", MB_OK | MB_ICONWARNING);
          }
        }
      }
      update_prompt = false;
    }
    InvalidateRect(window, nullptr, FALSE);
  }
  if (!updater.busy() && GetTickCount64() >= next_update_check)
    check_updates(false);
}
bool is_on(int id, const win::Settings& s) {
  switch (id) {
    case 230:
      return s.auto_profile;
    case 241:
      return connection_requested.load(std::memory_order_acquire);
    case 240:
      return auto_connect.load(std::memory_order_acquire);
    case 221:
      return s.follow_taxi;
    case 222:
      return s.automatic_exposure;
    case 223:
      return s.auto_detect;
    case 224:
      return s.manual_mask & 1;
    case 225:
      return s.manual_mask & 2;
    case 226:
      return s.calibration_mask & 1;
    case 227:
      return s.calibration_mask & 2;
    case 228:
      return s.single_camera;
    case 229:
      return s.scene_test;
    default:
      return false;
  }
}
LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
  if (taskbar_created && message == taskbar_created) {
    tray(true);
    return 0;
  }
  switch (message) {
    case WM_CREATE: {
      window = hwnd;
      dpi = GetDpiForWindow(hwnd);
      make_fonts();
      background_brush = CreateSolidBrush(Background);
      card_brush = CreateSolidBrush(Card);
      BOOL dark = TRUE;
      DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
      DWORD corner = 2;
      DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
      icon = make_icon();
      SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
      SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
      taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
      register_camera_hotkeys();
      if (hotkey_registration.conflicts())
        notice = L"Some shortcuts are unavailable. Check Overview > Flight-deck control.";
      build_controls();
      tray(true);
      SetTimer(hwnd, 1, 250, nullptr);
      return 0;
    }
    case WM_TIMER:
      if (show_event && WaitForSingleObject(show_event, 0) == WAIT_OBJECT_0)
        show();
      poll_updates();
      return 0;
    case WM_HOTKEY: {
      const int action = hotkey_registration.action(w, l);
      const auto focus = GetFocus();
      const auto focused_id = focus && GetParent(focus) == hwnd ? GetDlgCtrlID(focus) : 0;
      if (action >= 0 && !preview_ui && !(focused_id >= 620 && focused_id <= 622))
        toggle_camera_from_hotkey(static_cast<unsigned>(action));
      return 0;
    }
    case WM_GETMINMAXINFO: {
      auto* info = reinterpret_cast<MINMAXINFO*>(l);
      info->ptMinTrackSize = {scale(1055), scale(795)};
      return 0;
    }
    case WM_DPICHANGED: {
      dpi = HIWORD(w);
      make_fonts();
      const auto* r = reinterpret_cast<RECT*>(l);
      SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
      build_controls();
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_SETCURSOR:
      if (const auto link = GetDlgItem(hwnd, 514); link && reinterpret_cast<HWND>(w) == link && LOWORD(l) == HTCLIENT) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
      }
      break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      RECT rect{};
      GetClientRect(hwnd, &rect);
      HDC memory = CreateCompatibleDC(dc);
      HBITMAP bitmap = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
      const auto old = SelectObject(memory, bitmap);
      draw_page(memory);
      BitBlt(dc, 0, 0, rect.right, rect.bottom, memory, 0, 0, SRCCOPY);
      SelectObject(memory, old);
      DeleteObject(bitmap);
      DeleteDC(memory);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
      auto dc = reinterpret_cast<HDC>(w);
      SetTextColor(dc, Text);
      SetBkColor(dc, Card);
      return reinterpret_cast<LRESULT>(card_brush);
    }
    case WM_DRAWITEM: {
      auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(l);
      if (item->CtlType != ODT_BUTTON)
        break;
      const int id = static_cast<int>(item->CtlID);
      const bool selected = (id >= 100 && id < 107 && id - 100 == page) || is_on(id, draft());
      HBRUSH surround = CreateSolidBrush((id >= 100 && id < 107) || id == 512 || id == 513 || id == 514 ? Sidebar : Background);
      FillRect(item->hDC, &item->rcItem, surround);
      DeleteObject(surround);
      if (id == 514) {
        const int saved = SaveDC(item->hDC);
        wchar_t label[64]{};
        GetWindowTextW(item->hwndItem, label, 64);
        SelectObject(item->hDC, version_font);
        SetTextColor(item->hDC, item->itemState & ODS_SELECTED ? Text : Accent);
        SetBkMode(item->hDC, TRANSPARENT);
        auto bounds = item->rcItem;
        DrawTextW(item->hDC, label, -1, &bounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (item->itemState & ODS_FOCUS) {
          InflateRect(&bounds, -1, -1);
          DrawFocusRect(item->hDC, &bounds);
        }
        RestoreDC(item->hDC, saved);
        return TRUE;
      }
      const bool primary = id == 500;
      const bool down = (item->itemState & ODS_SELECTED) != 0;
      const COLORREF fill = primary ? Accent : selected ? RGB(30, 64, 63) : down ? Border : Card;
      HBRUSH brush = CreateSolidBrush(fill);
      HPEN pen = CreatePen(PS_SOLID, scale(1), selected || primary ? Accent : Border);
      const auto oldb = SelectObject(item->hDC, brush), oldp = SelectObject(item->hDC, pen);
      RoundRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom, scale(9), scale(9));
      SelectObject(item->hDC, oldb);
      SelectObject(item->hDC, oldp);
      DeleteObject(brush);
      DeleteObject(pen);
      RECT r = item->rcItem;
      if (id == 512) {
        draw_bug_icon(item->hDC, r, Accent);
        InflateRect(&r, -scale(4), -scale(4));
      } else {
        wchar_t label[160];
        GetWindowTextW(item->hwndItem, label, 160);
        SelectObject(item->hDC, normal);
        SetTextColor(item->hDC, primary ? Background : selected ? Accent : Text);
        SetBkMode(item->hDC, TRANSPARENT);
        InflateRect(&r, -scale(10), 0);
        DrawTextW(item->hDC, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      }
      if (item->itemState & ODS_FOCUS) {
        if (id != 512)
          InflateRect(&r, -2, -4);
        DrawFocusRect(item->hDC, &r);
      }
      return TRUE;
    }
    case StatusMessage:
      refresh_connection_button();
      sync_aircraft_session();
      auto_profile();
      if (page == 3)
        target_combos(draft());
      if (IsWindowVisible(hwnd))
        InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_CONTEXTMENU:
      PostMessageW(hwnd, TrayMessage, 0, WM_CONTEXTMENU);
      return 0;
    case TrayMessage:
      if (LOWORD(l) == WM_CONTEXTMENU || LOWORD(l) == WM_RBUTTONUP) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 600, L"Settings");
        AppendMenuW(menu, MF_STRING | (preview_ui ? MF_GRAYED : 0), 604,
                    win::connection_button_label(connection_requested.load(std::memory_order_acquire)));
        AppendMenuW(menu, MF_STRING | (preview_ui || updater.busy() || update_prompt ? MF_GRAYED : 0), 603,
                    updater.busy() ? L"Checking for updates..." : L"Check for updates");
        AppendMenuW(menu, MF_STRING, 512, L"Report a bug");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 601, L"Exit");
        POINT p;
        GetCursorPos(&p);
        SetForegroundWindow(hwnd);
        const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd, nullptr);
        DestroyMenu(menu);
        if (selected == 600)
          show();
        if (selected == 604) {
          toggle_connection();
        }
        if (selected == 603)
          check_updates(true);
        if (selected == 512)
          report_bug();
        if (selected == 601) {
          stop_service();
          DestroyWindow(hwnd);
        }
        PostMessageW(hwnd, WM_NULL, 0, 0);
      } else if (LOWORD(l) == NIN_SELECT || LOWORD(l) == NIN_KEYSELECT || LOWORD(l) == NIN_BALLOONUSERCLICK ||
                 LOWORD(l) == WM_LBUTTONDBLCLK)
        show();
      return 0;
    case WM_LBUTTONUP: {
      if (page == 6) {
        const POINT point{static_cast<LONG>(LOWORD(l)), static_cast<LONG>(HIWORD(l))};
        for (size_t i = 0; i < github_issue_cards.size(); ++i) {
          if (PtInRect(&github_issue_cards[i], point)) { open_github_issue(static_cast<int>(i)); return 0; }
        }
      }
      break;
    }
    case WM_COMMAND: {
      const int id = LOWORD(w);
      if (refreshing)
        return 0;
      if (id == 645) {
        edit_camera_hotkeys();
        return 0;
      }
      if (HIWORD(w) == CBN_SELCHANGE && (id == 400 || id == 401)) {
        if (apply(false))
          dirty_notice();
        return 0;
      }
      if (HIWORD(w) == EN_CHANGE || (HIWORD(w) == CBN_SELCHANGE && id != 210)) {
        dirty_notice();
        return 0;
      }
      if (id >= 100 && id < 107) {
        auto s = draft();
        const wchar_t* field_error{};
        if (!read_fields(s, &field_error)) {
          notice = field_error ? field_error : L"Finish the current values before changing pages.";
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        publish(s);
        page = id - 100;
        build_controls();
        if (page == 6 && github_issues.empty()) refresh_github_issues(false);
        return 0;
      }
      if (id == GithubRefreshId) { refresh_github_issues(true); return 0; }
      if (id == GithubOpenId) { ShellExecuteA(hwnd, "open", win::BugReportRepository.data(), nullptr, nullptr, SW_SHOWNORMAL); return 0; }
      if (id == GithubReportId) { report_bug(); return 0; }
      if (id == 602) {
        PostMessageW(hwnd, TrayMessage, 0, WM_CONTEXTMENU);
        return 0;
      }
      if (id == 512) {
        report_bug();
        return 0;
      }
      if (id == 513) {
        donate();
        return 0;
      }
      if (id == 514) {
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd, L"open", GithubUrl, nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32)
          MessageBoxW(hwnd, L"Could not open your browser. Visit https://github.com/rthoms334/taxi-cam.", L"Taxi Cam on GitHub",
                      MB_OK | MB_ICONWARNING);
        return 0;
      }
      if (id == 210 && HIWORD(w) == CBN_SELENDOK) {
        const auto index = SendDlgItemMessageW(hwnd, 210, CB_GETCURSEL, 0, 0);
        if (index < 0 || static_cast<size_t>(index) >= profiles::Catalog.size())
          return 0;
        if (!apply())
          return 0;
        win::Settings next;
        if (!win::load_settings(next, installation, profiles::Catalog[index]->id)) {
          notice = L"Could not load that aircraft profile.";
          return 0;
        }
        next.enabled = draft().enabled;
        if (!win::prepare_profile_selection(next, draft(), false) || !win::save_settings(next)) {
          notice = L"Could not apply that aircraft profile.";
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        publish(next);
        dirty = false;
        notice = L"Aircraft profile selected. Reconnecting its cameras and displays.";
        build_controls();
        return 0;
      }
      if (id == 230) {
        if (!apply(false))
          return 0;
        auto s = draft();
        s.auto_profile = !s.auto_profile;
        publish(s);
        apply();
        build_controls();
        return 0;
      }
      if (id == 231 || id == 372) {
        if (!apply(false))
          return 0;
        const auto s = draft();
        const bool markings = id == 372;
        const auto& color = markings ? s.guide_color : s.speed_color;
        static COLORREF custom[16]{};
        CHOOSECOLORW choice{};
        choice.lStructSize = sizeof(choice);
        choice.hwndOwner = hwnd;
        choice.lpCustColors = custom;
        choice.Flags = CC_FULLOPEN | CC_RGBINIT;
        choice.rgbResult = RGB(UINT(color[0] * 255), UINT(color[1] * 255), UINT(color[2] * 255));
        if (ChooseColorW(&choice))
          apply_color_selection(s, markings, choice.rgbResult);
        return 0;
      }
      if (id == 500) {
        apply();
        return 0;
      }
      if (id == 501) {
        ShowWindow(hwnd, SW_HIDE);
        return 0;
      }
      if (id == 240) {
        const bool next = !auto_connect.load(std::memory_order_acquire);
        auto_connect.store(next, std::memory_order_release);
        if (!win::save_auto_connect(win::settings_directory(), next))
          notice = L"Could not save the Auto-connect preference.";
        else
          notice = next ? L"Auto-connect on. Taxi Cam will connect when the simulator is available."
                        : L"Auto-connect off. Use Connect whenever you are ready, including in a loaded flight.";
        if (next)
          request_connection(win::ConnectCommand::connect);
        build_controls();
        return 0;
      }
      if (id == 241) {
        toggle_connection();
        return 0;
      }
      if (id >= 221 && id <= 229) {
        if (!apply(false))
          return 0;
        auto s = draft();
        if (id == 221) {
          const auto* profile = profiles::find(s.profile);
          if (profile && profile->taxi_control == profiles::TaxiControl::manual_only)
            return 0;
          s.follow_taxi = !s.follow_taxi;
          if (s.follow_taxi) {
            s.manual_mask = 0;
            s.calibration_mask = 0;
          }
        }
        if (id == 222)
          s.automatic_exposure = !s.automatic_exposure;
        if (id == 223)
          s.auto_detect = !s.auto_detect;
        if (id == 224 || id == 225) {
          win::toggle_manual_camera(s, id == 224 ? 0u : 1u);
        }
        if (id == 226 || id == 227) {
          s.manual_mask = 0;
          s.calibration_mask ^= id == 226 ? 1u : 2u;
          const auto* profile = profiles::find(s.profile);
          s.follow_taxi = s.calibration_mask == 0 && profile && profile->taxi_control != profiles::TaxiControl::manual_only;
        }
        if (id == 228)
          s.single_camera = !s.single_camera;
        if (id == 229)
          s.scene_test = !s.scene_test;
        publish(s);
        dirty_notice();
        build_controls();
        return 0;
      }
      if ((id >= 330 && id <= 333) || (id >= 340 && id <= 343)) {
        if (!apply(false))
          return 0;
        auto s = draft();
        const unsigned side = id >= 340 ? 1u : 0u;
        const int action = (id - 330) % 10;
        if (action == 0)
          s.mounts[side][1] -= 0.25;
        if (action == 1)
          s.mounts[side][1] += 0.25;
        if (action == 2)
          s.mounts[side][2] -= 1;
        if (action == 3)
          s.mounts[side][2] += 1;
        if (win::valid_settings(s)) {
          publish(s);
          dirty_notice();
          build_controls();
        }
        return 0;
      }
      if (id == 350) {
        auto s = draft();
        s.mounts = profiles::find(s.profile)->mounts;
        publish(s);
        dirty_notice();
        build_controls();
        return 0;
      }
      if (id == 370) {
        if (apply(false)) {
          dirty_notice();
          notice = L"Preview applied. Save changes to keep these guides.";
        }
        return 0;
      }
      if (id == 371) {
        auto s = draft();
        if (const auto* profile = profiles::find(s.profile)) {
          win::reset_guide_settings(s, *profile);
          publish(s);
          dirty_notice();
          notice = L"Profile guide positions and colour restored. Save changes to keep them.";
          build_controls();
        }
        return 0;
      }
      if (id == 402) {
        if (apply(false)) {
          win::Status sample;
          {
            const std::lock_guard lock(app_mutex);
            sample = status;
          }
          if (sample.graphics_ready && sample.candidate_count == 0 && sample.left_id == 0 && sample.right_id == 0) {
            notice = L"Waiting for cockpit displays to be drawn. Restart Flight only if the list stays empty.";
            InvalidateRect(hwnd, nullptr, FALSE);
          }
          build_controls();
        }
        return 0;
      }
      if (id == 403) {
        if (!apply(false))
          return 0;
        auto s = draft();
        const auto result = win::update_target_assignment(s.left_id, s.right_id, s.route_request, s.right_id, s.left_id);
        if (result == win::TargetAssignmentResult::sequence_exhausted) {
          notice = L"Display assignment request limit reached. Restart Taxi Cam.";
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        }
        publish(s);
        dirty_notice();
        build_controls();
        return 0;
      }
      if (id == 510) {
        win::advanced_diagnostics::initialize("companion");
        const auto& folder = win::advanced_diagnostics::directory;
        INT_PTR result = 0;
        if (!folder.empty()) {
          result = reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        }
        if (result <= 32) {
          notice = L"Could not open the Taxi Cam diagnostics folder.";
          win::advanced_diagnostics::event("diagnostics.folder_open_failed", "Failed to open the diagnostics folder", "companion");
        } else {
          notice = L"Opened the Taxi Cam diagnostics folder.";
          win::advanced_diagnostics::event("diagnostics.folder_opened", "Opened the diagnostics folder", "companion");
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      }
      if (id == 515) {
        open_diagnostic_file(win::advanced_diagnostics::summary_path, L"diagnostics-summary.txt");
        return 0;
      }
      if (id == 516) {
        open_diagnostic_file(win::advanced_diagnostics::readable_path, L"advanced-diagnostics.log");
        return 0;
      }
      if (id == 517) {
        open_diagnostic_file(win::advanced_diagnostics::events_path, L"advanced-diagnostics.jsonl");
        return 0;
      }
      if (id == 518) {
        open_diagnostic_file(win::advanced_diagnostics::state_path, L"advanced-state.txt");
        return 0;
      }
      if (id == 511) {
        auto s = draft();
        s.follow_taxi = 0;
        s.manual_mask = s.calibration_mask = s.scene_test = 0;
        publish(s);
        dirty_notice();
        build_controls();
        return 0;
      }
      return 0;
    }
    case WM_CLOSE:
      if (w == 1) {
        stop_service();
        DestroyWindow(hwnd);
      } else
        ShowWindow(hwnd, SW_HIDE);
      return 0;
    case WM_DESTROY:
      hotkeys_closing = true;
      hotkey_registration.clear();
      if (sidebar_tooltip) {
        DestroyWindow(sidebar_tooltip);
        sidebar_tooltip = nullptr;
      }
      stop_service();
      tray(false);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, message, w, l);
}
}  // namespace
int WINAPI wWinMain(HINSTANCE app, HINSTANCE, LPWSTR, int) {
  instance = app;
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  int argc{};
  auto** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; i < argc; ++i) {
    if (!std::wcscmp(argv[i], L"--background"))
      background_start = true;
    else if (!std::wcscmp(argv[i], L"--preview"))
      preview_ui = true;
    else if (!std::wcscmp(argv[i], L"--simulator") && i + 1 < argc)
      expected_simulator = argv[++i];
    else {
      LocalFree(argv);
      return ERROR_INVALID_PARAMETER;
    }
  }
  LocalFree(argv);
  // Retain coordination names so an older installed app cannot run alongside this one.
  singleton = CreateMutexW(nullptr, FALSE, preview_ui ? L"Local\\380TaxiCamera.Preview" : L"Local\\380TaxiCamera.Companion");
  const DWORD existing = GetLastError();
  show_event = CreateEventW(nullptr, FALSE, FALSE, preview_ui ? L"Local\\380TaxiCamera.Preview.Show" : L"Local\\380TaxiCamera.Show");
  if (!singleton || !show_event)
    return 1;
  if (existing == ERROR_ALREADY_EXISTS) {
    if (!background_start)
      SetEvent(show_event);
    CloseHandle(show_event);
    CloseHandle(singleton);
    return 0;
  }
  wchar_t path[32768]{};
  const DWORD n = GetModuleFileNameW(nullptr, path, 32768);
  if (!n || n >= 32768)
    return 1;
  installation = path;
  installation.resize(installation.find_last_of(L"\\/"));
  if (preview_ui)
    win::settings_override = installation + L"\\preview-settings";
  if (!win::load_settings(current, installation))
    notice = L"Saved settings were invalid; profile defaults loaded.";
  // enabled is runtime connection state. A saved Service: Off value from an
  // older version must never prevent Connect or Auto-connect from enabling it.
  current.enabled = 0;
  auto_connect.store(win::load_auto_connect(win::settings_directory()), std::memory_order_release);
  if (!win::load_camera_hotkeys(hotkey_saved, win::settings_directory()))
    notice = L"Saved shortcuts were invalid and disabled. Configure them in Overview > Flight-deck control.";
  hotkey_draft = hotkey_saved;
  INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES | ICC_HOTKEY_CLASS};
  InitCommonControlsEx(&common);
  WNDCLASSEXW type{};
  type.cbSize = sizeof(type);
  type.hInstance = instance;
  type.lpfnWndProc = procedure;
  type.lpszClassName = WindowClass;
  type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  if (!RegisterClassExW(&type))
    return 1;
  dpi = GetDpiForSystem();
  HWND main = CreateWindowExW(WS_EX_APPWINDOW, WindowClass, L"Taxi Cam", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
                              CW_USEDEFAULT, scale(1070), scale(810), nullptr, nullptr, instance, nullptr);
  if (!main)
    return 1;
  worker = CreateThread(nullptr, 0, connection_worker, nullptr, 0, nullptr);
  if (!worker) {
    DestroyWindow(main);
    return 1;
  }
  const win::StartupSettings startup(win::settings_directory(), background_start, preview_ui);
  if (startup.should_show()) {
    ShowWindow(main, SW_SHOW);
    if (!startup.record_shown(IsWindowVisible(main) != FALSE, worker != nullptr)) {
      notice = L"Could not remember the first launch. Settings may reopen next time.";
      InvalidateRect(main, nullptr, FALSE);
    }
  }
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(main, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  running = false;
  updater.stop();
  WaitForSingleObject(worker, 1500);
  CloseHandle(worker);
  CloseHandle(show_event);
  CloseHandle(singleton);
  return 0;
}

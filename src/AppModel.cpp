#include "AppModel.h"
#include "I18n.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace su {
namespace {

std::wstring IniRead(const fs::path& file, const std::wstring& section,
                     const std::wstring& key, const std::wstring& fallback = L"") {
    std::vector<wchar_t> buffer(32768);
    GetPrivateProfileStringW(section.c_str(), key.c_str(), fallback.c_str(), buffer.data(),
                             static_cast<DWORD>(buffer.size()), file.c_str());
    return buffer.data();
}

std::wstring EncodeLines(std::wstring value) {
    std::wstring out;
    for (wchar_t ch : value) {
        if (ch == L'\r') continue;
        if (ch == L'\n') out += L"|;|";
        else out += ch;
    }
    return out;
}

std::wstring DecodeLines(std::wstring value) {
    const std::wstring marker = L"|;|";
    size_t pos = 0;
    while ((pos = value.find(marker, pos)) != std::wstring::npos) {
        value.replace(pos, marker.size(), L"\r\n");
        pos += 2;
    }
    return value;
}

bool IniWrite(const fs::path& file, const std::wstring& section,
              const std::wstring& key, const std::wstring& value) {
    return WritePrivateProfileStringW(section.c_str(), key.c_str(), value.c_str(), file.c_str()) != FALSE;
}

bool CreateUnicodeIni(const fs::path& file) {
    HANDLE handle = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const wchar_t bom = 0xFEFF;
    DWORD written = 0;
    const bool okay = WriteFile(handle, &bom, sizeof(bom), &written, nullptr) != FALSE && written == sizeof(bom);
    CloseHandle(handle);
    return okay;
}

std::wstring EscapeVdf(std::wstring value) {
    std::wstring out;
    out.reserve(value.size() + 8);
    for (wchar_t ch : value) {
        if (ch == L'\"') out += L"\\\"";
        else out += ch;
    }
    return out;
}

bool WriteUtf8(const fs::path& path, const std::wstring& text, std::wstring& error) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        error = Tr(Str::ErrEncodeVdf);
        return false;
    }
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        utf8.data(), size, nullptr, nullptr);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = Fmt(Str::ErrWriteFile, {path.wstring()});
        return false;
    }
    stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    return stream.good();
}

bool MatchesExclusion(const fs::path& relative, const std::vector<std::wstring>& patterns) {
    std::wstring path = relative.generic_wstring();
    std::wstring name = relative.filename().wstring();
    for (auto pattern : patterns) {
        pattern = Trim(pattern);
        if (pattern.empty() || pattern[0] == L'#') continue;
        std::replace(pattern.begin(), pattern.end(), L'\\', L'/');
        if (PathMatchSpecW(path.c_str(), pattern.c_str()) ||
            PathMatchSpecW(name.c_str(), pattern.c_str())) return true;
        if (pattern.size() >= 2 && pattern.substr(pattern.size() - 2) == L"/*") {
            const auto prefix = pattern.substr(0, pattern.size() - 1);
            if (path.rfind(prefix, 0) == 0 || path.find(L"/" + prefix) != std::wstring::npos) return true;
        }
    }
    return false;
}

std::wstring LastWin32Error(DWORD code = GetLastError()) {
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring result = message ? message : Tr(Str::ErrUnknownWindows);
    if (message) LocalFree(message);
    return Trim(result);
}

} // namespace

fs::path AppDataRoot() {
    PWSTR raw = nullptr;
    fs::path root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw))) {
        root = fs::path(raw) / L"SteamUploader";
        CoTaskMemFree(raw);
    } else {
        root = fs::temp_directory_path() / L"SteamUploader";
    }
    std::error_code ec;
    fs::create_directories(root, ec);
    return root;
}

std::wstring ReadSetting(const std::wstring& key) {
    return IniRead(AppDataRoot() / L"settings.ini", L"General", key);
}

void WriteSetting(const std::wstring& key, const std::wstring& value) {
    IniWrite(AppDataRoot() / L"settings.ini", L"General", key, value);
}

std::wstring NewId() {
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    std::wstringstream out;
    out << std::hex << value.QuadPart << L"-" << GetCurrentProcessId() << L"-" << GetTickCount64();
    return out.str();
}

std::wstring FormatBytes(uint64_t bytes) {
    static constexpr const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
    std::wstringstream out;
    out << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << L" " << units[unit];
    return out.str();
}

std::wstring NowText() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u", t.wYear, t.wMonth, t.wDay,
               t.wHour, t.wMinute, t.wSecond);
    return buffer;
}

std::wstring Trim(std::wstring value) {
    const auto notSpace = [](wchar_t c) { return !iswspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::vector<std::wstring> SplitLines(const std::wstring& value) {
    std::vector<std::wstring> out;
    std::wstringstream stream(value);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        line = Trim(line);
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

std::vector<std::wstring> SplitCsv(const std::wstring& value) {
    std::vector<std::wstring> out;
    std::wstringstream stream(value);
    std::wstring item;
    while (std::getline(stream, item, L',')) {
        item = Trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::wstring QuoteArgument(const std::wstring& value) {
    std::wstring out = L"\"";
    unsigned slashes = 0;
    for (wchar_t ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'\"') { out.append(slashes * 2 + 1, L'\\'); out += ch; slashes = 0; continue; }
        out.append(slashes, L'\\'); slashes = 0; out += ch;
    }
    out.append(slashes * 2, L'\\');
    out += L'\"';
    return out;
}

bool OpenTarget(const std::wstring& target) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", target.c_str(), nullptr,
                                                   nullptr, SW_SHOWNORMAL)) > 32;
}

ProfileStore::ProfileStore() : file_(AppDataRoot() / L"profiles.ini") {}

Profile ProfileStore::DefaultProfile() {
    Profile p;
    p.id = NewId();
    p.name = Tr(Str::NewProfileName);
    p.description = L"Windows build";
    p.exclusions = L"*.pdb\r\nManifest_*.txt\r\nsteam_appid.txt";
    return p;
}

std::vector<Profile> ProfileStore::Load() {
    std::vector<Profile> profiles;
    const int count = _wtoi(IniRead(file_, L"General", L"Count", L"0").c_str());
    for (int i = 0; i < count; ++i) {
        const std::wstring section = L"Profile" + std::to_wstring(i);
        Profile p;
        p.id = IniRead(file_, section, L"Id");
        p.name = IniRead(file_, section, L"Name");
        p.appId = IniRead(file_, section, L"AppId");
        p.depotIds = IniRead(file_, section, L"DepotIds");
        p.account = IniRead(file_, section, L"Account");
        p.contentRoot = IniRead(file_, section, L"ContentRoot");
        p.steamCmd = IniRead(file_, section, L"SteamCmd");
        p.description = IniRead(file_, section, L"Description");
        p.exclusions = DecodeLines(IniRead(file_, section, L"Exclusions"));
        p.setLive = IniRead(file_, section, L"SetLive");
        if (p.id.empty()) p.id = NewId();
        if (!p.name.empty()) profiles.push_back(std::move(p));
    }
    if (profiles.empty()) profiles.push_back(DefaultProfile());
    return profiles;
}

bool ProfileStore::Save(const std::vector<Profile>& profiles, std::wstring& error) {
    std::error_code ec;
    fs::create_directories(file_.parent_path(), ec);
    if (fs::exists(file_, ec) && !DeleteFileW(file_.c_str())) {
        error = Fmt(Str::ErrOverwriteProfiles, {file_.wstring()});
        return false;
    }
    if (!CreateUnicodeIni(file_)) {
        error = Fmt(Str::ErrCreateProfiles, {file_.wstring()});
        return false;
    }
    if (!IniWrite(file_, L"General", L"Count", std::to_wstring(profiles.size()))) {
        error = Fmt(Str::ErrSaveProfiles, {file_.wstring()});
        return false;
    }
    for (size_t i = 0; i < profiles.size(); ++i) {
        const auto& p = profiles[i];
        const std::wstring s = L"Profile" + std::to_wstring(i);
        const std::pair<const wchar_t*, std::wstring> fields[] = {
            {L"Id", p.id}, {L"Name", p.name}, {L"AppId", p.appId},
            {L"DepotIds", p.depotIds}, {L"Account", p.account},
            {L"ContentRoot", p.contentRoot}, {L"SteamCmd", p.steamCmd},
            {L"Description", p.description}, {L"Exclusions", EncodeLines(p.exclusions)},
            {L"SetLive", p.setLive}
        };
        for (const auto& [key, value] : fields) {
            if (!IniWrite(file_, s, key, value)) {
                error = Fmt(Str::ErrWriteField, {key});
                return false;
            }
        }
    }
    return true;
}

bool ProfileStore::Export(const Profile& profile, const fs::path& file, std::wstring& error) {
    std::error_code ec;
    if (!file.parent_path().empty()) fs::create_directories(file.parent_path(), ec);
    if (fs::exists(file, ec) && !DeleteFileW(file.c_str())) {
        error = Fmt(Str::ErrOverwriteExport, {file.wstring()});
        return false;
    }
    if (!CreateUnicodeIni(file)) {
        error = Fmt(Str::ErrCreateExport, {file.wstring()});
        return false;
    }
    const std::pair<const wchar_t*, std::wstring> fields[] = {
        {L"Format", L"SteamUploaderProfileV1"}, {L"Name", profile.name},
        {L"AppId", profile.appId}, {L"DepotIds", profile.depotIds},
        {L"Account", profile.account}, {L"ContentRoot", profile.contentRoot},
        {L"SteamCmd", profile.steamCmd}, {L"Description", profile.description},
        {L"Exclusions", EncodeLines(profile.exclusions)}, {L"SetLive", profile.setLive}
    };
    for (const auto& [key, value] : fields) {
        if (!IniWrite(file, L"Profile", key, value)) {
            error = Fmt(Str::ErrExportField, {key});
            return false;
        }
    }
    return true;
}

bool ProfileStore::Import(const fs::path& file, Profile& profile, std::wstring& error) {
    if (!fs::is_regular_file(file)) { error = Tr(Str::ErrImportMissing); return false; }
    if (IniRead(file, L"Profile", L"Format") != L"SteamUploaderProfileV1") {
        error = Tr(Str::ErrImportFormat);
        return false;
    }
    profile.id = NewId();
    profile.name = IniRead(file, L"Profile", L"Name");
    profile.appId = IniRead(file, L"Profile", L"AppId");
    profile.depotIds = IniRead(file, L"Profile", L"DepotIds");
    profile.account = IniRead(file, L"Profile", L"Account");
    profile.contentRoot = IniRead(file, L"Profile", L"ContentRoot");
    profile.steamCmd = IniRead(file, L"Profile", L"SteamCmd");
    profile.description = IniRead(file, L"Profile", L"Description");
    profile.exclusions = DecodeLines(IniRead(file, L"Profile", L"Exclusions"));
    profile.setLive = IniRead(file, L"Profile", L"SetLive");
    if (Trim(profile.name).empty()) { error = Tr(Str::ErrImportNoName); return false; }
    return true;
}

ScanResult ScanBuild(const Profile& profile) {
    ScanResult result;
    const fs::path root(profile.contentRoot);
    std::error_code ec;
    if (!fs::is_directory(root, ec)) result.errors.push_back(Tr(Str::ErrContentRoot));
    if (!fs::is_regular_file(fs::path(profile.steamCmd), ec)) result.errors.push_back(Tr(Str::ErrSteamCmdPath));
    if (Trim(profile.name).empty()) result.errors.push_back(Tr(Str::ErrNameEmpty));
    const auto digit = [](wchar_t ch) { return iswdigit(ch) != 0; };
    if (Trim(profile.appId).empty() || !std::all_of(profile.appId.begin(), profile.appId.end(), digit))
        result.errors.push_back(Tr(Str::ErrAppIdNumeric));
    const auto depots = SplitCsv(profile.depotIds);
    if (depots.empty()) result.errors.push_back(Tr(Str::ErrNoDepot));
    for (const auto& depot : depots) {
        if (!std::all_of(depot.begin(), depot.end(), digit)) result.errors.push_back(Fmt(Str::ErrDepotNumeric, {depot}));
    }
    if (!result.errors.empty() || !fs::is_directory(root, ec)) return result;

    const auto patterns = SplitLines(profile.exclusions);
    bool hasStrayAppId = false;
    bool hasSavedData = false;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        const fs::path relative = fs::relative(it->path(), root, ec);
        if (ec) { ec.clear(); continue; }
        if (MatchesExclusion(relative, patterns)) { ++result.excludedCount; continue; }
        ++result.fileCount;
        result.totalBytes += it->file_size(ec);
        if (ec) ec.clear();
        if (_wcsicmp(relative.extension().c_str(), L".exe") == 0 && result.mainExe.empty())
            result.mainExe = relative.generic_wstring();
        if (_wcsicmp(relative.filename().c_str(), L"steam_appid.txt") == 0) hasStrayAppId = true;
        std::wstring lower = relative.generic_wstring();
        std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
        if (lower.find(L"/saved/") != std::wstring::npos || lower.rfind(L"saved/", 0) == 0) hasSavedData = true;
    }
    if (result.fileCount == 0) result.errors.push_back(Tr(Str::ErrNoFiles));
    if (result.mainExe.empty()) result.errors.push_back(Tr(Str::ErrNoExe));
    if (hasStrayAppId) result.errors.push_back(Tr(Str::ErrStrayAppId));
    if (hasSavedData) result.warnings.push_back(Tr(Str::WarnSavedContent));
    if (!Trim(profile.setLive).empty()) result.warnings.push_back(Tr(Str::WarnSetLive));
    result.valid = result.errors.empty();
    return result;
}

bool GenerateVdf(const Profile& profile, bool preview, RunFiles& files, std::wstring& error) {
    const auto depots = SplitCsv(profile.depotIds);
    if (depots.empty()) { error = Tr(Str::ErrNoDepot); return false; }
    files.directory = AppDataRoot() / L"runs" / NewId();
    files.buildOutput = AppDataRoot() / L"build-cache" / profile.appId;
    files.appVdf = files.directory / (L"app_build_" + profile.appId + L".vdf");
    std::error_code ec;
    fs::create_directories(files.directory, ec);
    fs::create_directories(files.buildOutput, ec);
    if (ec) { error = Fmt(Str::ErrRunDirectory, {std::to_wstring(ec.value())}); return false; }

    std::wstringstream app;
    app << L"\"AppBuild\"\n{\n"
        << L"\t\"AppID\" \"" << EscapeVdf(profile.appId) << L"\"\n"
        << L"\t\"Desc\" \"" << EscapeVdf(profile.description) << L"\"\n"
        << L"\t\"ContentRoot\" \"" << EscapeVdf(fs::path(profile.contentRoot).wstring() + L"\\") << L"\"\n"
        << L"\t\"BuildOutput\" \"" << EscapeVdf(files.buildOutput.wstring() + L"\\") << L"\"\n"
        << L"\t\"SetLive\" \"" << EscapeVdf(profile.setLive) << L"\"\n"
        << L"\t\"Preview\" \"" << (preview ? L"1" : L"0") << L"\"\n"
        << L"\t\"Depots\"\n\t{\n";
    for (const auto& depot : depots)
        app << L"\t\t\"" << depot << L"\" \"depot_build_" << depot << L".vdf\"\n";
    app << L"\t}\n}\n";
    if (!WriteUtf8(files.appVdf, app.str(), error)) return false;

    const auto exclusions = SplitLines(profile.exclusions);
    for (const auto& depot : depots) {
        std::wstringstream vdf;
        vdf << L"\"DepotBuild\"\n{\n\t\"DepotID\" \"" << depot << L"\"\n"
            << L"\t\"FileMapping\"\n\t{\n\t\t\"LocalPath\" \"*\"\n"
            << L"\t\t\"DepotPath\" \".\"\n\t\t\"Recursive\" \"1\"\n\t}\n";
        for (const auto& pattern : exclusions) {
            if (!pattern.empty() && pattern[0] != L'#') {
                std::wstring steamPattern = pattern;
                std::replace(steamPattern.begin(), steamPattern.end(), L'/', L'\\');
                vdf << L"\t\"FileExclusion\" \"" << EscapeVdf(steamPattern) << L"\"\n";
            }
        }
        vdf << L"}\n";
        if (!WriteUtf8(files.directory / (L"depot_build_" + depot + L".vdf"), vdf.str(), error)) return false;
    }
    return true;
}

void CleanupRunFiles(const RunFiles& files) {
    std::error_code ec;
    if (!files.directory.empty()) fs::remove_all(files.directory, ec);
}

void AppendHistory(const Profile& profile, bool preview, const ProcessResult& result) {
    const fs::path dir = AppDataRoot() / L"history";
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::wstringstream line;
    line << NowText() << L" | " << (preview ? L"Preview" : L"Upload")
         << L" | Exit=" << result.exitCode;
    if (!result.buildId.empty()) line << L" | Build=" << result.buildId;
    if (!result.manifestId.empty()) line << L" | Manifest=" << result.manifestId;
    if (result.cancelled) line << L" | Cancelled";
    if (!result.error.empty()) line << L" | " << result.error;
    line << L"\n";
    const std::wstring wide = line.str();
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return;
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
    std::ofstream stream(dir / (profile.id + L".log"), std::ios::binary | std::ios::app);
    if (stream) stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
}

std::vector<std::wstring> LoadHistory(const std::wstring& profileId) {
    std::vector<std::wstring> lines;
    std::ifstream stream(AppDataRoot() / L"history" / (profileId + L".log"), std::ios::binary);
    std::string raw;
    while (std::getline(stream, raw)) {
        const int size = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
        if (size <= 0) continue;
        std::wstring line(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()), line.data(), size);
        lines.push_back(std::move(line));
    }
    return lines;
}

SteamProcess::~SteamProcess() { Cancel(); }

bool SteamProcess::LaunchInteractiveLogin(const Profile& profile, std::wstring& error) {
    if (!fs::is_regular_file(fs::path(profile.steamCmd))) { error = Tr(Str::ErrSteamCmdPath); return false; }
    if (Trim(profile.account).empty()) { error = Tr(Str::ErrNoAccount); return false; }
    std::wstring command = QuoteArgument(profile.steamCmd) + L" +login " + QuoteArgument(profile.account) + L" +quit";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE,
                        nullptr, fs::path(profile.steamCmd).parent_path().c_str(), &si, &pi)) {
        error = Fmt(Str::ErrStartSteamCmd, {LastWin32Error()});
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

ProcessResult SteamProcess::Run(const Profile& profile, const RunFiles& files,
                                const std::function<void(const std::wstring&)>& onLine) {
    ProcessResult result;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) { result.error = LastWin32Error(); return result; }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::wstring command = QuoteArgument(profile.steamCmd) + L" +login " + QuoteArgument(profile.account) +
                           L" +run_app_build " + QuoteArgument(files.appVdf.wstring()) + L" +quit";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, files.directory.c_str(), &si, &pi)) {
        result.error = Fmt(Str::ErrStartSteamCmd, {LastWin32Error()});
        CloseHandle(readPipe); CloseHandle(writePipe);
        return result;
    }
    result.started = true;
    CloseHandle(writePipe);
    CloseHandle(pi.hThread);
    process_ = pi.hProcess;

    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        AssignProcessToJobObject(job, pi.hProcess);
        job_ = job;
    }
    if (cancelRequested_) {
        if (job) TerminateJobObject(job, ERROR_CANCELLED);
        else TerminateProcess(pi.hProcess, ERROR_CANCELLED);
    }

    std::string pending;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        pending.append(buffer, read);
        size_t newline;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string raw = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();
            int count = MultiByteToWideChar(CP_UTF8, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
            UINT codePage = CP_UTF8;
            if (count <= 0) { codePage = CP_ACP; count = MultiByteToWideChar(codePage, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0); }
            std::wstring line(static_cast<size_t>(std::max(count, 0)), L'\0');
            if (count > 0) MultiByteToWideChar(codePage, 0, raw.data(), static_cast<int>(raw.size()), line.data(), count);
            onLine(line);
            std::wsmatch match;
            if (std::regex_search(line, match, std::wregex(LR"((?:BuildID|Build ID)\D+(\d+))", std::regex::icase))) result.buildId = match[1];
            if (std::regex_search(line, match, std::wregex(LR"((?:manifest|ManifestID)\D+(\d{8,}))", std::regex::icase))) result.manifestId = match[1];
        }
    }
    if (!pending.empty()) {
        int count = MultiByteToWideChar(CP_UTF8, 0, pending.data(), static_cast<int>(pending.size()), nullptr, 0);
        std::wstring line(static_cast<size_t>(std::max(count, 0)), L'\0');
        if (count > 0) MultiByteToWideChar(CP_UTF8, 0, pending.data(), static_cast<int>(pending.size()), line.data(), count);
        onLine(line);
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = exitCode;
    result.cancelled = cancelRequested_;
    CloseHandle(readPipe);
    CloseHandle(pi.hProcess);
    process_ = nullptr;
    if (job) CloseHandle(job);
    job_ = nullptr;
    if (result.cancelled) result.error = Tr(Str::TaskCancelled);
    else if (exitCode != 0) result.error = Fmt(Str::SteamCmdExitCode, {std::to_wstring(exitCode)});
    return result;
}

void SteamProcess::Prepare() {
    cancelRequested_ = false;
}

void SteamProcess::Cancel() {
    cancelRequested_ = true;
    HANDLE job = static_cast<HANDLE>(job_.load());
    HANDLE process = static_cast<HANDLE>(process_.load());
    if (job) TerminateJobObject(job, ERROR_CANCELLED);
    else if (process) TerminateProcess(process, ERROR_CANCELLED);
}

} // namespace su

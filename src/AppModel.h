#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace su {

struct Profile {
    std::wstring id;
    std::wstring name;
    std::wstring appId;
    std::wstring depotIds;
    std::wstring account;
    std::wstring contentRoot;
    std::wstring steamCmd;
    std::wstring description;
    std::wstring exclusions;
    std::wstring setLive;
};

struct ScanResult {
    bool valid = false;
    uint64_t fileCount = 0;
    uint64_t totalBytes = 0;
    uint64_t excludedCount = 0;
    std::wstring mainExe;
    std::vector<std::wstring> errors;
    std::vector<std::wstring> warnings;
};

struct RunFiles {
    std::filesystem::path directory;
    std::filesystem::path appVdf;
    std::filesystem::path buildOutput;
};

struct ProcessResult {
    bool started = false;
    bool cancelled = false;
    unsigned long exitCode = 1;
    std::wstring error;
    std::wstring buildId;
    std::wstring manifestId;
};

std::filesystem::path AppDataRoot();
// App-wide preferences in settings.ini (kept apart from profiles.ini, which Save() rewrites).
std::wstring ReadSetting(const std::wstring& key);
void WriteSetting(const std::wstring& key, const std::wstring& value);
std::wstring NewId();
std::wstring FormatBytes(uint64_t bytes);
std::wstring NowText();
std::vector<std::wstring> SplitLines(const std::wstring& value);
std::vector<std::wstring> SplitCsv(const std::wstring& value);
std::wstring Trim(std::wstring value);
std::wstring QuoteArgument(const std::wstring& value);
bool OpenTarget(const std::wstring& target);

class ProfileStore {
public:
    ProfileStore();
    std::vector<Profile> Load();
    bool Save(const std::vector<Profile>& profiles, std::wstring& error);
    bool Export(const Profile& profile, const std::filesystem::path& file, std::wstring& error);
    bool Import(const std::filesystem::path& file, Profile& profile, std::wstring& error);
    static Profile DefaultProfile();

private:
    std::filesystem::path file_;
};

ScanResult ScanBuild(const Profile& profile);
bool GenerateVdf(const Profile& profile, bool preview, RunFiles& files, std::wstring& error);
void CleanupRunFiles(const RunFiles& files);
void AppendHistory(const Profile& profile, bool preview, const ProcessResult& result);
std::vector<std::wstring> LoadHistory(const std::wstring& profileId);

class SteamProcess {
public:
    SteamProcess() = default;
    ~SteamProcess();
    SteamProcess(const SteamProcess&) = delete;
    SteamProcess& operator=(const SteamProcess&) = delete;

    ProcessResult Run(const Profile& profile, const RunFiles& files,
                      const std::function<void(const std::wstring&)>& onLine);
    bool LaunchInteractiveLogin(const Profile& profile, std::wstring& error);
    void Prepare();
    void Cancel();

private:
    std::atomic<void*> process_{nullptr};
    std::atomic<void*> job_{nullptr};
    std::atomic_bool cancelRequested_{false};
};

} // namespace su

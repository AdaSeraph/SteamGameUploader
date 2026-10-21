#pragma once

#include <initializer_list>
#include <string>

namespace su {

enum class Lang : int { English, ChineseSimplified, Japanese, Count };

// Every user-visible string. Keep the order in sync with kTable in I18n.cpp;
// a static_assert there fails the build if the two drift apart.
enum class Str : int {
    AppTitle,
    LanguageLabel,
    ProfilesHeader,
    NewProfile,
    CopyProfile,
    DeleteProfile,
    SaveProfile,
    ImportProfile,
    ExportProfile,
    LabelName,
    LabelAppId,
    LabelDepotIds,
    LabelAccount,
    LabelContentRoot,
    LabelSteamCmd,
    LabelDescription,
    LabelSetLive,
    LabelExclusions,
    Browse,
    Login,
    Scan,
    Preview,
    Upload,
    Cancel,
    OpenBuilds,
    OpenData,
    Ready,
    LogReady,

    ProfileLoaded,
    ProfileSaved,
    SaveFailedTitle,
    FilterExecutables,
    FilterProfiles,
    FilterAllFiles,
    BrowseContentTitle,
    NewProfileName,
    CopySuffix,
    ConfirmDeleteMsg,
    ConfirmDeleteTitle,
    ImportFailedTitle,
    Imported,
    ExportFailedTitle,
    Exported,
    LoginOpened,
    LoginTitle,
    LoginFailedTitle,

    Scanning,
    CheckHeader,
    FileCount,
    TotalSize,
    ExcludedCount,
    MainExe,
    WarningPrefix,
    ErrorPrefix,
    CheckPassedLog,
    CheckPassed,
    CheckPassedWarnings,
    CheckFailedLog,
    CheckFailed,

    UploadBlockedMsg,
    UploadBlockedTitle,
    ConfirmUploadMsg,
    SetLiveEmpty,
    SetLiveAuto,
    ConfirmUploadTitle,
    VdfFailedTitle,
    PreviewStartLog,
    UploadStartLog,
    AppDepotLog,
    PreviewRunning,
    Uploading,
    ConfirmCancelMsg,
    ConfirmCancelTitle,
    Cancelling,
    TaskDone,
    PreviewPassed,
    PreviewPassedLog,
    UploadSucceeded,
    UploadSucceededLog,
    BuildIdUnknown,
    TaskFailed,
    FailedPrefix,
    ConfirmExitMsg,
    ConfirmExitTitle,
    ExitingSafely,

    ErrEncodeVdf,
    ErrWriteFile,
    ErrUnknownWindows,
    ErrOverwriteProfiles,
    ErrCreateProfiles,
    ErrSaveProfiles,
    ErrWriteField,
    ErrOverwriteExport,
    ErrCreateExport,
    ErrExportField,
    ErrImportMissing,
    ErrImportFormat,
    ErrImportNoName,
    ErrContentRoot,
    ErrSteamCmdPath,
    ErrNameEmpty,
    ErrAppIdNumeric,
    ErrNoDepot,
    ErrDepotNumeric,
    ErrNoFiles,
    ErrNoExe,
    ErrStrayAppId,
    WarnSavedContent,
    WarnSetLive,
    ErrRunDirectory,
    ErrNoAccount,
    ErrStartSteamCmd,
    TaskCancelled,
    SteamCmdExitCode,

    Count
};

struct LanguageInfo {
    const wchar_t* code;       // stored in settings.ini, e.g. L"zh-CN"
    const wchar_t* nativeName; // shown in the language picker
    const wchar_t* fontFace;   // UI font that renders this language well
};

const LanguageInfo& GetLanguageInfo(Lang lang);
Lang CurrentLanguage();
void SetLanguage(Lang lang);
// Maps a stored code back to a language; returns false for unknown codes.
bool LanguageFromCode(const std::wstring& code, Lang& lang);
// Picks the closest supported language from the Windows UI language.
Lang DetectSystemLanguage();

const wchar_t* Tr(Str id);
// Tr() with {0}, {1}, ... replaced by the given arguments.
std::wstring Fmt(Str id, std::initializer_list<std::wstring> args);

} // namespace su

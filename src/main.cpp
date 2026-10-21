#include "AppModel.h"
#include "I18n.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

using su::Str;
using su::Tr;

constexpr UINT WM_APP_LOG = WM_APP + 1;
constexpr UINT WM_APP_DONE = WM_APP + 2;

struct DonePayload {
    bool preview = false;
    su::ProcessResult result;
};

struct AppState {
    HWND window = nullptr;
    HFONT font = nullptr;
    HBRUSH panelBrush = nullptr;
    su::ProfileStore store;
    std::vector<su::Profile> profiles;
    int selected = -1;
    bool busy = false;
    bool exitRequested = false;
    bool previewPassed = false;
    std::wstring previewSignature;
    su::SteamProcess steam;
    std::thread worker;
};

AppState g;

// Controls whose caption comes straight from the string table; re-applied on language change.
struct TextBinding { int id; Str text; };
constexpr TextBinding kTextBindings[] = {
    {IDC_LABEL_PROFILES, Str::ProfilesHeader}, {IDC_LABEL_LANGUAGE, Str::LanguageLabel},
    {IDC_NEW_PROFILE, Str::NewProfile}, {IDC_COPY_PROFILE, Str::CopyProfile},
    {IDC_DELETE_PROFILE, Str::DeleteProfile}, {IDC_SAVE_PROFILE, Str::SaveProfile},
    {IDC_IMPORT_PROFILE, Str::ImportProfile}, {IDC_EXPORT_PROFILE, Str::ExportProfile},
    {IDC_LABEL_NAME, Str::LabelName}, {IDC_LABEL_APP_ID, Str::LabelAppId},
    {IDC_LABEL_DEPOT_IDS, Str::LabelDepotIds}, {IDC_LABEL_ACCOUNT, Str::LabelAccount},
    {IDC_LABEL_CONTENT_ROOT, Str::LabelContentRoot}, {IDC_LABEL_STEAM_CMD, Str::LabelSteamCmd},
    {IDC_LABEL_DESCRIPTION, Str::LabelDescription}, {IDC_LABEL_SETLIVE, Str::LabelSetLive},
    {IDC_LABEL_EXCLUSIONS, Str::LabelExclusions},
    {IDC_BROWSE_CONTENT, Str::Browse}, {IDC_BROWSE_STEAM, Str::Browse},
    {IDC_LOGIN, Str::Login}, {IDC_SCAN, Str::Scan}, {IDC_PREVIEW, Str::Preview},
    {IDC_UPLOAD, Str::Upload}, {IDC_CANCEL, Str::Cancel},
    {IDC_OPEN_BUILDS, Str::OpenBuilds}, {IDC_OPEN_DATA, Str::OpenData},
};

// Single-line form rows, top to bottom. The multi-line exclusions box follows them.
struct FieldRow { int label; int edit; int browse; };
constexpr FieldRow kFieldRows[] = {
    {IDC_LABEL_NAME, IDC_NAME, 0}, {IDC_LABEL_APP_ID, IDC_APP_ID, 0},
    {IDC_LABEL_DEPOT_IDS, IDC_DEPOT_IDS, 0}, {IDC_LABEL_ACCOUNT, IDC_ACCOUNT, 0},
    {IDC_LABEL_CONTENT_ROOT, IDC_CONTENT_ROOT, IDC_BROWSE_CONTENT},
    {IDC_LABEL_STEAM_CMD, IDC_STEAM_CMD, IDC_BROWSE_STEAM},
    {IDC_LABEL_DESCRIPTION, IDC_DESCRIPTION, 0}, {IDC_LABEL_SETLIVE, IDC_SETLIVE, 0},
};

constexpr int kActionButtons[] = {IDC_LOGIN, IDC_SCAN, IDC_PREVIEW, IDC_UPLOAD,
                                  IDC_CANCEL, IDC_OPEN_BUILDS, IDC_OPEN_DATA};

HWND C(int id) { return GetDlgItem(g.window, id); }

std::wstring GetText(int id) {
    const int length = GetWindowTextLengthW(C(id));
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(C(id), value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

void SetText(int id, const std::wstring& value) { SetWindowTextW(C(id), value.c_str()); }

int TextWidth(int id) {
    const std::wstring text = GetText(id);
    HDC dc = GetDC(g.window);
    HGDIOBJ old = SelectObject(dc, g.font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(g.window, dc);
    return size.cx;
}

HWND AddControl(const wchar_t* cls, const wchar_t* text, DWORD style, int id,
                int x = 0, int y = 0, int w = 10, int h = 10, DWORD exStyle = 0) {
    HWND handle = CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                                  x, y, w, h, g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                  GetModuleHandleW(nullptr), nullptr);
    SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return handle;
}

HFONT CreateUiFont() {
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    metrics.lfMessageFont.lfHeight = -16;
    wcscpy_s(metrics.lfMessageFont.lfFaceName, su::GetLanguageInfo(su::CurrentLanguage()).fontFace);
    return CreateFontIndirectW(&metrics.lfMessageFont);
}

// Builds an OPENFILENAME filter: alternating label/pattern parts, each NUL-terminated, plus a final NUL.
std::wstring FileFilter(std::initializer_list<std::wstring> parts) {
    std::wstring filter;
    for (const auto& part : parts) { filter += part; filter += L'\0'; }
    filter += L'\0';
    return filter;
}

void Status(const std::wstring& text) { SetText(IDC_STATUS, text); }

void AppendLog(const std::wstring& text) {
    HWND log = C(IDC_LOG);
    const int length = GetWindowTextLengthW(log);
    SendMessageW(log, EM_SETSEL, length, length);
    const std::wstring line = text + L"\r\n";
    SendMessageW(log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(line.c_str()));
    SendMessageW(log, EM_SCROLLCARET, 0, 0);
}

std::wstring ProfileSignature(const su::Profile& p) {
    return p.appId + L"|" + p.depotIds + L"|" + p.account + L"|" + p.contentRoot + L"|" +
           p.steamCmd + L"|" + p.description + L"|" + p.exclusions + L"|" + p.setLive;
}

su::Profile ReadForm() {
    su::Profile p;
    if (g.selected >= 0 && g.selected < static_cast<int>(g.profiles.size())) p.id = g.profiles[g.selected].id;
    if (p.id.empty()) p.id = su::NewId();
    p.name = su::Trim(GetText(IDC_NAME));
    p.appId = su::Trim(GetText(IDC_APP_ID));
    p.depotIds = su::Trim(GetText(IDC_DEPOT_IDS));
    p.account = su::Trim(GetText(IDC_ACCOUNT));
    p.contentRoot = su::Trim(GetText(IDC_CONTENT_ROOT));
    p.steamCmd = su::Trim(GetText(IDC_STEAM_CMD));
    p.description = su::Trim(GetText(IDC_DESCRIPTION));
    p.exclusions = GetText(IDC_EXCLUSIONS);
    p.setLive = su::Trim(GetText(IDC_SETLIVE));
    return p;
}

void EnableActions() {
    const BOOL idle = !g.busy;
    EnableWindow(C(IDC_PROFILE_LIST), idle);
    EnableWindow(C(IDC_NEW_PROFILE), idle);
    EnableWindow(C(IDC_COPY_PROFILE), idle);
    EnableWindow(C(IDC_DELETE_PROFILE), idle && g.profiles.size() > 1);
    EnableWindow(C(IDC_SAVE_PROFILE), idle);
    EnableWindow(C(IDC_IMPORT_PROFILE), idle);
    EnableWindow(C(IDC_EXPORT_PROFILE), idle);
    EnableWindow(C(IDC_LANGUAGE), idle);
    EnableWindow(C(IDC_LOGIN), idle);
    EnableWindow(C(IDC_SCAN), idle);
    EnableWindow(C(IDC_PREVIEW), idle);
    const bool signatureOkay = g.selected >= 0 && g.previewPassed &&
                               g.previewSignature == ProfileSignature(ReadForm());
    EnableWindow(C(IDC_UPLOAD), idle && signatureOkay);
    EnableWindow(C(IDC_CANCEL), g.busy);
}

void RefreshHistory() {
    HWND history = C(IDC_HISTORY);
    ListView_DeleteAllItems(history);
    if (g.selected < 0 || g.selected >= static_cast<int>(g.profiles.size())) return;
    auto lines = su::LoadHistory(g.profiles[g.selected].id);
    std::reverse(lines.begin(), lines.end());
    int row = 0;
    for (const auto& line : lines) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row++;
        item.pszText = const_cast<wchar_t*>(line.c_str());
        ListView_InsertItem(history, &item);
    }
}

void FillForm(int index) {
    if (index < 0 || index >= static_cast<int>(g.profiles.size())) return;
    g.selected = index;
    const auto& p = g.profiles[index];
    SetText(IDC_NAME, p.name);
    SetText(IDC_APP_ID, p.appId);
    SetText(IDC_DEPOT_IDS, p.depotIds);
    SetText(IDC_ACCOUNT, p.account);
    SetText(IDC_CONTENT_ROOT, p.contentRoot);
    SetText(IDC_STEAM_CMD, p.steamCmd);
    SetText(IDC_DESCRIPTION, p.description);
    SetText(IDC_EXCLUSIONS, p.exclusions);
    SetText(IDC_SETLIVE, p.setLive);
    g.previewPassed = false;
    g.previewSignature.clear();
    RefreshHistory();
    Status(Tr(Str::ProfileLoaded));
    EnableActions();
}

void RefreshProfileList(int selectIndex) {
    HWND list = C(IDC_PROFILE_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& p : g.profiles) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    selectIndex = std::clamp(selectIndex, 0, static_cast<int>(g.profiles.size()) - 1);
    SendMessageW(list, LB_SETCURSEL, selectIndex, 0);
    FillForm(selectIndex);
}

bool SaveProfiles(bool announce = true) {
    if (g.selected >= 0 && g.selected < static_cast<int>(g.profiles.size())) g.profiles[g.selected] = ReadForm();
    std::wstring error;
    if (!g.store.Save(g.profiles, error)) {
        MessageBoxW(g.window, error.c_str(), Tr(Str::SaveFailedTitle), MB_ICONERROR);
        return false;
    }
    const int selected = g.selected;
    RefreshProfileList(selected);
    if (announce) Status(Tr(Str::ProfileSaved));
    return true;
}

std::wstring SelectFolder(const wchar_t* title) {
    BROWSEINFOW info{};
    info.hwndOwner = g.window;
    info.lpszTitle = title;
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&info);
    if (!pidl) return {};
    wchar_t path[MAX_PATH]{};
    SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    return path;
}

std::wstring SelectExe() {
    const std::wstring filter = FileFilter({L"SteamCMD (steamcmd.exe)", L"steamcmd.exe",
                                            Tr(Str::FilterExecutables), L"*.exe"});
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW info{sizeof(info)};
    info.hwndOwner = g.window;
    info.lpstrFilter = filter.c_str();
    info.lpstrFile = file;
    info.nMaxFile = MAX_PATH;
    info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&info) ? file : L"";
}

std::wstring SelectProfileFile(bool save) {
    const std::wstring filter = FileFilter({Tr(Str::FilterProfiles), L"*.suprofile",
                                            Tr(Str::FilterAllFiles), L"*.*"});
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW info{sizeof(info)};
    info.hwndOwner = g.window;
    info.lpstrFilter = filter.c_str();
    info.lpstrFile = file;
    info.nMaxFile = MAX_PATH;
    info.lpstrDefExt = L"suprofile";
    info.Flags = OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return (save ? GetSaveFileNameW(&info) : GetOpenFileNameW(&info)) ? file : L"";
}

bool ValidateAndLog(const su::Profile& p) {
    Status(Tr(Str::Scanning));
    SetText(IDC_LOG, L"");
    AppendLog(su::Fmt(Str::CheckHeader, {su::NowText()}));
    const auto scan = su::ScanBuild(p);
    AppendLog(su::Fmt(Str::FileCount, {std::to_wstring(scan.fileCount)}));
    AppendLog(su::Fmt(Str::TotalSize, {su::FormatBytes(scan.totalBytes)}));
    AppendLog(su::Fmt(Str::ExcludedCount, {std::to_wstring(scan.excludedCount)}));
    if (!scan.mainExe.empty()) AppendLog(su::Fmt(Str::MainExe, {scan.mainExe}));
    for (const auto& warning : scan.warnings) AppendLog(Tr(Str::WarningPrefix) + warning);
    for (const auto& error : scan.errors) AppendLog(Tr(Str::ErrorPrefix) + error);
    if (scan.valid) {
        AppendLog(Tr(Str::CheckPassedLog));
        Status(Tr(scan.warnings.empty() ? Str::CheckPassed : Str::CheckPassedWarnings));
    } else {
        AppendLog(Tr(Str::CheckFailedLog));
        Status(Tr(Str::CheckFailed));
    }
    return scan.valid;
}

void SetBusy(bool busy, const std::wstring& status) {
    g.busy = busy;
    Status(status);
    SendMessageW(C(IDC_PROGRESS), PBM_SETMARQUEE, busy, 35);
    ShowWindow(C(IDC_PROGRESS), busy ? SW_SHOW : SW_HIDE);
    EnableActions();
}

void RunSteam(bool preview) {
    const su::Profile p = ReadForm();
    if (!ValidateAndLog(p)) return;
    if (!preview) {
        if (!g.previewPassed || g.previewSignature != ProfileSignature(p)) {
            MessageBoxW(g.window, Tr(Str::UploadBlockedMsg), Tr(Str::UploadBlockedTitle), MB_ICONWARNING);
            return;
        }
        const std::wstring setLive = p.setLive.empty() ? std::wstring(Tr(Str::SetLiveEmpty))
                                                       : su::Fmt(Str::SetLiveAuto, {p.setLive});
        const std::wstring warning = su::Fmt(Str::ConfirmUploadMsg, {p.name, p.appId, p.depotIds, p.contentRoot, setLive});
        if (MessageBoxW(g.window, warning.c_str(), Tr(Str::ConfirmUploadTitle), MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) return;
    }

    su::RunFiles files;
    std::wstring error;
    if (!su::GenerateVdf(p, preview, files, error)) {
        MessageBoxW(g.window, error.c_str(), Tr(Str::VdfFailedTitle), MB_ICONERROR);
        return;
    }
    AppendLog(Tr(preview ? Str::PreviewStartLog : Str::UploadStartLog));
    AppendLog(su::Fmt(Str::AppDepotLog, {p.appId, p.depotIds}));
    SetBusy(true, Tr(preview ? Str::PreviewRunning : Str::Uploading));
    g.steam.Prepare();
    g.worker = std::thread([p, files, preview]() {
        auto result = g.steam.Run(p, files, [](const std::wstring& line) {
            auto* copy = new std::wstring(line);
            PostMessageW(g.window, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(copy));
        });
        su::CleanupRunFiles(files);
        su::AppendHistory(p, preview, result);
        auto* payload = new DonePayload{preview, std::move(result)};
        PostMessageW(g.window, WM_APP_DONE, 0, reinterpret_cast<LPARAM>(payload));
    });
}

// Widths are measured from the current captions so every language fits without clipping.
void Layout(int width, int height) {
    const int margin = 14, sidebar = 215, gap = 12;
    const int mainX = margin + sidebar + gap;
    const int mainW = std::max(600, width - mainX - margin);

    MoveWindow(C(IDC_LABEL_PROFILES), margin, 18, sidebar, 22, TRUE);
    const int comboW = 150, comboX = mainX + mainW - comboW;
    const int languageLabelW = TextWidth(IDC_LABEL_LANGUAGE) + 4;
    MoveWindow(C(IDC_LABEL_LANGUAGE), comboX - languageLabelW - 8, 16, languageLabelW, 22, TRUE);
    MoveWindow(C(IDC_LANGUAGE), comboX, 12, comboW, 240, TRUE);

    MoveWindow(C(IDC_PROFILE_LIST), margin, 48, sidebar, std::max(180, height - 210), TRUE);
    const int buttonY = height - 148;
    const int thirdW = (sidebar - 12) / 3, halfW = (sidebar - 8) / 2;
    MoveWindow(C(IDC_NEW_PROFILE), margin, buttonY, thirdW, 30, TRUE);
    MoveWindow(C(IDC_COPY_PROFILE), margin + thirdW + 6, buttonY, thirdW, 30, TRUE);
    MoveWindow(C(IDC_DELETE_PROFILE), margin + 2 * (thirdW + 6), buttonY, sidebar - 2 * (thirdW + 6), 30, TRUE);
    MoveWindow(C(IDC_SAVE_PROFILE), margin, buttonY + 38, sidebar, 32, TRUE);
    MoveWindow(C(IDC_IMPORT_PROFILE), margin, buttonY + 78, halfW, 30, TRUE);
    MoveWindow(C(IDC_EXPORT_PROFILE), margin + sidebar - halfW, buttonY + 78, halfW, 30, TRUE);

    int labelW = std::max(80, TextWidth(IDC_LABEL_EXCLUSIONS) + 12);
    for (const auto& row : kFieldRows) labelW = std::max(labelW, TextWidth(row.label) + 12);
    const int fieldX = mainX + labelW, fieldW = mainW - labelW;
    const int browseW = std::max(72, TextWidth(IDC_BROWSE_CONTENT) + 24);
    int y = 48;
    for (const auto& row : kFieldRows) {
        const int w = row.browse ? fieldW - browseW - 6 : fieldW;
        MoveWindow(C(row.label), mainX, y + 4, labelW - 4, 22, TRUE);
        MoveWindow(C(row.edit), fieldX, y, w, 25, TRUE);
        if (row.browse) MoveWindow(C(row.browse), fieldX + w + 6, y, browseW, 25, TRUE);
        y += 31;
    }
    MoveWindow(C(IDC_LABEL_EXCLUSIONS), mainX, y + 4, labelW - 4, 22, TRUE);
    MoveWindow(C(IDC_EXCLUSIONS), fieldX, y, fieldW, 84, TRUE);
    y += 94;

    int bx = mainX;
    for (int id : kActionButtons) {
        const int w = std::max(id >= IDC_OPEN_BUILDS ? 126 : 104, TextWidth(id) + 28);
        if (bx > mainX && bx + w > mainX + mainW) { bx = mainX; y += 36; }
        MoveWindow(C(id), bx, y, w, 30, TRUE);
        bx += w + 6;
    }
    y += 38;
    MoveWindow(C(IDC_STATUS), mainX, y, mainW, 22, TRUE);
    MoveWindow(C(IDC_PROGRESS), mainX, y + 23, mainW, 6, TRUE);
    y += 36;
    const int remaining = std::max(120, height - y - margin);
    const int logH = remaining * 62 / 100;
    MoveWindow(C(IDC_LOG), mainX, y, mainW, logH, TRUE);
    MoveWindow(C(IDC_HISTORY), mainX, y + logH + 8, mainW, remaining - logH - 8, TRUE);
}

void ApplyLanguage() {
    HFONT oldFont = g.font;
    g.font = CreateUiFont();
    EnumChildWindows(g.window, [](HWND child, LPARAM font) {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(g.font));
    if (oldFont) DeleteObject(oldFont);

    SetWindowTextW(g.window, Tr(Str::AppTitle));
    for (const auto& binding : kTextBindings) SetText(binding.id, Tr(binding.text));
    if (!g.busy) Status(Tr(Str::Ready));
    RECT rect{}; GetClientRect(g.window, &rect); Layout(rect.right, rect.bottom);
    InvalidateRect(g.window, nullptr, TRUE);
}

void CreateUi(HWND window) {
    g.window = window;
    g.font = CreateUiFont();
    g.panelBrush = CreateSolidBrush(RGB(248, 249, 251));

    // Captions are filled in by ApplyLanguage(); positions by Layout().
    AddControl(L"STATIC", L"", SS_LEFT, IDC_LABEL_PROFILES);
    AddControl(L"STATIC", L"", SS_RIGHT, IDC_LABEL_LANGUAGE);
    HWND language = AddControl(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_LANGUAGE);
    for (int i = 0; i < static_cast<int>(su::Lang::Count); ++i)
        SendMessageW(language, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(su::GetLanguageInfo(static_cast<su::Lang>(i)).nativeName));
    SendMessageW(language, CB_SETCURSEL, static_cast<WPARAM>(su::CurrentLanguage()), 0);

    AddControl(WC_LISTBOXW, L"", LBS_NOTIFY | WS_BORDER | WS_VSCROLL, IDC_PROFILE_LIST, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_NEW_PROFILE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_COPY_PROFILE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_DELETE_PROFILE);
    AddControl(L"BUTTON", L"", BS_DEFPUSHBUTTON, IDC_SAVE_PROFILE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_IMPORT_PROFILE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_EXPORT_PROFILE);

    for (const auto& row : kFieldRows) AddControl(L"STATIC", L"", SS_LEFT, row.label);
    AddControl(L"STATIC", L"", SS_LEFT, IDC_LABEL_EXCLUSIONS);

    const int editStyle = WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER;
    AddControl(L"EDIT", L"", editStyle, IDC_NAME, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"EDIT", L"", editStyle | ES_NUMBER, IDC_APP_ID, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"EDIT", L"", editStyle, IDC_DEPOT_IDS, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"EDIT", L"", editStyle, IDC_ACCOUNT, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"EDIT", L"", editStyle, IDC_CONTENT_ROOT, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_BROWSE_CONTENT);
    AddControl(L"EDIT", L"", editStyle, IDC_STEAM_CMD, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"BUTTON", L"", BS_PUSHBUTTON, IDC_BROWSE_STEAM);
    AddControl(L"EDIT", L"", editStyle, IDC_DESCRIPTION, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"EDIT", L"", editStyle, IDC_SETLIVE, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    AddControl(L"EDIT", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
               IDC_EXCLUSIONS, 0, 0, 10, 10, WS_EX_CLIENTEDGE);

    for (int id : kActionButtons) AddControl(L"BUTTON", L"", BS_PUSHBUTTON, id);
    AddControl(L"STATIC", L"", SS_LEFT, IDC_STATUS);
    AddControl(PROGRESS_CLASSW, L"", PBS_MARQUEE, IDC_PROGRESS);
    ShowWindow(C(IDC_PROGRESS), SW_HIDE);
    const std::wstring logReady = std::wstring(Tr(Str::LogReady)) + L"\r\n";
    AddControl(L"EDIT", logReady.c_str(), WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
               IDC_LOG, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    HWND history = AddControl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER,
                              IDC_HISTORY, 0, 0, 10, 10, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(history, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    LVCOLUMNW column{};
    column.mask = LVCF_WIDTH;
    column.cx = 900;
    ListView_InsertColumn(history, 0, &column);

    ApplyLanguage();
    g.profiles = g.store.Load();
    RefreshProfileList(0);
}

void ChangeLanguage() {
    const auto index = SendMessageW(C(IDC_LANGUAGE), CB_GETCURSEL, 0, 0);
    if (index < 0 || index >= static_cast<LRESULT>(su::Lang::Count)) return;
    const auto lang = static_cast<su::Lang>(index);
    if (lang == su::CurrentLanguage()) return;
    su::SetLanguage(lang);
    su::WriteSetting(L"Language", su::GetLanguageInfo(lang).code);
    ApplyLanguage();
}

void Command(int id, int code) {
    if (id == IDC_PROFILE_LIST && code == LBN_SELCHANGE) {
        const int selected = static_cast<int>(SendMessageW(C(IDC_PROFILE_LIST), LB_GETCURSEL, 0, 0));
        FillForm(selected);
        return;
    }
    if (id == IDC_LANGUAGE) {
        if (code == CBN_SELCHANGE) ChangeLanguage();
        return;
    }
    switch (id) {
    case IDC_NEW_PROFILE: {
        // Keep the SteamCMD path: it is usually shared by every game on the machine.
        su::Profile p = su::ProfileStore::DefaultProfile();
        p.steamCmd = su::Trim(GetText(IDC_STEAM_CMD));
        g.profiles.push_back(std::move(p)); RefreshProfileList(static_cast<int>(g.profiles.size()) - 1); break;
    }
    case IDC_COPY_PROFILE: {
        if (g.selected < 0) break;
        su::Profile p = ReadForm(); p.id = su::NewId(); p.name += Tr(Str::CopySuffix);
        g.profiles.push_back(std::move(p)); RefreshProfileList(static_cast<int>(g.profiles.size()) - 1); break;
    }
    case IDC_DELETE_PROFILE:
        if (g.profiles.size() > 1 && MessageBoxW(g.window, Tr(Str::ConfirmDeleteMsg), Tr(Str::ConfirmDeleteTitle), MB_YESNO | MB_ICONWARNING) == IDYES) {
            g.profiles.erase(g.profiles.begin() + g.selected); RefreshProfileList(std::max(0, g.selected - 1)); SaveProfiles(false);
        } break;
    case IDC_SAVE_PROFILE: SaveProfiles(); break;
    case IDC_IMPORT_PROFILE: {
        const auto file = SelectProfileFile(false);
        if (file.empty()) break;
        su::Profile p; std::wstring error;
        if (!g.store.Import(file, p, error)) MessageBoxW(g.window, error.c_str(), Tr(Str::ImportFailedTitle), MB_ICONERROR);
        else { g.profiles.push_back(std::move(p)); RefreshProfileList(static_cast<int>(g.profiles.size()) - 1); SaveProfiles(false); Status(Tr(Str::Imported)); }
        break;
    }
    case IDC_EXPORT_PROFILE: {
        const auto file = SelectProfileFile(true);
        if (file.empty()) break;
        std::wstring error;
        if (!g.store.Export(ReadForm(), file, error)) MessageBoxW(g.window, error.c_str(), Tr(Str::ExportFailedTitle), MB_ICONERROR);
        else Status(su::Fmt(Str::Exported, {file}));
        break;
    }
    case IDC_BROWSE_CONTENT: { auto path = SelectFolder(Tr(Str::BrowseContentTitle)); if (!path.empty()) SetText(IDC_CONTENT_ROOT, path); break; }
    case IDC_BROWSE_STEAM: { auto path = SelectExe(); if (!path.empty()) SetText(IDC_STEAM_CMD, path); break; }
    case IDC_LOGIN: {
        std::wstring error;
        if (g.steam.LaunchInteractiveLogin(ReadForm(), error))
            MessageBoxW(g.window, Tr(Str::LoginOpened), Tr(Str::LoginTitle), MB_ICONINFORMATION);
        else MessageBoxW(g.window, error.c_str(), Tr(Str::LoginFailedTitle), MB_ICONERROR);
        break;
    }
    case IDC_SCAN: ValidateAndLog(ReadForm()); break;
    case IDC_PREVIEW: RunSteam(true); break;
    case IDC_UPLOAD: RunSteam(false); break;
    case IDC_CANCEL:
        if (MessageBoxW(g.window, Tr(Str::ConfirmCancelMsg), Tr(Str::ConfirmCancelTitle), MB_YESNO | MB_ICONWARNING) == IDYES) {
            Status(Tr(Str::Cancelling)); g.steam.Cancel();
        } break;
    case IDC_OPEN_BUILDS: {
        auto p = ReadForm(); if (!p.appId.empty()) su::OpenTarget(L"https://partner.steamgames.com/apps/builds/" + p.appId); break;
    }
    case IDC_OPEN_DATA: su::OpenTarget(su::AppDataRoot().wstring()); break;
    default:
        if (code == EN_CHANGE && id >= IDC_NAME && id <= IDC_SETLIVE) EnableActions();
        break;
    }
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: CreateUi(window); return 0;
    case WM_SIZE: Layout(LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_COMMAND: Command(LOWORD(wParam), HIWORD(wParam)); return 0;
    case WM_APP_LOG: {
        std::unique_ptr<std::wstring> line(reinterpret_cast<std::wstring*>(lParam));
        AppendLog(*line); return 0;
    }
    case WM_APP_DONE: {
        std::unique_ptr<DonePayload> payload(reinterpret_cast<DonePayload*>(lParam));
        if (g.worker.joinable()) g.worker.join();
        SetBusy(false, Tr(Str::TaskDone));
        if (payload->preview && payload->result.exitCode == 0 && !payload->result.cancelled) {
            g.previewPassed = true;
            g.previewSignature = ProfileSignature(ReadForm());
            Status(Tr(Str::PreviewPassed));
            AppendLog(Tr(Str::PreviewPassedLog));
        } else if (!payload->preview && payload->result.exitCode == 0) {
            Status(Tr(Str::UploadSucceeded));
            AppendLog(su::Fmt(Str::UploadSucceededLog, {payload->result.buildId.empty() ? std::wstring(Tr(Str::BuildIdUnknown)) : payload->result.buildId}));
            g.previewPassed = false; g.previewSignature.clear();
        } else {
            Status(payload->result.error.empty() ? std::wstring(Tr(Str::TaskFailed)) : payload->result.error);
            AppendLog(Tr(Str::FailedPrefix) + payload->result.error);
        }
        RefreshHistory(); EnableActions();
        if (g.exitRequested) DestroyWindow(window);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(248,249,251));
        return reinterpret_cast<LRESULT>(g.panelBrush);
    case WM_CLOSE:
        if (g.busy && MessageBoxW(window, Tr(Str::ConfirmExitMsg), Tr(Str::ConfirmExitTitle), MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
        if (g.busy) { g.exitRequested = true; Status(Tr(Str::ExitingSafely)); g.steam.Cancel(); return 0; }
        DestroyWindow(window); return 0;
    case WM_DESTROY:
        if (g.worker.joinable()) g.worker.join();
        if (g.font) DeleteObject(g.font);
        if (g.panelBrush) DeleteObject(g.panelBrush);
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);

    su::Lang language = su::DetectSystemLanguage();
    su::LanguageFromCode(su::ReadSetting(L"Language"), language);
    su::SetLanguage(language);

    WNDCLASSEXW cls{sizeof(cls)};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    cls.hbrBackground = CreateSolidBrush(RGB(248, 249, 251));
    cls.lpszClassName = L"SteamUploaderMainWindow";
    cls.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&cls);

    HWND window = CreateWindowExW(0, cls.lpszClassName, Tr(Str::AppTitle),
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1220, 900,
                                  nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    CoUninitialize();
    return static_cast<int>(message.wParam);
}

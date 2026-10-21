#include "I18n.h"

#include <windows.h>

#include <atomic>
#include <iterator>

namespace su {
namespace {

constexpr size_t kLangCount = static_cast<size_t>(Lang::Count);

constexpr LanguageInfo kLanguages[] = {
    {L"en", L"English", L"Segoe UI"},
    {L"zh-CN", L"简体中文", L"Microsoft YaHei UI"},
    {L"ja", L"日本語", L"Yu Gothic UI"},
};
static_assert(std::size(kLanguages) == kLangCount, "Every language needs a LanguageInfo entry.");

struct Entry {
    Str id;
    const wchar_t* text[kLangCount]; // English, Simplified Chinese, Japanese
};

// To add a language: extend su::Lang, kLanguages, and append one column to every row below.
constexpr Entry kTable[] = {
    {Str::AppTitle, {L"Steam Game Uploader", L"Steam 多游戏上传工具", L"Steam ゲームアップローダー"}},
    {Str::LanguageLabel, {L"Language", L"语言", L"言語"}},
    {Str::ProfilesHeader, {L"Game Profiles", L"游戏配置", L"ゲームプロファイル"}},
    {Str::NewProfile, {L"New", L"新建", L"新規"}},
    {Str::CopyProfile, {L"Copy", L"复制", L"複製"}},
    {Str::DeleteProfile, {L"Delete", L"删除", L"削除"}},
    {Str::SaveProfile, {L"Save Profile", L"保存当前配置", L"プロファイルを保存"}},
    {Str::ImportProfile, {L"Import…", L"导入配置", L"インポート"}},
    {Str::ExportProfile, {L"Export…", L"导出配置", L"エクスポート"}},
    {Str::LabelName, {L"Profile Name", L"配置名称", L"プロファイル名"}},
    {Str::LabelAppId, {L"App ID", L"AppID", L"App ID"}},
    {Str::LabelDepotIds, {L"Depot IDs", L"Depot ID", L"Depot ID"}},
    {Str::LabelAccount, {L"Steam Account", L"Steam 账号", L"Steam アカウント"}},
    {Str::LabelContentRoot, {L"Content Root", L"构建目录", L"コンテンツルート"}},
    {Str::LabelSteamCmd, {L"SteamCMD", L"SteamCMD", L"SteamCMD"}},
    {Str::LabelDescription, {L"Description", L"构建描述", L"ビルド説明"}},
    {Str::LabelSetLive, {L"SetLive Branch", L"SetLive 分支", L"SetLive ブランチ"}},
    {Str::LabelExclusions, {L"Exclusions", L"排除规则", L"除外ルール"}},
    {Str::Browse, {L"Browse…", L"浏览…", L"参照…"}},
    {Str::Login, {L"Log In", L"登录认证", L"ログイン"}},
    {Str::Scan, {L"Check Build", L"发布检查", L"ビルド検査"}},
    {Str::Preview, {L"Preview", L"Preview", L"プレビュー"}},
    {Str::Upload, {L"Upload", L"正式上传", L"本番アップロード"}},
    {Str::Cancel, {L"Cancel", L"取消任务", L"キャンセル"}},
    {Str::OpenBuilds, {L"Steamworks Builds", L"Steam 构建后台", L"Steam ビルド管理"}},
    {Str::OpenData, {L"Data Folder", L"打开数据目录", L"データフォルダー"}},
    {Str::Ready, {L"Ready", L"就绪", L"準備完了"}},
    {Str::LogReady, {L"SteamUploader is ready.", L"SteamUploader 已就绪。", L"SteamUploader の準備ができました。"}},

    {Str::ProfileLoaded, {L"Profile loaded. Save any changes and run Preview again.",
                          L"已载入配置。修改后请保存并重新运行 Preview。",
                          L"プロファイルを読み込みました。変更したら保存して、プレビューを再実行してください。"}},
    {Str::ProfileSaved, {L"Profile saved. Run Preview again before uploading.",
                         L"配置已保存。正式上传前需要重新执行 Preview。",
                         L"プロファイルを保存しました。本番アップロードの前にプレビューを再実行してください。"}},
    {Str::SaveFailedTitle, {L"Save Failed", L"保存失败", L"保存に失敗しました"}},
    {Str::FilterExecutables, {L"Executables (*.exe)", L"可执行文件 (*.exe)", L"実行ファイル (*.exe)"}},
    {Str::FilterProfiles, {L"SteamUploader profile (*.suprofile)", L"SteamUploader 配置 (*.suprofile)",
                           L"SteamUploader プロファイル (*.suprofile)"}},
    {Str::FilterAllFiles, {L"All files (*.*)", L"所有文件 (*.*)", L"すべてのファイル (*.*)"}},
    {Str::BrowseContentTitle, {L"Select the content root folder of the packaged game",
                               L"选择已打包游戏的内容根目录",
                               L"パッケージ済みゲームのコンテンツルートフォルダーを選択"}},
    {Str::NewProfileName, {L"New Game", L"新游戏配置", L"新しいゲーム"}},
    {Str::CopySuffix, {L" - Copy", L" - 副本", L" - コピー"}},
    {Str::ConfirmDeleteMsg, {L"Delete the current profile?", L"确认删除当前配置？", L"現在のプロファイルを削除しますか？"}},
    {Str::ConfirmDeleteTitle, {L"Delete Profile", L"删除配置", L"プロファイルの削除"}},
    {Str::ImportFailedTitle, {L"Import Failed", L"导入失败", L"インポートに失敗しました"}},
    {Str::Imported, {L"Profile imported.", L"配置导入成功。", L"プロファイルをインポートしました。"}},
    {Str::ExportFailedTitle, {L"Export Failed", L"导出失败", L"エクスポートに失敗しました"}},
    {Str::Exported, {L"Profile exported: {0}", L"配置已导出：{0}", L"プロファイルをエクスポートしました：{0}"}},
    {Str::LoginOpened, {L"A SteamCMD login window has been opened. Enter your password and Steam Guard code in that window; this tool never reads or stores your credentials.",
                        L"已打开 SteamCMD 登录窗口。请在该窗口输入密码和 Steam Guard 验证码；本工具不会读取或保存凭据。",
                        L"SteamCMD のログインウィンドウを開きました。パスワードと Steam Guard コードはそのウィンドウで入力してください。このツールが認証情報を読み取ったり保存したりすることはありません。"}},
    {Str::LoginTitle, {L"Steam Login", L"Steam 登录", L"Steam ログイン"}},
    {Str::LoginFailedTitle, {L"Cannot Log In", L"无法登录", L"ログインできません"}},

    {Str::Scanning, {L"Scanning the content root…", L"正在扫描构建目录……", L"コンテンツルートをスキャンしています…"}},
    {Str::CheckHeader, {L"=== Build check {0} ===", L"=== 发布检查 {0} ===", L"=== ビルド検査 {0} ==="}},
    {Str::FileCount, {L"Files: {0}", L"文件数量：{0}", L"ファイル数：{0}"}},
    {Str::TotalSize, {L"Total size: {0}", L"内容体积：{0}", L"合計サイズ：{0}"}},
    {Str::ExcludedCount, {L"Excluded files: {0}", L"排除文件：{0}", L"除外ファイル：{0}"}},
    {Str::MainExe, {L"Main executable: {0}", L"检测到主程序：{0}", L"メイン実行ファイル：{0}"}},
    {Str::WarningPrefix, {L"[Warning] ", L"[警告] ", L"[警告] "}},
    {Str::ErrorPrefix, {L"[Error] ", L"[错误] ", L"[エラー] "}},
    {Str::CheckPassedLog, {L"[Passed] Build check complete.", L"[通过] 发布检查完成。", L"[合格] ビルド検査が完了しました。"}},
    {Str::CheckPassed, {L"Build check passed.", L"发布检查通过。", L"ビルド検査に合格しました。"}},
    {Str::CheckPassedWarnings, {L"Build check passed with warnings. See the log.",
                                L"发布检查通过，但存在警告，请查看日志。",
                                L"ビルド検査に合格しましたが、警告があります。ログを確認してください。"}},
    {Str::CheckFailedLog, {L"[Failed] Fix the errors and try again.", L"[失败] 请修正错误后重试。",
                           L"[失敗] エラーを修正してから再試行してください。"}},
    {Str::CheckFailed, {L"Build check failed.", L"发布检查失败。", L"ビルド検査に失敗しました。"}},

    {Str::UploadBlockedMsg, {L"This profile has not passed Preview, or it was changed after Preview.",
                             L"当前配置尚未通过 Preview，或 Preview 后配置已改变。",
                             L"このプロファイルはプレビューに合格していないか、プレビュー後に変更されています。"}},
    {Str::UploadBlockedTitle, {L"Upload Blocked", L"禁止上传", L"アップロードできません"}},
    {Str::ConfirmUploadMsg, {L"You are about to upload this build:\n\nProfile: {0}\nApp ID: {1}\nDepots: {2}\nContent root: {3}\nSetLive: {4}\n\nThis will transfer files to Steam. Continue?",
                             L"即将正式上传以下构建：\n\n配置：{0}\nAppID：{1}\nDepot：{2}\n目录：{3}\nSetLive：{4}\n\n此操作会向 Steam 传输文件。确认继续？",
                             L"次のビルドを本番アップロードします：\n\nプロファイル：{0}\nApp ID：{1}\nDepot：{2}\nコンテンツルート：{3}\nSetLive：{4}\n\nSteam にファイルを転送します。続行しますか？"}},
    {Str::SetLiveEmpty, {L"(empty, the build will not go live automatically)", L"空（不自动上线）",
                         L"空（自動では公開されません）"}},
    {Str::SetLiveAuto, {L"{0} (set live automatically after a successful upload)", L"{0}（上传成功后自动推送）",
                        L"{0}（アップロード成功後に自動で公開）"}},
    {Str::ConfirmUploadTitle, {L"Confirm Upload", L"确认正式上传", L"本番アップロードの確認"}},
    {Str::VdfFailedTitle, {L"VDF Generation Failed", L"VDF 生成失败", L"VDF の生成に失敗しました"}},
    {Str::PreviewStartLog, {L"=== Starting SteamPipe Preview ===", L"=== 启动 SteamPipe Preview ===",
                            L"=== SteamPipe プレビューを開始 ==="}},
    {Str::UploadStartLog, {L"=== Starting upload ===", L"=== 启动正式上传 ===", L"=== 本番アップロードを開始 ==="}},
    {Str::AppDepotLog, {L"App ID: {0}; Depots: {1}", L"AppID：{0}；Depot：{1}", L"App ID：{0}；Depot：{1}"}},
    {Str::PreviewRunning, {L"SteamPipe Preview is running…", L"SteamPipe Preview 正在运行……",
                           L"SteamPipe プレビューを実行しています…"}},
    {Str::Uploading, {L"Uploading to Steam…", L"正在上传到 Steam……", L"Steam にアップロードしています…"}},
    {Str::ConfirmCancelMsg, {L"Cancel the running SteamCMD task?", L"确认取消正在运行的 SteamCMD 任务？",
                             L"実行中の SteamCMD タスクをキャンセルしますか？"}},
    {Str::ConfirmCancelTitle, {L"Cancel Task", L"取消任务", L"タスクのキャンセル"}},
    {Str::Cancelling, {L"Cancelling…", L"正在取消……", L"キャンセルしています…"}},
    {Str::TaskDone, {L"Task finished.", L"任务完成。", L"タスクが完了しました。"}},
    {Str::PreviewPassed, {L"Preview passed. Upload is unlocked.", L"Preview 通过，已解锁正式上传。",
                          L"プレビューに合格しました。本番アップロードが可能になりました。"}},
    {Str::PreviewPassedLog, {L"[Success] Preview passed. The Upload button is now unlocked.",
                             L"[成功] Preview 通过。正式上传按钮已解锁。",
                             L"[成功] プレビューに合格しました。本番アップロードボタンが有効になりました。"}},
    {Str::UploadSucceeded, {L"Upload succeeded. Set the build live on a branch in Steamworks.",
                            L"正式上传成功。构建尚需在 Steamworks 后台选择分支。",
                            L"本番アップロードに成功しました。Steamworks でビルドをブランチに設定してください。"}},
    {Str::UploadSucceededLog, {L"[Success] Upload complete. Build ID: {0}", L"[成功] 上传完成。Build ID：{0}",
                               L"[成功] アップロードが完了しました。Build ID：{0}"}},
    {Str::BuildIdUnknown, {L"see the SteamCMD log", L"请查看 SteamCMD 日志", L"SteamCMD のログを確認してください"}},
    {Str::TaskFailed, {L"Task failed. See the log.", L"任务失败，请查看日志。", L"タスクが失敗しました。ログを確認してください。"}},
    {Str::FailedPrefix, {L"[Failed] ", L"[失败] ", L"[失敗] "}},
    {Str::ConfirmExitMsg, {L"An upload task is still running. Closing the app will cancel it. Exit anyway?",
                           L"上传任务仍在运行。关闭程序会取消任务，确认退出？",
                           L"アップロードタスクが実行中です。閉じるとタスクはキャンセルされます。終了しますか？"}},
    {Str::ConfirmExitTitle, {L"Confirm Exit", L"确认退出", L"終了の確認"}},
    {Str::ExitingSafely, {L"Cancelling the task and exiting safely…", L"正在取消任务并安全退出……",
                          L"タスクをキャンセルして安全に終了しています…"}},

    {Str::ErrEncodeVdf, {L"Could not encode the VDF file.", L"无法编码 VDF 文件。", L"VDF ファイルをエンコードできません。"}},
    {Str::ErrWriteFile, {L"Could not write file: {0}", L"无法写入文件：{0}", L"ファイルを書き込めません：{0}"}},
    {Str::ErrUnknownWindows, {L"Unknown Windows error", L"未知 Windows 错误", L"不明な Windows エラー"}},
    {Str::ErrOverwriteProfiles, {L"Could not overwrite the profile file: {0}", L"无法覆盖配置文件：{0}",
                                 L"プロファイルファイルを上書きできません：{0}"}},
    {Str::ErrCreateProfiles, {L"Could not create the Unicode profile file: {0}", L"无法创建 Unicode 配置文件：{0}",
                              L"Unicode プロファイルファイルを作成できません：{0}"}},
    {Str::ErrSaveProfiles, {L"Could not save profiles: {0}", L"无法保存配置：{0}", L"プロファイルを保存できません：{0}"}},
    {Str::ErrWriteField, {L"Could not write profile field: {0}", L"写入配置字段失败：{0}",
                          L"プロファイル項目を書き込めません：{0}"}},
    {Str::ErrOverwriteExport, {L"Could not overwrite the export file: {0}", L"无法覆盖导出文件：{0}",
                               L"エクスポートファイルを上書きできません：{0}"}},
    {Str::ErrCreateExport, {L"Could not create the export file: {0}", L"无法创建导出文件：{0}",
                            L"エクスポートファイルを作成できません：{0}"}},
    {Str::ErrExportField, {L"Could not export profile field: {0}", L"导出配置字段失败：{0}",
                           L"プロファイル項目をエクスポートできません：{0}"}},
    {Str::ErrImportMissing, {L"The profile file does not exist.", L"配置文件不存在。", L"プロファイルファイルが存在しません。"}},
    {Str::ErrImportFormat, {L"Unsupported profile file format.", L"不支持的配置文件格式。",
                            L"サポートされていないプロファイル形式です。"}},
    {Str::ErrImportNoName, {L"The imported profile has no name.", L"导入配置缺少名称。",
                            L"インポートしたプロファイルに名前がありません。"}},
    {Str::ErrContentRoot, {L"The content root does not exist or cannot be read.", L"构建目录不存在或无法读取。",
                           L"コンテンツルートが存在しないか、読み取れません。"}},
    {Str::ErrSteamCmdPath, {L"The SteamCMD path is invalid.", L"SteamCMD 路径无效。", L"SteamCMD のパスが無効です。"}},
    {Str::ErrNameEmpty, {L"Profile name cannot be empty.", L"配置名称不能为空。", L"プロファイル名は空にできません。"}},
    {Str::ErrAppIdNumeric, {L"App ID must be a number.", L"AppID 必须是数字。", L"App ID は数字で入力してください。"}},
    {Str::ErrNoDepot, {L"At least one Depot ID is required.", L"至少需要一个 Depot ID。", L"Depot ID が 1 つ以上必要です。"}},
    {Str::ErrDepotNumeric, {L"Depot ID must be a number: {0}", L"Depot ID 必须是数字：{0}",
                            L"Depot ID は数字で入力してください：{0}"}},
    {Str::ErrNoFiles, {L"No files to upload were found.", L"没有找到可上传文件。",
                       L"アップロードするファイルが見つかりません。"}},
    {Str::ErrNoExe, {L"No .exe main executable was found in the upload content.", L"上传内容中没有找到 .exe 主程序。",
                     L"アップロード内容に .exe のメイン実行ファイルが見つかりません。"}},
    {Str::ErrStrayAppId, {L"steam_appid.txt is not excluded. Uploading it is not allowed.",
                          L"steam_appid.txt 未被排除，禁止上传。",
                          L"steam_appid.txt が除外されていません。アップロードできません。"}},
    {Str::WarnSavedContent, {L"Found Saved content that is not excluded; it may contain logs, config files, or test saves.",
                             L"发现未排除的 Saved 内容，可能包含日志、配置或测试存档。",
                             L"除外されていない Saved フォルダーの内容があります。ログ、設定、テスト用セーブデータが含まれている可能性があります。"}},
    {Str::WarnSetLive, {L"SetLive is set. A successful upload will set the build live on that branch automatically.",
                        L"SetLive 已填写，上传成功会自动推送指定分支。",
                        L"SetLive が設定されています。アップロードに成功すると、そのブランチで自動的に公開されます。"}},
    {Str::ErrRunDirectory, {L"Could not create the run folder (system error {0}).", L"无法创建运行目录，系统错误码：{0}",
                            L"実行用フォルダーを作成できません（システムエラー {0}）。"}},
    {Str::ErrNoAccount, {L"Enter the Steamworks account first.", L"请先填写 Steamworks 账号。",
                         L"先に Steamworks アカウントを入力してください。"}},
    {Str::ErrStartSteamCmd, {L"Could not start SteamCMD: {0}", L"无法启动 SteamCMD：{0}", L"SteamCMD を起動できません：{0}"}},
    {Str::TaskCancelled, {L"Task cancelled by user.", L"任务已由用户取消。", L"タスクはユーザーによってキャンセルされました。"}},
    {Str::SteamCmdExitCode, {L"SteamCMD exited with code {0}.", L"SteamCMD 返回退出码 {0}。",
                             L"SteamCMD が終了コード {0} で終了しました。"}},
};

constexpr bool TableMatchesEnum() {
    for (size_t i = 0; i < std::size(kTable); ++i) {
        if (static_cast<size_t>(kTable[i].id) != i) return false;
        for (const wchar_t* text : kTable[i].text) {
            if (text == nullptr) return false;
        }
    }
    return true;
}
static_assert(std::size(kTable) == static_cast<size_t>(Str::Count), "kTable is missing strings.");
static_assert(TableMatchesEnum(), "kTable rows must follow the order of su::Str and translate every language.");

std::atomic<int> g_current{static_cast<int>(Lang::English)};

} // namespace

const LanguageInfo& GetLanguageInfo(Lang lang) {
    return kLanguages[static_cast<size_t>(lang)];
}

Lang CurrentLanguage() { return static_cast<Lang>(g_current.load()); }

void SetLanguage(Lang lang) {
    if (lang >= Lang::English && lang < Lang::Count) g_current = static_cast<int>(lang);
}

bool LanguageFromCode(const std::wstring& code, Lang& lang) {
    for (size_t i = 0; i < kLangCount; ++i) {
        if (_wcsicmp(code.c_str(), kLanguages[i].code) == 0) {
            lang = static_cast<Lang>(i);
            return true;
        }
    }
    return false;
}

Lang DetectSystemLanguage() {
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_CHINESE: return Lang::ChineseSimplified;
    case LANG_JAPANESE: return Lang::Japanese;
    default: return Lang::English;
    }
}

const wchar_t* Tr(Str id) {
    return kTable[static_cast<size_t>(id)].text[static_cast<size_t>(g_current.load())];
}

std::wstring Fmt(Str id, std::initializer_list<std::wstring> args) {
    // Single pass, so a placeholder inside an argument (e.g. a path with "{1}") is left alone.
    const std::wstring pattern = Tr(id);
    std::wstring out;
    out.reserve(pattern.size() + 64);
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == L'{' && i + 2 < pattern.size() && pattern[i + 1] >= L'0' && pattern[i + 1] <= L'9' &&
            pattern[i + 2] == L'}') {
            const size_t index = static_cast<size_t>(pattern[i + 1] - L'0');
            if (index < args.size()) {
                out += *(args.begin() + index);
                i += 2;
                continue;
            }
        }
        out += pattern[i];
    }
    return out;
}

} // namespace su

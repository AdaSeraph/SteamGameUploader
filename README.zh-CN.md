# Steam Game Uploader

[English](README.md) | **简体中文**

一个通过 SteamPipe（SteamCMD）向 Steam 上传游戏构建的原生 Windows 小工具。每个游戏一份配置，上传前先检查构建目录；只有相同配置的 SteamPipe Preview 成功之后，才会解锁正式上传。

- 原生 Win32 / C++20，单个约 500 KB 的 `.exe`，无需安装 .NET、Qt 或 VC++ 运行库。
- 从不要求、也不保存 Steam 密码和 Steam Guard 验证码。
- 界面支持 English、简体中文、日本語。

## 功能

- **多游戏配置**：AppID、Depot ID、Steamworks 账号、构建目录、SteamCMD 路径、构建描述、排除规则、SetLive 分支；可导入/导出为 `.suprofile` 文件。
- **发布检查**：统计文件数量与体积，检测主程序 `.exe`；若 `steam_appid.txt` 会被上传则直接拦截；对残留的 `Saved` 内容和已填写的 SetLive 给出警告。
- **先 Preview**：SteamPipe Preview 通过前，“正式上传”按钮保持禁用；之后修改任何上传相关设置都会重新锁定。
- **上传前二次确认**：每次上传前弹出摘要对话框。SetLive 默认为空，不会误推正式分支。
- **实时 SteamCMD 日志**、按配置记录的上传历史，以及能终止 SteamCMD 及其全部子进程的“取消任务”。

## 下载

从 [Releases](https://github.com/AdaSeraph/SteamGameUploader/releases) 下载最新的 `SteamGameUploader-<版本>-win-x64.zip`，解压后运行 `SteamUploader.exe`。每个压缩包旁都附有 `.sha256` 文件，可用于校验下载。

运行要求：

- Windows 10 或 11，x64。
- `steamcmd.exe`：随 [Steamworks SDK](https://partner.steamgames.com/doc/sdk) 提供，位于 `sdk/tools/ContentBuilder/builder/steamcmd.exe`；也可以单独下载 [SteamCMD](https://developer.valvesoftware.com/wiki/SteamCMD)。
- 拥有该应用构建上传权限的 Steamworks 账号。

> 程序没有代码签名，首次运行时 Windows SmartScreen 可能会提示风险。选择“更多信息 → 仍要运行”即可，或者自行从源码构建。

## 快速上手

1. 启动 `SteamUploader.exe`。首次运行会创建一个空白的“新游戏配置”。
2. 填写 **AppID**、**Depot ID**（逗号分隔）、**Steam 账号**、**构建目录**（已打包游戏所在的文件夹）以及 **SteamCMD** 路径。
3. 在 **排除规则** 中每行填写一条规则，务必保留 `steam_appid.txt`。
4. 点击 **保存当前配置**。
5. 点击 **登录认证**，会弹出独立的 SteamCMD 控制台窗口，在其中输入密码和 Steam Guard 验证码。SteamCMD 会缓存登录状态；Preview 和正式上传在后台无交互地运行 SteamCMD，因此必须先完成这一步。缓存失效后重新登录即可。
6. 点击 **发布检查**，修正所有报错。
7. 点击 **Preview**。SteamPipe 只生成清单、不上传文件。成功后“正式上传”解锁。
8. 点击 **正式上传** 并确认。

上传完成不等于公开发布。除非填写了 SetLive，否则需要点击 **Steam 构建后台**，在 Steamworks 中自行把新构建设置到分支上。

## 排除规则

- 每行一条；空行和以 `#` 开头的行会被忽略。
- 支持 `*` 和 `?` 通配符，同时匹配相对于构建目录的路径和文件名，因此 `*.pdb` 会排除所有子目录中的 PDB 文件。
- `目录/*` 会排除该目录下的全部内容。
- 每条规则都会作为 `FileExclusion` 传给每个 Depot。建议使用相对于构建目录的路径（例如 `MyGame/Saved/*`），这样本地检查与 SteamPipe 的解释一致。

一个典型的 Unreal Engine 配置：

```text
*.pdb
Manifest_*.txt
steam_appid.txt
MyGame/Saved/*
```

## 多 Depot

**Depot ID** 支持逗号分隔，例如 `1234561, 1234562`。所有 Depot 都会映射到同一个构建目录，并使用同一组排除规则。若不同 Depot 需要不同的目录或规则，请分别建立配置。

## 数据与安全

所有数据都保存在 `%LOCALAPPDATA%\SteamUploader`（点击“打开数据目录”即可打开）：

| 路径 | 内容 |
| --- | --- |
| `profiles.ini` | 游戏配置，不含密码和验证码。 |
| `settings.ini` | 程序偏好，例如界面语言。 |
| `history\` | 每个配置的 Preview/上传结果。 |
| `build-cache\` | SteamPipe 增量构建缓存，保留它可以加快后续上传。 |
| `runs\` | 任务运行期间的临时 VDF，任务结束后自动删除。 |

内置的安全约束：

- 凭据只在 SteamCMD 自己的窗口中输入，本工具不会读取或保存。
- 包含 `steam_appid.txt` 的上传无法通过发布检查。
- 正式上传前必须先通过相同配置的 Preview，并经过二次确认。
- SetLive 默认留空，除非手动填写，否则不会推送任何分支。
- 任务运行中关闭窗口需要确认，并会安全终止 SteamCMD。

## 界面语言

界面语言默认跟随 Windows 显示语言（无匹配时使用英文）。可以随时通过右上角的语言选择框切换，选择结果保存在 `settings.ini` 中。

如需新增语言：在 [`src/I18n.h`](src/I18n.h) 的 `su::Lang` 与 [`src/I18n.cpp`](src/I18n.cpp) 的 `LanguageInfo` 中各加一项，然后为字符串表的每一行补上一列翻译。缺少任何字符串都会导致编译失败。

## 从源码构建

需要 Visual Studio 2022 或更新版本，并安装“使用 C++ 的桌面开发”工作负载和 Windows 10/11 SDK。工程会自动使用当前 Visual Studio 自带的工具集。

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-Release.ps1
```

产物为 `bin\Release\SteamUploader.exe`。程序静态链接 C 运行库（`/MT`），无需额外 DLL。也可以直接用 Visual Studio 打开 `SteamUploader.sln`。

生成发布用的压缩包和校验文件（输出到 `dist\`）：

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-Release.ps1 -Package -Version v1.0.0
```

## 发布新版本

GitHub Actions 会构建每次推送和 Pull Request。推送 `v*` 标签时，还会自动创建 GitHub Release 并上传压缩包和 `.sha256`。附注标签（annotated tag）的信息会作为 Release 说明；包含 `-` 的标签（例如 `v1.1.0-beta.1`）会标记为预发布。

```powershell
git tag -a v1.0.1 -m "在这里写更新说明"
git push origin v1.0.1
```

## 免责声明

本项目与 Valve Corporation 无关，也未获其认可。Steam 和 Steamworks 是 Valve Corporation 的商标。将构建设为上线之前，请务必在 Steamworks 中确认。

## 许可证

[MIT](LICENSE)

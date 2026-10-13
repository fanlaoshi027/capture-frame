# 录制区域框 (Capture Frame)

Windows 轻量桌面覆盖工具：只显示一个可拖动、可调整大小的细虚线矩形，作为 OBS 录制区域的视觉参考，不会修改 OBS 的任何捕获设置。

## 快捷键

- `Ctrl + Alt + F`：显示/隐藏矩形框
- `Ctrl + Alt + P`：切换鼠标穿透（穿透时不响应拖动）
- `Ctrl + Alt + Esc`：退出
- 托盘图标右键：边框颜色、线条样式和粗细
- 按住 `Shift` 拖动边框：移动；直接拖动边缘/角落：调整尺寸

位置、大小和外观保存在 `%LOCALAPPDATA%\\CaptureFrame\\settings.json`。

## 构建 EXE

在 Windows 安装 .NET 8 SDK 后运行：

```powershell
dotnet publish src/CaptureFrame/CaptureFrame.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o publish
```

生成 `publish\\CaptureFrame.exe`。GitHub Actions 会在 push 到 `capture-frame-v1` 或 `main` 时自动构建，并上传名为 `CaptureFrame-win-x64` 的 artifact。

## OBS 提示

透明覆盖窗口仍有可能被 OBS「显示器采集」捕获。录制前请测试；如果框会进入成片，使用 `Ctrl+Alt+F` 隐藏后录制。`Ctrl+Alt+P` 仅让鼠标操作穿过覆盖窗口，不代表 OBS 不会捕获它。

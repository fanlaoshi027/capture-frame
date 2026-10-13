Native Win32 轻量版：虚线矩形覆盖框，用于 OBS 录制区域参考。

快捷键：Ctrl+Alt+F 显示/隐藏；Ctrl+Alt+P 鼠标穿透；Ctrl+Alt+Esc 退出。托盘右键可改颜色、线型和粗细。拖动内部移动，拖动边缘调整大小。设置保存在 %LOCALAPPDATA%\\CaptureFrame\\CaptureFrame.ini。

本版使用原生 Win32 API，不需要 .NET 运行时。目标为几百 KB，实际以构建产物为准。注意 OBS 显示器采集可能录入参考框，录制前请测试，必要时按 Ctrl+Alt+F 隐藏。
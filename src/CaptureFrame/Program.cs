using System.Runtime.InteropServices;
using System.Text.Json;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace CaptureFrame;

internal static class Program
{
    [STAThread]
    static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new FrameForm());
    }
}

internal sealed class FrameForm : Form
{
    private const int WM_HOTKEY = 0x0312;
    private const int HOTKEY_TOGGLE = 1, HOTKEY_PASSTHROUGH = 2, HOTKEY_QUIT = 3;
    private const int GWL_EXSTYLE = -20;
    private const int WS_EX_TRANSPARENT = 0x20;
    private const int WS_EX_TOOLWINDOW = 0x80;
    private const int WS_EX_LAYERED = 0x80000;
    private const uint MOD_ALT = 0x0001, MOD_CONTROL = 0x0002, MOD_NOREPEAT = 0x4000;
    private const int BORDER = 9;
    private readonly Color keyColor = Color.Magenta;
    private readonly string settingsPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "CaptureFrame", "settings.json");
    private readonly NotifyIcon tray;
    private Rectangle savedBounds = new(180, 120, 960, 540);
    private Point dragStart, formStart;
    private ResizeMode resizeMode;
    private bool dragging, passthrough;
    private Color frameColor = Color.FromArgb(255, 65, 65);
    private float lineWidth = 1.4f;
    private DashStyle dashStyle = DashStyle.Dash;

    private enum ResizeMode { None, Move, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight }

    [DllImport("user32.dll", SetLastError = true)] private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);
    [DllImport("user32.dll", SetLastError = true)] private static extern bool UnregisterHotKey(IntPtr hWnd, int id);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongW")] private static extern int GetWindowLong(IntPtr hWnd, int nIndex);
    [DllImport("user32.dll", EntryPoint = "SetWindowLongW")] private static extern int SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

    public FrameForm()
    {
        FormBorderStyle = FormBorderStyle.None;
        ShowInTaskbar = false;
        TopMost = true;
        StartPosition = FormStartPosition.Manual;
        BackColor = keyColor;
        TransparencyKey = keyColor;
        DoubleBuffered = true;
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        LoadSettings();
        Bounds = ClampToVirtualScreen(savedBounds);
        var menu = new ContextMenuStrip();
        menu.Items.Add("显示 / 隐藏边框  (Ctrl+Alt+F)", null, (_, _) => ToggleVisible());
        menu.Items.Add("鼠标穿透开 / 关  (Ctrl+Alt+P)", null, (_, _) => TogglePassthrough());
        var colorMenu = new ToolStripMenuItem("边框颜色");
        AddColor(colorMenu, "红色", Color.FromArgb(255, 65, 65));
        AddColor(colorMenu, "蓝色", Color.FromArgb(60, 160, 255));
        AddColor(colorMenu, "绿色", Color.FromArgb(70, 230, 130));
        menu.Items.Add(colorMenu);
        var styleMenu = new ToolStripMenuItem("线条样式");
        styleMenu.DropDownItems.Add("虚线", null, (_, _) => { dashStyle = DashStyle.Dash; Invalidate(); });
        styleMenu.DropDownItems.Add("点线", null, (_, _) => { dashStyle = DashStyle.Dot; Invalidate(); });
        styleMenu.DropDownItems.Add("实线", null, (_, _) => { dashStyle = DashStyle.Solid; Invalidate(); });
        menu.Items.Add(styleMenu);
        var widthMenu = new ToolStripMenuItem("线条粗细");
        foreach (var w in new[] { 1.0f, 1.4f, 2.0f, 3.0f }) { var ww = w; widthMenu.DropDownItems.Add($"{ww:0.0} px", null, (_, _) => { lineWidth = ww; Invalidate(); }); }
        menu.Items.Add(widthMenu);
        menu.Items.Add("退出", null, (_, _) => Close());
        tray = new NotifyIcon { Icon = SystemIcons.Application, Text = "录制区域框", ContextMenuStrip = menu, Visible = true };
        tray.DoubleClick += (_, _) => ToggleVisible();
        MouseDown += OnMouseDown; MouseMove += OnMouseMove; MouseUp += OnMouseUp;
        FormClosing += (_, _) => { SaveSettings(); tray.Visible = false; tray.Dispose(); UnregisterHotKey(Handle, HOTKEY_TOGGLE); UnregisterHotKey(Handle, HOTKEY_PASSTHROUGH); UnregisterHotKey(Handle, HOTKEY_QUIT); };
        Shown += (_, _) =>
        {
            RegisterHotKey(Handle, HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, (uint)Keys.F);
            RegisterHotKey(Handle, HOTKEY_PASSTHROUGH, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, (uint)Keys.P);
            RegisterHotKey(Handle, HOTKEY_QUIT, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, (uint)Keys.Escape);
        };
    }

    protected override CreateParams CreateParams
    {
        get { var cp = base.CreateParams; cp.ExStyle |= WS_EX_TOOLWINDOW | WS_EX_LAYERED; return cp; }
    }

    protected override void WndProc(ref Message m)
    {
        if (m.Msg == WM_HOTKEY)
        {
            switch (m.WParam.ToInt32())
            {
                case HOTKEY_TOGGLE: ToggleVisible(); break;
                case HOTKEY_PASSTHROUGH: TogglePassthrough(); break;
                case HOTKEY_QUIT: Close(); break;
            }
        }
        base.WndProc(ref m);
    }

    private void AddColor(ToolStripMenuItem parent, string name, Color color) =>
        parent.DropDownItems.Add(name, null, (_, _) => { frameColor = color; Invalidate(); SaveSettings(); });

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);
        e.Graphics.SmoothingMode = SmoothingMode.None;
        using var pen = new Pen(frameColor, lineWidth) { DashStyle = dashStyle };
        var inset = Math.Max(1, (int)Math.Ceiling(lineWidth));
        e.Graphics.DrawRectangle(pen, inset, inset, Math.Max(1, Width - inset * 2 - 1), Math.Max(1, Height - inset * 2 - 1));
        using var handleBrush = new SolidBrush(frameColor);
        foreach (var p in GetHandles()) e.Graphics.FillRectangle(handleBrush, p.X - 2, p.Y - 2, 4, 4);
    }

    private Point[] GetHandles() => new[]
    {
        new Point(Width/2,0), new Point(Width/2,Height-1),
        new Point(0,Height/2), new Point(Width-1,Height/2),
        new Point(0,0), new Point(Width-1,0),
        new Point(0,Height-1), new Point(Width-1,Height-1)
    };

    private ResizeMode HitTest(Point p)
    {
        bool left = p.X <= BORDER, right = p.X >= Width - BORDER, top = p.Y <= BORDER, bottom = p.Y >= Height - BORDER;
        if (top && left) return ResizeMode.TopLeft;
        if (top && right) return ResizeMode.TopRight;
        if (bottom && left) return ResizeMode.BottomLeft;
        if (bottom && right) return ResizeMode.BottomRight;
        if (left) return ResizeMode.Left;
        if (right) return ResizeMode.Right;
        if (top) return ResizeMode.Top;
        if (bottom) return ResizeMode.Bottom;
        return ResizeMode.Move;
    }

    private void OnMouseDown(object? sender, MouseEventArgs e)
    {
        if (passthrough || e.Button != MouseButtons.Left) return;
        dragging = true; dragStart = Cursor.Position; formStart = Location; resizeMode = HitTest(e.Location);
        if (resizeMode == ResizeMode.Move && (ModifierKeys & Keys.Shift) == 0) { dragging = false; return; }
        Capture = true;
    }

    private void OnMouseMove(object? sender, MouseEventArgs e)
    {
        if (passthrough) return;
        if (!dragging)
        {
            var mode = HitTest(e.Location);
            Cursor = mode switch
            {
                ResizeMode.Left or ResizeMode.Right => Cursors.SizeWE,
                ResizeMode.Top or ResizeMode.Bottom => Cursors.SizeNS,
                ResizeMode.TopLeft or ResizeMode.BottomRight => Cursors.SizeNWSE,
                ResizeMode.TopRight or ResizeMode.BottomLeft => Cursors.SizeNESW,
                _ => Cursors.SizeAll
            };
            return;
        }
        var dx = Cursor.Position.X - dragStart.X; var dy = Cursor.Position.Y - dragStart.Y;
        var r = new Rectangle(formStart, Size);
        switch (resizeMode)
        {
            case ResizeMode.Move: r.X += dx; r.Y += dy; break;
            case ResizeMode.Left: r.X += dx; r.Width -= dx; break;
            case ResizeMode.Right: r.Width += dx; break;
            case ResizeMode.Top: r.Y += dy; r.Height -= dy; break;
            case ResizeMode.Bottom: r.Height += dy; break;
            case ResizeMode.TopLeft: r.X += dx; r.Width -= dx; r.Y += dy; r.Height -= dy; break;
            case ResizeMode.TopRight: r.Width += dx; r.Y += dy; r.Height -= dy; break;
            case ResizeMode.BottomLeft: r.X += dx; r.Width -= dx; r.Height += dy; break;
            case ResizeMode.BottomRight: r.Width += dx; r.Height += dy; break;
        }
        if (r.Width >= 160 && r.Height >= 100) Bounds = r;
    }

    private void OnMouseUp(object? sender, MouseEventArgs e)
    {
        if (!dragging) return;
        dragging = false; Capture = false; savedBounds = Bounds; SaveSettings();
    }

    private void ToggleVisible() { Visible = !Visible; if (Visible) BringToFront(); }
    private void TogglePassthrough()
    {
        passthrough = !passthrough;
        int ex = GetWindowLong(Handle, GWL_EXSTYLE);
        SetWindowLong(Handle, GWL_EXSTYLE, passthrough ? ex | WS_EX_TRANSPARENT : ex & ~WS_EX_TRANSPARENT);
        tray.Text = passthrough ? "录制区域框（鼠标穿透）" : "录制区域框";
    }

    private static Rectangle ClampToVirtualScreen(Rectangle r)
    {
        var area = SystemInformation.VirtualScreen;
        if (r.Width < 160 || r.Height < 100) return new Rectangle(area.Left + 80, area.Top + 80, Math.Min(960, area.Width - 100), Math.Min(540, area.Height - 100));
        return r;
    }

    private void LoadSettings()
    {
        try
        {
            if (!File.Exists(settingsPath)) return;
            var s = JsonSerializer.Deserialize<SavedSettings>(File.ReadAllText(settingsPath));
            if (s is null) return;
            savedBounds = new Rectangle(s.X, s.Y, s.Width, s.Height);
            if (!string.IsNullOrWhiteSpace(s.Color)) frameColor = ColorTranslator.FromHtml(s.Color);
            if (s.LineWidth > 0) lineWidth = s.LineWidth;
            dashStyle = s.DashStyle switch { "Dot" => DashStyle.Dot, "Solid" => DashStyle.Solid, _ => DashStyle.Dash };
        }
        catch { }
    }

    private void SaveSettings()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(settingsPath)!);
            var s = new SavedSettings { X = Bounds.X, Y = Bounds.Y, Width = Bounds.Width, Height = Bounds.Height, Color = ColorTranslator.ToHtml(frameColor), LineWidth = lineWidth, DashStyle = dashStyle.ToString() };
            File.WriteAllText(settingsPath, JsonSerializer.Serialize(s, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }

    private sealed class SavedSettings
    {
        public int X { get; set; } public int Y { get; set; } public int Width { get; set; } public int Height { get; set; }
        public string Color { get; set; } = "#FFFF4141";
        public float LineWidth { get; set; } = 1.4f;
        public string DashStyle { get; set; } = "Dash";
    }
}

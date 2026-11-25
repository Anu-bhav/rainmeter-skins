using System.Drawing;
using System.Drawing.Drawing2D;
using RainmeterFreeze.Enumerations;
using H.NotifyIcon.Core;
using SDIcon = System.Drawing.Icon;

namespace RainmeterFreeze;

/// <summary>
/// Handles the RainmeterFreeze tray icon.
/// </summary>
public class ControlTrayIcon
{
    readonly TrayIconWithContextMenu trayIcon;
    readonly PopupMenuItem algoNotOnDesktopToggle;
    readonly PopupMenuItem algoMaximizedToggle;
    readonly PopupMenuItem algoFullscreenToggle;

    readonly PopupMenuItem modeSuspendToggle;
    readonly PopupMenuItem modeLowPriorityToggle;

    readonly PopupMenuItem onlyRainmeterMonitorToggle;
    readonly PopupMenuItem debugLogToggle;

    // Nocturne color scheme
    private static readonly Color Cyan = Color.FromArgb(0, 202, 255);      // #00CAFF
    private static readonly Color Magenta = Color.FromArgb(255, 43, 255);  // #FF2BFF
    private static readonly Color CyanDark = Color.FromArgb(0, 149, 255);  // #0095FF
    private static readonly Color MagentaDark = Color.FromArgb(200, 30, 200);
    private static readonly Color FrozenRed = Color.FromArgb(255, 60, 60);
    private static readonly Color ActiveCyan = Color.FromArgb(0, 255, 200);

    /// <summary>
    /// The actual icon displayed in the tray.
    /// </summary>
    public TrayIconWithContextMenu Icon => trayIcon;

    /// <summary>
    /// Creates a new tray icon, belonging to the specified application context.
    /// </summary>
    /// <param name="owner">The owner of this tray icon.</param>
    public ControlTrayIcon()
    {
        trayIcon = new() {
            Icon = CreateStateIconHandle(false), // Start with "unfrozen" icon
            Visibility = IconVisibility.Visible,
            ToolTip = "RainmeterFreeze - Active",
            ContextMenu = new() {
                Items = {
                    new PopupSubMenu("Freeze when..."),
                    (
                        algoNotOnDesktopToggle = new PopupMenuItem(
                            "Not on desktop",
                            (_, _) => Program.SetFreezeAlgorithm(FreezeAlgorithm.NotOnDesktop)
                        ) { Checked = Program.Configuration.FreezeAlgorithm == FreezeAlgorithm.NotOnDesktop }
                    ),
                    (
                        algoMaximizedToggle = new PopupMenuItem(
                            "Foreground window is maximized",
                            (_, _) => Program.SetFreezeAlgorithm(FreezeAlgorithm.Maximized)
                        ) { Checked = Program.Configuration.FreezeAlgorithm == FreezeAlgorithm.Maximized }
                    ),
                    (
                        algoFullscreenToggle = new PopupMenuItem(
                            "When in full-screen mode",
                            (_, _) => Program.SetFreezeAlgorithm(FreezeAlgorithm.FullScreen)
                        ) { Checked = Program.Configuration.FreezeAlgorithm == FreezeAlgorithm.FullScreen }
                    ),

                    new PopupMenuSeparator(),

                    new PopupSubMenu("Mode"),
                    (
                        modeSuspendToggle = new PopupMenuItem(
                            "Suspend",
                            (_, _) => Program.SetFreezeMode(FreezeMode.Suspend)
                        ) { Checked = Program.Configuration.FreezeMode == FreezeMode.Suspend }
                    ),
                    (
                        modeLowPriorityToggle = new PopupMenuItem(
                            "Low Priority",
                            (_, _) => Program.SetFreezeMode(FreezeMode.LowPriority)
                        ) { Checked = Program.Configuration.FreezeMode == FreezeMode.LowPriority }
                    ),
                    
                    new PopupMenuSeparator(),

                    (
                        onlyRainmeterMonitorToggle = new PopupMenuItem(
                            "Only on Rainmeter's monitor",
                            (_, _) => Program.SetOnlyFreezeOnRainmeterMonitor(
                                !Program.Configuration.OnlyFreezeOnRainmeterMonitor)
                        ) { Checked = Program.Configuration.OnlyFreezeOnRainmeterMonitor }
                    ),

                    new PopupMenuSeparator(),

                    (
                        debugLogToggle = new PopupMenuItem(
                            "Debug Log",
                            (_, _) => ToggleDebugLog()
                        ) { Checked = Program.Configuration.EnableDebugLog }
                    ),

                    new PopupMenuSeparator(),

                    new PopupMenuItem("Exit", (_, _) => Program.Exit())
                }
            }
        };

        trayIcon.Create();

        Program.FreezeModeChanged += OnFreezeModeChanged;
        Program.FreezeAlgorithmChanged += OnFreezeAlgorithmChanged;
        Program.OnlyMonitorChanged += OnOnlyMonitorChanged;
    }

    /// <summary>
    /// Called when freeze state changes (frozen/unfrozen).
    /// Updates the tray icon to show current state.
    /// </summary>
    public void OnFreezeStateChanged(bool frozen)
    {
        try {
            // Update icon with state indicator
            trayIcon.Icon = CreateStateIconHandle(frozen);
            trayIcon.ToolTip = frozen ? "RainmeterFreeze - FROZEN" : "RainmeterFreeze - Active";
        } catch { /* ignore icon update failures */ }
    }

    private void ToggleDebugLog()
    {
        Program.Configuration.EnableDebugLog = !Program.Configuration.EnableDebugLog;
        Program.Configuration.Save();
        debugLogToggle.Checked = Program.Configuration.EnableDebugLog;
    }

    private void OnFreezeAlgorithmChanged()
    {
        var cfg = Program.Configuration;
        algoNotOnDesktopToggle.Checked = cfg.FreezeAlgorithm == FreezeAlgorithm.NotOnDesktop;
        algoMaximizedToggle.Checked = cfg.FreezeAlgorithm == FreezeAlgorithm.Maximized;
        algoFullscreenToggle.Checked = cfg.FreezeAlgorithm == FreezeAlgorithm.FullScreen;
    }

    private void OnFreezeModeChanged()
    {
        var cfg = Program.Configuration;
        modeSuspendToggle.Checked = cfg.FreezeMode == FreezeMode.Suspend;
        modeLowPriorityToggle.Checked = cfg.FreezeMode == FreezeMode.LowPriority;
    }

    private void OnOnlyMonitorChanged()
    {
        onlyRainmeterMonitorToggle.Checked = Program.Configuration.OnlyFreezeOnRainmeterMonitor;
    }

    /// <summary>
    /// Creates a tray icon handle with a Nocturne-themed visual state indicator.
    /// Active: Cyan pulse ring | Frozen: Magenta pause symbol with red accent
    /// </summary>
    private static nint CreateStateIconHandle(bool frozen)
    {
        try {
            var bmp = new Bitmap(16, 16, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp)) {
                g.Clear(Color.Transparent);
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;

                // Draw base icon: a stylized snowflake/asterisk (Nocturne theme)
                DrawBaseIcon(g, frozen);

                // Draw state indicator
                if (frozen) {
                    DrawFrozenState(g);
                } else {
                    DrawActiveState(g);
                }
            }
            return SDIcon.FromHandle(bmp.GetHicon()).Handle;
        } catch {
            return SystemIcons.Application.Handle;
        }
    }

    private static void DrawBaseIcon(Graphics g, bool frozen)
    {
        // Base shape: 4-pointed star/snowflake (matches Nocturne's geometric style)
        var center = new PointF(8, 8);
        var outerRadius = 6.5f;
        var innerRadius = 3f;

        var points = new PointF[8];
        for (int i = 0; i < 4; i++) {
            float angle = i * 90f - 45f; // 45° offset for diamond orientation
            float outerX = center.X + outerRadius * (float)Math.Cos(angle * Math.PI / 180);
            float outerY = center.Y + outerRadius * (float)Math.Sin(angle * Math.PI / 180);
            float innerX = center.X + innerRadius * (float)Math.Cos((angle + 45) * Math.PI / 180);
            float innerY = center.Y + innerRadius * (float)Math.Sin((angle + 45) * Math.PI / 180);
            
            points[i * 2] = new PointF(outerX, outerY);
            points[i * 2 + 1] = new PointF(innerX, innerY);
        }

        using (var path = new GraphicsPath()) {
            path.AddPolygon(points);

            // Gradient fill
            using (var brush = new LinearGradientBrush(
                new RectangleF(1.5f, 1.5f, 13, 13),
                frozen ? MagentaDark : CyanDark,
                frozen ? Magenta : Cyan,
                LinearGradientMode.ForwardDiagonal)) {
                g.FillPath(brush, path);
            }

            // Subtle highlight on top edge
            using (var pen = new Pen(Color.FromArgb(80, 255, 255, 255), 1)) {
                g.DrawPath(pen, path);
            }

            // Inner glow
            using (var innerPath = new GraphicsPath()) {
                var innerRect = new RectangleF(4, 4, 8, 8);
                innerPath.AddEllipse(innerRect);
                using (var brush = new SolidBrush(Color.FromArgb(40, 255, 255, 255))) {
                    g.FillPath(brush, innerPath);
                }
            }
        }
    }

    private static void DrawActiveState(Graphics g)
    {
        // Active: pulsating cyan ring (drawn as partial arc for "live" feel)
        using (var pen = new Pen(ActiveCyan, 1.5f) {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        }) {
            // Draw 3/4 circle arc at bottom-right
            var rect = new RectangleF(6, 6, 9, 9);
            g.DrawArc(pen, rect, -45, 270);
        }

        // Small cyan dot at center-bottom
        using (var brush = new SolidBrush(ActiveCyan)) {
            g.FillEllipse(brush, 10, 10, 4, 4);
        }
    }

    private static void DrawFrozenState(Graphics g)
    {
        // Frozen: double vertical bars (pause symbol) in magenta with red accent
        var barWidth = 2f;
        var barHeight = 7f;
        var gap = 2f;
        var startX = 9f;
        var startY = 4.5f;

        // Left bar
        using (var brush = new LinearGradientBrush(
            new RectangleF(startX, startY, barWidth, barHeight),
            FrozenRed, Magenta, LinearGradientMode.Vertical)) {
            g.FillRectangle(brush, startX, startY, barWidth, barHeight);
        }

        // Right bar
        using (var brush = new LinearGradientBrush(
            new RectangleF(startX + barWidth + gap, startY, barWidth, barHeight),
            FrozenRed, Magenta, LinearGradientMode.Vertical)) {
            g.FillRectangle(brush, startX + barWidth + gap, startY, barWidth, barHeight);
        }

        // Subtle glow behind bars
        using (var brush = new SolidBrush(Color.FromArgb(60, Magenta.R, Magenta.G, Magenta.B))) {
            g.FillRectangle(brush, startX - 1, startY - 1, 2 * barWidth + gap + 2, barHeight + 2);
        }

        // Red indicator dot at top-right
        using (var brush = new SolidBrush(FrozenRed)) {
            g.FillEllipse(brush, 11, 2, 4, 4);
        }
    }
}

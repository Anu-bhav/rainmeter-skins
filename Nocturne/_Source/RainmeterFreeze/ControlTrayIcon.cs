using System.Drawing;
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
    /// Creates a tray icon handle with a visual state indicator.
    /// Green dot = active, Red dot = frozen.
    /// </summary>
    private static nint CreateStateIconHandle(bool frozen)
    {
        try {
            // Get base icon
            var baseIcon = SDIcon.ExtractAssociatedIcon(Environment.ProcessPath ?? "");
            if (baseIcon == null) return SystemIcons.Application.Handle;

            // Create a composite icon with state indicator
            var bmp = new Bitmap(16, 16, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            using (var g = Graphics.FromImage(bmp)) {
                g.Clear(Color.Transparent);
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                g.DrawImage(baseIcon.ToBitmap(), 0, 0, 16, 16);

                // Draw state dot in bottom-right corner
                var dotColor = frozen ? Color.Red : Color.LimeGreen;
                using (var brush = new SolidBrush(dotColor)) {
                    g.FillEllipse(brush, 10, 10, 6, 6);
                }
                // White outline for visibility
                using (var pen = new Pen(Color.White, 1)) {
                    g.DrawEllipse(pen, 10, 10, 5, 5);
                }
            }
            return SDIcon.FromHandle(bmp.GetHicon()).Handle;
        } catch {
            return SystemIcons.Application.Handle;
        }
    }
}

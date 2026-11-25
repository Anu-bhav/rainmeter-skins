using System.Runtime.InteropServices;
using RainmeterFreeze.Native;
using RainmeterFreeze.Native.Structures;

namespace RainmeterFreeze;

/// <summary>
/// Provides information about the screen and the windows that are displayed on it.
/// </summary>
public static class ScreenInfo
{
    // Tolerance for full-screen detection (borderless windowed games may be 1-2px off)
    private const int FullScreenTolerance = 2;

    /// <summary>
    /// Checks if the foreground window is in full-screen mode.
    /// </summary>
    public static bool IsForegroundFullScreen()
    {
        var rect = new Rect();
        nint hWnd = User32.GetForegroundWindow();

        if (!User32.GetWindowRect(hWnd, ref rect))
            return false;

        nint monitor = User32.MonitorFromWindow(hWnd, MonitorFromWindowFlags.DefaultToNearest);
        if (monitor == 0)
            return false;

        var monitorInfo = new MonitorInfo();
        if (!User32.GetMonitorInfo(monitor, ref monitorInfo))
            return false;

        var monitorRect = monitorInfo.Monitor;

        return (
            Math.Abs(rect.Left - monitorRect.Left) <= FullScreenTolerance &&
            Math.Abs(rect.Top - monitorRect.Top) <= FullScreenTolerance &&
            Math.Abs(rect.Right - monitorRect.Right) <= FullScreenTolerance &&
            Math.Abs(rect.Bottom - monitorRect.Bottom) <= FullScreenTolerance
        );
    }

    /// <summary>
    /// Returns the handle of the primary monitor (the one that contains the
    /// taskbar and Start menu).
    /// </summary>
    private static nint PrimaryMonitor()
    {
        // Use the shell window to reliably get the primary monitor
        nint shellWnd = User32.GetShellWindow();
        if (shellWnd != 0)
            return User32.MonitorFromWindow(shellWnd, MonitorFromWindowFlags.DefaultToPrimary);

        // Fallback: (0,0) is typically the primary monitor origin
        var origin = new Point { X = 0, Y = 0 };
        return User32.MonitorFromPoint(origin, MonitorFromWindowFlags.DefaultToPrimary);
    }

    /// <summary>
    /// Checks whether the foreground window lives on the primary monitor.
    /// </summary>
    /// <param name="onlyPrimaryMonitor">
    /// When 'true', a window on a non-primary monitor never triggers a freeze.
    /// When 'false', the monitor is ignored entirely (original behaviour).
    /// </param>
    public static bool IsForegroundOnPrimaryMonitor(bool onlyPrimaryMonitor)
    {
        if (!onlyPrimaryMonitor)
            return true;

        nint hWnd = User32.GetForegroundWindow();
        if (hWnd == 0)
            return false;

        nint windowMonitor = User32.MonitorFromWindow(hWnd, MonitorFromWindowFlags.DefaultToNearest);
        if (windowMonitor == 0)
            return false;

        return windowMonitor == PrimaryMonitor();
    }

    /// <summary>
    /// Checks whether the foreground window is full-screen AND on the primary
    /// monitor, so a full-screen game on a secondary display does not freeze
    /// Rainmeter while its skins are still visible.
    /// </summary>
    public static bool IsForegroundFullScreenOnPrimary()
    {
        if (!IsForegroundOnPrimaryMonitor(true))
            return false;

        return IsForegroundFullScreen();
    }
}
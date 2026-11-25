using System.Diagnostics;

namespace RainmeterFreeze.Native;

/// <summary>
/// Provides additional methods in order to help with process management.
/// </summary>
static class ProcessManagement
{
    // Retry configuration for suspend/resume (handles thread creation/destruction races)
    private const int MaxRetries = 3;
    private const int RetryDelayMs = 50;
    private const uint ThreadSuspendFailed = 0xFFFFFFFF;
    private const int ThreadResumeFailed = -1;

    /// <summary>
    /// Suspends a process by its ID with retry logic.
    /// </summary>
    /// <param name="pid">The target process's ID (PID).</param>
    internal static void SuspendProcess(int pid)
    {
        var process = Process.GetProcessById(pid);

        for (int attempt = 0; attempt < MaxRetries; attempt++) {
            bool allSucceeded = true;

            foreach (ProcessThread pT in process.Threads) {
                nint pOpenThread = Kernel32.OpenThread(
                    Kernel32.ThreadAccess.SuspendResume,
                    false,
                    (uint)pT.Id
                );

                if (pOpenThread == 0) {
                    allSucceeded = false;
                    continue;
                }

                uint suspendResult = Kernel32.SuspendThread(pOpenThread);
                if (suspendResult == ThreadSuspendFailed) { // Failed
                    allSucceeded = false;
                }
                Kernel32.CloseHandle(pOpenThread);
            }

            if (allSucceeded)
                return;

            if (attempt < MaxRetries - 1)
                Thread.Sleep(RetryDelayMs);
        }
    }

    /// <summary>
    /// Resumes a previously suspended process by its ID with retry logic.
    /// </summary>
    /// <param name="pid">The target process's ID (PID).</param>
    internal static void ResumeProcess(int pid)
    {
        var process = Process.GetProcessById(pid);

        if (process.ProcessName == string.Empty)
            return;

        for (int attempt = 0; attempt < MaxRetries; attempt++) {
            bool allSucceeded = true;

            foreach (ProcessThread pT in process.Threads) {
                nint pOpenThread = Kernel32.OpenThread(
                    Kernel32.ThreadAccess.SuspendResume,
                    false,
                    (uint)pT.Id
                );

                if (pOpenThread == 0) {
                    allSucceeded = false;
                    continue;
                }

                int suspendCount;
                do {
                    suspendCount = Kernel32.ResumeThread(pOpenThread);
                } while (suspendCount > 0 && suspendCount != ThreadResumeFailed);

                if (suspendCount == ThreadResumeFailed)
                    allSucceeded = false;

                Kernel32.CloseHandle(pOpenThread);
            }

            if (allSucceeded)
                return;

            if (attempt < MaxRetries - 1)
                Thread.Sleep(RetryDelayMs);
        }
    }
}

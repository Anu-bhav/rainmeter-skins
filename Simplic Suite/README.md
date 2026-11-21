# Simplic Suite

Three Rainmeter widgets in one visual language: transparent background,
250px wide, a graph on top and a two-value readout underneath.

| Skin | File | Shows |
|---|---|---|
| CPU and RAM | `CPU and RAM/cpu_ram.ini` | CPU load + system RAM used |
| Network | `Network/Network.ini` | Download + upload throughput |
| Gpu | `Gpu/GPU.ini` | GPU load + video RAM used |

All three default to **Update=1000**. Change `Update=` in the `[Rainmeter]`
section of each `.ini` to adjust.

## Readout

```
CPU 12%          RAM 11.2/64GB
GPU 9%           VRAM 1.5/12GB
Download 1.2mbps Upload 0.4mbps
```

Each value is **centred as a pair**: the left group sits on the ¼ line and
the right group on the ¾ line. Labels are inline with their values, so the
readout stays centred as a whole regardless of digit count — no hard-coded
per-label X positions to drift out of alignment.

## Graphs: hybrid scaling

Rainmeter's `AutoScale=1` rescales each graph to its own min/max every
frame. Great for a metric that moves, misleading for one that doesn't:

| Metric | AutoScale | Fixed 0–100 |
|---|---|---|
| VRAM 1.7/12 GB (12%) | line pinned at **top** | 12% height ✓ |
| RAM 11/64 GB (17%) | line near **top** | 17% height ✓ |

So each skin uses autoscale for the dynamic metric and fixed scaling for
the constant one:

| Skin | AutoScale | Fixed 0–100 |
|---|---|---|
| CPU and RAM | CPU | RAM |
| Gpu | GPU | VRAM |
| Network | both (naturally bursty) | — |

Verified empirically: with `AutoScale=1` a constant 12 rendered at the top
whether `MinValue`/`MaxValue` were set or not — AutoScale ignores them.

## GPU: why NVML, not UsageMonitor or nvidia-smi

**Not `UsageMonitor`.** It sums per-engine/per-process utilisation and
reports 34–46% while the GPU is at 4%. Filtering to 3D engines still gives
38.8%.

**Not `nvidia-smi`.** Accurate, but ~45 ms per process launch. Polling it
once a second cost ~1.5% CPU — more than every other skin combined.

**NVML via `SimplicGpu.dll`.** Reads `nvml.dll` (ships with every NVIDIA
driver) in-process. ~0.08% CPU.

The plugin must be in `%APPDATA%\Rainmeter\Plugins\` and is 64-bit.
Source and build script: `_Source\SimplicGpu\`.

## Transparency

Rainmeter skins are transparent by default (`BackgroundMode=1` is the
default and none of these skins define a `Background`). The 2px line under
each graph is a separator, not a background.

## Colour source

All colours come from `@Resources\Variables.inc` (`FontColor1` cyan,
`FontColor2` magenta, `FontColor3` white) so every widget stays in sync.

## Source folders

- `_Source\SimplicGpu\` — the NVML plugin (C++, MinGW build).
- `_Source\RainmeterFreeze\` — RainmeterFreeze plus the multi-monitor
  patch described below.

### RainmeterFreeze multi-monitor patch

Upstream freezes Rainmeter whenever the foreground window is maximized, on
any monitor. With skins on only one screen, a maximised window elsewhere
would freeze them while still visible.

The patch adds `OnlyFreezeOnRainmeterMonitor` (default **true**) and a tray
toggle, so a window on a secondary monitor never freezes Rainmeter.

Upstream: <https://github.com/ascpixi/RainmeterFreeze> (GPL-2.0)
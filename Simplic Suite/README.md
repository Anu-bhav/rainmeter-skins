# Simplic Suite

Three Rainmeter widgets in one visual language: transparent background,
250px wide, a graph on top and a two-value readout underneath.

| Skin | File | Shows |
|---|---|---|
| CPU and RAM | `CPU and RAM/cpu_ram.ini` | CPU load + system RAM used |
| Network | `Network/Network.ini` | Download + upload throughput |
| Gpu | `Gpu/GPU.ini` | GPU load + video RAM used |
| Temp | `Temp/Temp.ini` | GPU temperature + CPU temperature |

All four default to **Update=1000**. Change `Update=` in the `[Rainmeter]`
section of each `.ini` to adjust.

## Preview

![All three widgets](Images/all_widgets.gif)

| CPU and RAM | Gpu | Network |
|:---:|:---:|:---:|
| ![](Images/cpu_ram.gif) | ![](Images/gpu.gif) | ![](Images/network.gif) |

These are real captures, recorded with the skins running: CPU load was
generated to move the graph, a CUDA-free D3D11 load generator exercised the
GPU, and a file download drove the network widget.

## Readout

```
CPU 12%    RAM 11.2/64GB
GPU 9%     VRAM 1.5/12GB
q 1.2mbps  p 0.4mbps
```

Each readout is **one String meter** centred on `X=125` — the middle of the
250px line — with the second half recoloured through an inline setting.

That is deliberate. Centring two separate meters on their own ¼ and ¾ points
does **not** centre the pair: `GPU 9%` is narrow while `VRAM 1.5/12GB` is
wide, so the visual block drifts right of centre (measured 7–11px). One
centred meter is centred by construction at any digit count.

```ini
[Readout]
Meter=String
StringAlign=Center
X=125
Text="GPU %1%    VRAM %2.1f/%3.0fGB"
InlineSetting=Color | #FontColor2#
InlinePattern=VRAM.*
```

**Inline settings are matched pairs.** `InlineSetting` goes with
`InlinePattern`, `InlineSetting2` with `InlinePattern2`. Mismatching the
numbers silently does nothing (or recolours everything), which is an easy
way to end up with a magenta label.

The Wingdings arrows on the Network skin are part of the same string via an
inline font change, so they can no longer overlap the numbers.

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
| Temp | — | both (0–100 °C) |
| Network | both (naturally bursty) | — |

Verified empirically: with `AutoScale=1` a constant 12 rendered at the top
whether `MinValue`/`MaxValue` were set or not — AutoScale ignores them.

The Temp skin fixes both graphs at 0–100 °C instead. A temperature has an
absolute meaning that autoscaling destroys: a CPU at 45 °C and one at 85 °C
would draw the same height, so the graph stops being able to show whether
the part is actually hot. Fixed scaling keeps 50 °C at half height.

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

## CPU temperature: solved with PawnIO + Intel MSRs

Windows exposes no CPU temperature through any documented API, and the ACPI
thermal zone is useless. Both were measured on this machine (MSI PRO Z790-A
WIFI, i9-13900KF):

| Source | Result |
|---|---|
| `\Thermal Zone Information(*)\Temperature` | 301 / 3010, **never changes** |
| `MSAcpi_ThermalZoneTemperature` (**elevated**) | 3010 → 27.85 °C, **never changes** |
| `EsifDeviceInformation` (Intel DTT) | class absent; `esifsvc` not installed |
| `MSFT_ThermalZoneTemperature` | invalid namespace |
| `Win32_PerfFormattedData_ThermalZoneInformation` | invalid class |
| `Win32_TemperatureProbe` | `32768`, WMI's "unknown" sentinel |

The decisive test sampled elevated under 12–28 % sustained CPU load: the raw
value stayed pinned at **3010** while `ThermalStamp` advanced normally
(8278 → 8306). Windows was polling and the ACPI call was succeeding — the
firmware returns a constant.

`0x0BC2` = 3010 is the hard-coded fallback branch motherboard vendors put at
the end of `TZ00._TMP`. Confirmed locally without a decompiler:
`GetSystemFirmwareTable` shows **`TZ00` present and `0x0BC2` present** in this
machine's DSDT. Two unrelated boards elsewhere have had their tables dumped
showing the same branch, so it is a vendor pattern.

Note the same 3010 reads as 30.1 °C when treated as tenths-of-°C instead of
tenths-of-Kelvin — one constant, two plausible-looking numbers.

### What does work: reading the MSR

Real per-core temperature lives in machine-specific registers reachable only
from ring 0. [PawnIO](https://pawnio.eu) is a signed ring-0 IO driver that
loads small signed **Pawn modules**; the official
[IntelMSR](https://github.com/namazso/PawnIO.Modules) module permits reading
an allowlist of MSRs, and `SimplicCpuTemp.dll` uses it:

| MSR | Purpose |
|---|---|
| `0x1A2` `IA32_TEMPERATURE_TARGET` | TjMax in bits 23:16 (100 °C here) |
| `0x19C` `IA32_THERM_STATUS` | current temperature in bits 22:16 |

```
TjMax = (MSR_TEMPERATURE_TARGET >> 16) & 0xFF
T     = TjMax - ((MSR_THERM_STATUS >> 16) & 0x7F)
```

Verified live: 35–47 °C idle, 35–46 °C under a 24-worker load, 45 → 37 °C
during cooldown. It moves, which is the whole point.

### Requirements

- **PawnIO installed.** It ships with some apps (here: OpenRGB). The registry
  key `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PawnIO` gives
  `InstallLocation`; the plugin also probes `%ProgramFiles%\PawnIO`.
- **Rainmeter must run elevated.** PawnIO's INF sets
  `HKR,,Security,,"D:P(A;;GA;;;SY)(A;;GA;;;BA)"` — SYSTEM and Builtin
  Administrators only. An unelevated open returns `ERROR_ACCESS_DENIED`.
- The `IntelMSR` module blob is **embedded in the DLL** (5,452 bytes), so
  there is no side file to lose. Source of the blob:
  <https://github.com/namazso/PawnIO.Modules/releases/tag/0.2.12> (LGPL-2.1).

On this machine elevation is arranged with a scheduled task
(`RainmeterElevated`, trigger *at logon*, run level *Highest*) rather than the
Startup-folder shortcut, since a Startup shortcut cannot elevate and a
"run as administrator" shortcut would prompt at every logon.

### When it cannot read

`Update()` returns **-1.0**, never a fabricated 0 °C. The skin substitutes
that with `--`, so the readout shows `CPU --°C` rather than implying a real
sub-zero reading. This happens if PawnIO is missing, the module is rejected,
or Rainmeter is not elevated.

---

## Encoding: use UTF-16LE for skins with a `°`

`°` must not be stored as UTF-8 `C2 B0`. Rainmeter decodes that as CP1252
on this setup and renders `Â°`. Every working skin on this machine
(WeatherX, Mond Weather) stores it as UTF-16LE with a `FF FE` BOM, or as a
single-byte `B0`. `Temp/Temp.ini` is saved as UTF-16LE.

## Transparency

Rainmeter skins are transparent by default (`BackgroundMode=1` is the
default and none of these skins define a `Background`). The 2px line under
each graph is a separator, not a background.

## Colour source

All colours come from `@Resources\Variables.inc` (`FontColor1` cyan,
`FontColor2` magenta, `FontColor3` white) so every widget stays in sync.

## Source folders

- `_Source\SimplicGpu\` — the NVML plugin (C++, MinGW build).
- `_Source\SimplicCpuTemp\` — the CPU temperature plugin (C++, MinGW build).
  Reads Intel MSRs through PawnIO; `IntelMSR_blob.h` is generated from the
  official `IntelMSR.bin` module, see `gen_blob.ps1`.
- `_Source\RainmeterFreeze\` — RainmeterFreeze plus the multi-monitor
  patch described below.

### RainmeterFreeze multi-monitor patch

Upstream freezes Rainmeter whenever the foreground window is maximized, on
any monitor. With skins on only one screen, a maximised window elsewhere
would freeze them while still visible.

The patch adds `OnlyFreezeOnRainmeterMonitor` (default **true**) and a tray
toggle, so a window on a secondary monitor never freezes Rainmeter.

Upstream: <https://github.com/ascpixi/RainmeterFreeze> (GPL-2.0)
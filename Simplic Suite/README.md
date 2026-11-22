# Simplic Suite

**Author:** Anu-Bhav
**Source:** <https://github.com/Anu-bhav/rainmeter-skins>
**Licence:** GPL-2.0 for the plugins and skins; the bundled `IntelMSR` Pawn
module is LGPL-2.1, as shipped upstream.

Five Rainmeter skins in one visual language: transparent background, 250 px
wide, a graph on top and a two-value readout underneath.

| Skin | File | Shows |
|---|---|---|
| CPU and RAM | `CPU and RAM/cpu_ram.ini` | CPU load + system RAM used |
| Network | `Network/Network.ini` | Download + upload throughput |
| Gpu | `Gpu/GPU.ini` | GPU load + video RAM used |
| Temp | `Temp/Temp.ini` | GPU temperature + CPU temperature |
| Clock | `Clock/Clock.ini` | Day name, letter-spaced, with a glow |

All five default to **Update=1000**. Change `Update=` in the `[Rainmeter]`
section of each `.ini` to adjust.

The Clock is the odd one out. It is a port of Connect-R's Mond clock and keeps
its own `Scale` / `BgWidth` variables, its `letters.lua` layout script, the
Anurati font and a `Language` include. Its original author is credited in the
file header. It does not use the 250 px graph-and-readout layout the other four
share.

## Repository contents

This repo holds the skins actually in use, not a reference collection:

```
Simplic Suite/     this suite
SASMusicPlayer/    S.A.S. Music Player for Web (vendor skin, 118 ini files)
BasiliskPower/     BasiliskPower
@Vault/            Rainmeter plugin vault, not a skin
```

Everything else that used to live here — Mond monitors 1-3, Simplic,
SimplyRound, Socks, illustro, Cleartext, WeatherX, UnPlayer,
WebNowPlayingRedux — was removed on 2026-11-22. It is recoverable from git
history if you want a reference back.

## Preview

| CPU and RAM | Gpu | Network |
|:---:|:---:|:---:|
| ![](Images/cpu_ram.gif) | ![](Images/gpu.gif) | ![](Images/network.gif) |

Real captures with the skins running: CPU load was generated to move the
graph, a CUDA-free D3D11 load generator exercised the GPU, and a file download
drove the network widget.

`Images/all_widgets.gif` predates the Temp and Clock skins.

## Readout

```
CPU 12%    RAM 11.2/64GB
GPU 9%     VRAM 1.5/12GB
GPU 48°C   CPU 44°C
q 1.2mbps  p 0.4mbps
```

Each readout is **one String meter** centred on `X=125`, the middle of the
250 px line, with the second half recoloured through an inline setting.

That is deliberate. Centring two separate meters on their own ¼ and ¾ points
does **not** centre the pair: `GPU 9%` is narrow while `VRAM 1.5/12GB` is
wide, so the block drifts right of centre (measured 7-11 px). One centred meter
is centred by construction at any digit count.

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
numbers silently does nothing (or recolours everything), an easy way to end up
with a magenta label. Numbering must also have no gaps: `InlineSetting2`
without an `InlineSetting` makes Rainmeter discard the whole block.

The Wingdings arrows on the Network skin are part of the same string via an
inline font change, so they can no longer overlap the numbers.

## Graphs: when to autoscale

`AutoScale=1` rescales each graph to its own min/max every frame. Great for a
metric that moves, misleading for one that doesn't:

| Metric | AutoScale | Fixed 0-100 |
|---|---|---|
| VRAM 1.7/12 GB (12%) | line pinned at **top** | 12% height correct |
| RAM 11/64 GB (17%) | line near **top** | 17% height correct |

| Skin | AutoScale | Fixed 0-100 |
|---|---|---|
| CPU and RAM | CPU | RAM |
| Gpu | GPU | VRAM |
| Temp | — | both (0-100 °C) |
| Network | both (naturally bursty) | — |

Verified empirically: with `AutoScale=1` a constant 12 rendered at the top
whether `MinValue`/`MaxValue` were set or not — AutoScale ignores them.

**Temp fixes both graphs at 0-100 °C.** A temperature has an absolute meaning
that autoscaling destroys: a CPU at 45 °C and one at 85 °C would draw the same
height, so the graph could no longer show whether the part is hot. Fixed
scaling keeps 50 °C at half height. Both measures carry `MinValue=0` /
`MaxValue=100` and the meters use `AutoScale=0`.

## GPU: why NVML, not UsageMonitor or nvidia-smi

**Not `UsageMonitor`.** Sums per-engine/per-process utilisation; reported
34-46% while the GPU was at 4%. Filtering to 3D engines still gave 38.8%.

**Not `nvidia-smi`.** Accurate, but ~45 ms per process launch. Polling once a
second cost ~1.5% CPU — more than every other skin combined.

**NVML via `SimplicGpu.dll`.** Reads `nvml.dll`, which ships with every NVIDIA
driver, in-process. ~0.08% CPU.

The plugin must be in `%APPDATA%\Rainmeter\Plugins\` and is 64-bit. Source and
build script: `_Source\SimplicGpu\`.

## CPU temperature: PawnIO + Intel MSRs

Windows exposes no CPU temperature through any documented API, and the ACPI
thermal zone is a firmware stub. Both were measured on this machine (MSI PRO
Z790-A WIFI, i9-13900KF):

| Source | Result |
|---|---|
| `\Thermal Zone Information(*)\Temperature` | 301 / 3010, **never changes** |
| `MSAcpi_ThermalZoneTemperature` (**elevated**) | 3010 -> 27.85 °C, **never changes** |
| `EsifDeviceInformation` (Intel DTT) | class absent; `esifsvc` not installed |
| `MSFT_ThermalZoneTemperature` | invalid namespace |
| `Win32_PerfFormattedData_ThermalZoneInformation` | invalid class |
| `Win32_TemperatureProbe` | `32768`, WMI's "unknown" sentinel |

The decisive test sampled elevated under 12-28% sustained CPU load: the raw
value stayed pinned at **3010** while `ThermalStamp` advanced normally
(8278 -> 8306). Windows was polling and the ACPI call was succeeding — the
firmware returns a constant.

`0x0BC2` = 3010 is the hard-coded fallback branch motherboard vendors put at
the end of `TZ00._TMP`. Confirmed locally without a decompiler:
`GetSystemFirmwareTable` shows both `TZ00` and `0x0BC2` present in this
machine's DSDT. Two unrelated boards elsewhere have had their tables dumped
showing the same branch, so it is a vendor pattern rather than a one-off.

The same 3010 reads as 30.1 °C when treated as tenths-of-°C instead of
tenths-of-Kelvin — one constant, two plausible-looking numbers. That is how it
survived several rounds of testing.

### Reading the real sensor

Real per-core temperature lives in machine-specific registers reachable only
from ring 0. [PawnIO](https://pawnio.eu) is a signed ring-0 IO driver that
loads small signed **Pawn modules**. The official
[IntelMSR](https://github.com/namazso/PawnIO.Modules) module permits reading an
allowlist of MSRs, and `SimplicCpuTemp.dll` uses it:

| MSR | Purpose |
|---|---|
| `0x1A2` `IA32_TEMPERATURE_TARGET` | TjMax in bits 23:16 (100 °C here) |
| `0x19C` `IA32_THERM_STATUS` | current temperature in bits 22:16 |

```
TjMax = (MSR_TEMPERATURE_TARGET >> 16) & 0xFF
T     = TjMax - ((MSR_THERM_STATUS >> 16) & 0x7F)
```

Verified live: 35-47 °C idle, 35-46 °C under a 24-worker load, 45 -> 37 °C
during cooldown. It moves, which is the whole point.

### Requirements

- **PawnIO installed.** It ships with some apps (here: OpenRGB, v2.2.0.0). The
  registry key
  `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PawnIO` gives
  `InstallLocation`; the plugin also probes `%ProgramFiles%\PawnIO`.
- **Rainmeter must run elevated.** PawnIO's INF sets
  `HKR,,Security,,"D:P(A;;GA;;;SY)(A;;GA;;;BA)"` — SYSTEM and Builtin
  Administrators only. An unelevated open returns `ERROR_ACCESS_DENIED`, which
  is why the skin shows `CPU --°C` instead of a number in that state.
- The `IntelMSR` blob is **embedded in the DLL** (5,452 bytes) so there is no
  side file to lose or version-skew. Source:
  <https://github.com/namazso/PawnIO.Modules/releases/tag/0.2.12> (LGPL-2.1).
  Regenerate with `_Source\SimplicCpuTemp\gen_blob.ps1` after a module release.
  Only the official signed module loads; the driver verifies the signature.

On this machine elevation is arranged with a scheduled task
(`RainmeterElevated`, trigger *at logon*, run level *Highest*) instead of the
Startup-folder shortcut, since a Startup shortcut cannot elevate and a "run as
administrator" shortcut would prompt at every logon. The old Startup
`Rainmeter.lnk` is renamed `.disabled` so it cannot launch a second,
unelevated instance.

### When it cannot read

`Update()` returns **-1.0**, never a fabricated 0 °C. The skin's Calc wrapper
maps that to `-1` and `Substitute="-1":"--"` turns it into `--`, so the readout
shows `CPU --°C` rather than implying a physically impossible sub-zero reading.
The graph treats the negative value as empty.

This happens if PawnIO is missing, the module is rejected, or Rainmeter is not
elevated.

## Encoding: use UTF-16LE for skins containing `°`

`°` must not be stored as UTF-8 `C2 B0`. Rainmeter decodes that as CP1252 on
this setup and renders `Â°`. Every working skin here stores it as UTF-16LE with
a `FF FE` BOM, or as single-byte `B0`.

`Temp/Temp.ini` is UTF-16LE with a BOM. If you edit it in an editor that
defaults to UTF-8, the degree sign will break — save it back as UTF-16LE.

## Transparency

Rainmeter skins are transparent by default (`BackgroundMode=1`, and none of
these skins define a `Background`). The 2 px line under each graph is a
separator, not a background.

## Colour source

All colours come from `@Resources\Variables.inc`:

| Variable | Value | Used for |
|---|---|---|
| `FontColor1` | `0,202,255,100` | cyan — text, lines |
| `FontColor2` | `255,43,255,100` | magenta — second readout half, lines |
| `FontColor3` | `255,255,255,100` | white — divider |
| `FontColor1_Area` | `0,202,255,51` | cyan graph fill |
| `FontColor2_Area` | `255,43,255,51` | magenta graph fill |

Text and line alpha is **100**, area fills **51**. The area fill is
deliberately fainter so overlapping graphs do not turn to mud.

## Source folders

- `_Source\SimplicGpu\` — the NVML plugin (C++, MinGW, no CRT).
  Options: `Metric=Usage` (default) | `VramUsed` | `VramTotal` | `Temp` |
  `Name`, plus `Index`.
- `_Source\SimplicCpuTemp\` — the CPU temperature plugin (C++, MinGW, no
  CRT). Reads Intel MSRs through PawnIO. `IntelMSR_blob.h` is generated from
  the official `IntelMSR.bin` by `gen_blob.ps1`.
- `_Source\RainmeterFreeze\` — RainmeterFreeze plus the multi-monitor patch.

Both plugins are built with the MinGW toolchain bundled with Strawberry Perl
(`C:\Strawberry\c\bin\g++.exe`). Each `build.bat` drops the DLL next to
itself; copy it to `%APPDATA%\Rainmeter\Plugins\`.

### RainmeterFreeze multi-monitor patch

Upstream freezes Rainmeter whenever the foreground window is maximized, on any
monitor. With skins on only one screen, a maximised window elsewhere would
freeze them while still visible.

The patch adds `OnlyFreezeOnRainmeterMonitor` (default **true**) and a tray
toggle, so a window on a secondary monitor never freezes Rainmeter.

Upstream: <https://github.com/ascpixi/RainmeterFreeze> (GPL-2.0)

## Deploying on a fresh machine

1. Install Rainmeter.
2. Clone this repo into `%USERPROFILE%\Documents\Rainmeter\Skins`.
3. Copy `_Source\SimplicGpu\SimplicGpu.dll` and
   `_Source\SimplicCpuTemp\SimplicCpuTemp.dll` to
   `%APPDATA%\Rainmeter\Plugins\`.
4. For CPU temperature only: install PawnIO, then make Rainmeter run elevated
   (scheduled task at logon with run level Highest).
5. Refresh Rainmeter. The Temp skin shows `CPU --°C` until both of the above
   are in place; everything else works immediately.

CPU temperature needs an **Intel** CPU — the `IntelMSR` module's `main()`
returns `STATUS_NOT_SUPPORTED` on AMD.
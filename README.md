<div align="center">

# ❄ Rainmeter Skins

**A personal Rainmeter setup — a Simplic suite of my own, plus the
third-party skins I run alongside it.**

![Rainmeter](https://img.shields.io/badge/Rainmeter-4.5-00CAFF?style=flat-square)
![Windows](https://img.shields.io/badge/Windows-10%2F11-0078D6?style=flat-square)
![License](https://img.shields.io/badge/skins-see%20each%20folder-lightgrey?style=flat-square)

<br>

### The whole suite, live

![CPU and RAM, GPU and Network widgets animating](<Simplic Suite/Images/all_widgets.gif>)

*CPU & RAM · GPU · Network — real captures, 1s update, transparent.*

</div>

---

## 📸 What it looks like

Three widgets in one visual language — 250px wide, transparent background,
a graph on top and a two-value readout underneath. Each animation is a real
capture of the skin running.

### CPU & RAM

![CPU and RAM widget](<Simplic Suite/Images/cpu_ram.gif>)

**CPU load + system RAM**, cyan and magenta over a shared graph.

### GPU

![GPU widget](<Simplic Suite/Images/gpu.gif>)

**Overall GPU load + video RAM**, read from NVML. The graph and numbers move
under real GPU load.

### Network

![Network widget](<Simplic Suite/Images/network.gif>)

**Download + upload throughput**, captured while a download was running.

### Anatomy

```
┌──────────────────────────┐
│      ▁▂▃▅▃▂▁▂▃  graph   │   ← blue = primary metric, magenta = secondary
│  GPU 4%    VRAM 1.5/12GB │   ← readout, centred as one line
└──────────────────────────┘
  0px                      125px                     250px
```

All three are 250px wide with a transparent background and share the same
palette (cyan `#0,202,255`, magenta `#255,43,255`, white `#255,255,255`) from
`@Resources\Variables.inc`.

| Skin | Readout | Size | Update |
|---|---|---:|:---:|
| CPU and RAM | CPU load + system RAM | 250×119 | 1000 ms |
| Gpu | GPU load + video RAM | 250×119 | 1000 ms |
| Network | Download + upload | 250×120 | 1000 ms |

---

## 🗂 Folder map

```
Skins/
├── Simplic Suite/          ← the main event (my widgets)
│   ├── CPU and RAM/
│   ├── Network/
│   ├── Gpu/
│   ├── @Resources/
│   ├── README.md
│   └── _Source/            ← C++ / C# source for the custom bits
│       ├── SimplicGpu/
│       └── RainmeterFreeze/
│
├── Simplic/                ← the original suite these were derived from
├── illustro/               ← third-party (poiru)
├── SimplyRound/            ← third-party (gronostaj)
├── Mond/                   ← third-party (Connect-R), clock patched
│   └── monitor 1/Clock/
│       ├── Clock.ini       ← modified: even letter spacing
│       └── letters.lua     ← added: per-letter slot layout
├── Socks/                  ← third-party
├── Cleartext/              ← third-party
├── WeatherX/               ← third-party
├── SASMusicPlayer/         ← third-party
├── UnPlayer/               ← third-party
└── WebNowPlayingRedux/     ← third-party
```

> Anything not in `Simplic Suite/` is someone else's skin, kept here so the
> whole setup can be restored from one clone. Each of those carries its own
> license — see the folder it came from.

### The Mond clock fix

The stock `Mond` clock used `CharacterSpacing` for the day name, but its font
(**Anurati**) has very uneven glyph widths — `I` is 32px while `W` is 324px at
the clock's size, a **10× spread**. Since `CharacterSpacing` adds the same
amount to every glyph, the unevenness survives: "SATURDAY" measured gaps of
**60px to 132px**, which is the "some letters too close, some too far" effect.

`letters.lua` replaces that with a fixed-slot layout: every letter is centred in
an identical advance slot sized to the widest glyph in the current day name.
Gaps became **50, 52, 53, 53, 55 px** — even by construction. The one wider gap
sits after `T`, which is genuinely a narrow glyph; that is correct, not a defect.

The clock keeps its original look — same font, size, shadow and colours. Only
the horizontal spacing changed.

---

## ⚡ Quick start

1. Install [Rainmeter](https://www.rainmeter.net/) (64-bit).
2. Clone this repo into your Rainmeter skins folder:
   ```powershell
   cd ~/Documents/Rainmeter
   git clone git@github.com:Anu-bhav/rainmeter-skins.git Skins
   ```
   (or clone somewhere else and point Rainmeter's `SkinPath` at it)
3. Copy the plugin:
   ```powershell
   copy "Simplic Suite\_Source\SimplicGpu\SimplicGpu.dll" "$env:APPDATA\Rainmeter\Plugins\"
   ```
   The prebuilt DLL is not committed — build it with `_Source\SimplicGpu\build.bat`
   (needs MinGW-w64).
4. Set up local secrets:
   ```powershell
   copy "Simplic\@Resources\Secrets.example.inc" "Simplic\@Resources\Secrets.inc"
   ```
   then edit `Secrets.inc` and put your weather API key in it.
5. Restart Rainmeter, then load the skins you want from **Manage**.

---

## 🎨 Simplic Suite

### CPU and RAM — `CPU and RAM/cpu_ram.ini`

| Value | Meaning |
|---|---|
| `CPU 12%` | Aggregate load across all logical cores |
| `RAM 11.2/64GB` | Used / total system RAM |

### GPU — `Gpu/GPU.ini`

| Value | Meaning |
|---|---|
| `GPU 9%` | Overall GPU utilisation, read from NVML |
| `VRAM 1.5/12GB` | Video memory used / total |

Reads `nvml.dll` directly through a small custom plugin
(`SimplicGpu.dll`) rather than spawning `nvidia-smi`, which would cost
~1.5% CPU instead of ~0.08%.

### Network — `Network/Network.ini`

| Value | Meaning |
|---|---|
| `Download 1.2mbps` | Current download throughput |
| `Upload 0.4mbps` | Current upload throughput |

---

## 🧠 Design notes

### Why the graphs don't use AutoScale

Rainmeter's `AutoScale=1` rescales each graph to its **own min/max every
frame**. That is great for a metric that genuinely moves (GPU load), but it
is actively misleading for one that sits nearly constant:

| Metric | With AutoScale | Fixed 0–100 |
|---|---|---|
| VRAM at 1.7 GB of 12 GB (12%) | line pinned at the **top** | line at 12% height ✓ |
| RAM at 11 of 64 GB (17%) | line near the **top** | line at 17% height ✓ |

So each skin uses **AutoScale for the dynamic metric and fixed scaling for
the constant one**:

| Skin | Autoscale | Fixed 0–100 |
|---|---|---|
| CPU and RAM | CPU | RAM |
| Gpu | GPU | VRAM |
| Network | both (naturally bursty) | — |

I verified this empirically: with `AutoScale=1` a constant value of 12
rendered at the top of the graph whether `MinValue`/`MaxValue` were set or
not — AutoScale ignores them entirely.

## Centred readout

Each readout is **one String meter** centred on `X=125` (the middle of the
250px line), with the second half recoloured via an inline setting:

```ini
[Readout]
Meter=String
StringAlign=Center
X=125
Text="GPU %1%    VRAM %2.1f/%3.0fGB"
InlineSetting=Color | #FontColor2#
InlinePattern=VRAM.*
```

This matters because centring two separate meters on their own quarter points
does **not** centre the pair. `GPU 9%` is narrow while `VRAM 1.5/12GB` is wide,
so the block drifts:

| Approach | Measured result |
|---|---|
| Two meters at X=62 / X=187 | shifted **right by 7–11px** |
| One centred meter at X=125 | **centred** (mid 125.5 vs skin mid 125) |

With one meter, centring is true by construction and stays correct at any
digit count.

**Inline settings are a matched pair.** `InlineSetting` goes with
`InlinePattern`, and `InlineSetting2` with `InlinePattern2`. Using
`InlineSetting` with `InlinePattern2` silently does nothing; using an
unsuffixed `InlineSetting` alongside a suffixed pattern recolours the whole
string, which is how the GPU label briefly turned magenta.

### Transparency

Rainmeter skins are transparent by default (`BackgroundMode=1`). No skin in
the suite defines a `Background`, so the desktop shows through. The thin
line under each graph is a separator, not a fill.

---

## 🔌 Custom components

### `SimplicGpu` — NVML plugin

A 12 KB C++ plugin that reports GPU metrics from `nvml.dll`. No CRT
dependency, no external processes.

| `Metric=` | Returns |
|---|---|
| `Usage` | Overall GPU utilisation, % |
| `VramUsed` | Video memory in use, bytes |
| `VramTotal` | Total video memory, bytes |
| `Temp` | GPU temperature, °C |
| `Name` | GPU name (string value) |

**Why not UsageMonitor?** Rainmeter's built-in `UsageMonitor` can't produce
a trustworthy overall GPU number. On a machine with 700+ GPU engine
instances it sums per-engine/per-process utilisation and reports 34–46%
while the GPU is actually at 4%. Filtering to 3D engines still gives 38.8%.

**Why not nvidia-smi?** It's accurate but costs ~45 ms per process launch.
Polling it once a second cost ~1.5% CPU — more than the rest of Rainmeter
combined. NVML costs ~0.08%.

### `RainmeterFreeze` — with a multi-monitor patch

Upstream [RainmeterFreeze](https://github.com/ascpixi/RainmeterFreeze)
suspends Rainmeter when the foreground window is maximized. The problem:
it does that regardless of **which** monitor the window is on. With skins
only on the primary screen, maximising something on a second monitor would
freeze them while they're still visible.

The patch adds an `OnlyFreezeOnRainmeterMonitor` option (default on) and a
matching tray toggle:

```
Freeze when...  ✓ Foreground window is maximized
Mode            ✓ Suspend
                ─────────────
                ✓ Only on Rainmeter's monitor
```

With it on, a maximised window on a secondary monitor leaves Rainmeter
running. Verified on a 3-monitor setup: Discord maximised on DISPLAY2 keeps
Rainmeter live, Mail maximised on DISPLAY1 freezes it.

Upstream: <https://github.com/ascpixi/RainmeterFreeze> (GPL-2.0)

---

## 🤝 Credits

| Skin | Author | License |
|---|---|---|
| Simplic Suite | me, built on Simplic | — |
| Simplic | HipHopium | — |
| illustro | poiru | CC BY-NC-SA 3.0 |
| SimplyRound | Krzysztof "gronostaj" Smialek | CC BY 3.0 |
| RainmeterFreeze | ascpixi | GPL-2.0 |
| Mond, Socks, Cleartext, WeatherX, SASMusicPlayer, UnPlayer, WebNowPlayingRedux | their respective authors | see each folder |

---

<div align="center">
<sub>Built with care and a lot of measuring. ❄</sub>
</div>
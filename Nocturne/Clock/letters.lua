-- Nocturne - even letter spacing for the day name
--
-- WHY THIS EXISTS
-- Anurati's glyphs vary enormously in width (I=37px, W=329px at 229px), so
-- Rainmeter's CharacterSpacing -- which adds the same amount to every glyph --
-- cannot produce even gaps. Worse, each glyph's ink is offset inside its
-- advance box, so centring by advance width leaves uneven gaps and lets A and Y
-- touch in "DAY":
--     A: ink 12..199   (ink sits 0.5px right of the advance centre)
--     Y: ink  1..186   (ink sits 10.5px LEFT of the advance centre)
--
-- THE FIX
-- Draw each glyph with StringAlign=Left, which puts the PEN at the meter's X.
-- With GDI+ the ink then begins at X + leftSideBearing. So to make a glyph's
-- visible ink start exactly at `inkStart`, set:
--     X = inkStart - leftSideBearing * scale
-- Each following glyph starts exactly `Gap` px after the previous glyph's ink
-- ends. Because placement is derived from measured ink extents, the gaps are
-- equal by construction, whatever the glyph shapes are.

-- Left ink bearing (l) and right ink edge (r) of each uppercase Anurati glyph,
-- measured with GDI+ Near alignment at the reference size (Bold). Values are in
-- PIXELS at the reference size and scale linearly with the rendered pixel size.
local REF_SIZE = 229.0

-- Rainmeter's FontSize is in POINTS, not pixels. GDI+ converts points to pixels
-- at (DPI / 72), which is 96/72 = 4/3 on a standard 96-DPI display. The glyph
-- metrics above are in pixels at 229 px, so the effective pixel scale is:
--     (fontSize_points * DPI/72) / REF_SIZE_pixels
-- Using fontSize/REF_SIZE directly (i.e. assuming 1px = 1pt) makes every glyph
-- 4/3 too wide, which closes the gaps and lets A/Y touch in "DAY".
local POINTS_TO_PIXELS = 96.0 / 72.0

local GLYPH = {
    A = { l =  12, r = 199 },
    B = { l =   0, r = 157 },
    C = { l =   0, r = 161 },
    D = { l =   1, r = 157 },
    E = { l =   0, r = 127 },
    F = { l =   0, r = 127 },
    G = { l =   0, r = 172 },
    H = { l =   0, r = 157 },
    I = { l =   0, r =  35 },
    J = { l =   0, r = 109 },
    K = { l =   1, r = 139 },
    L = { l =   0, r = 127 },
    M = { l =   0, r = 188 },
    N = { l =   0, r = 157 },
    O = { l =   0, r = 188 },
    P = { l =   0, r = 157 },
    Q = { l =   0, r = 188 },
    R = { l =   1, r = 146 },
    S = { l =   0, r = 157 },
    T = { l =   0, r =  96 },
    U = { l =   0, r = 157 },
    V = { l =   1, r = 113 },
    W = { l =   1, r = 303 },
    X = { l =   1, r = 186 },
    Y = { l =   1, r = 186 },
    Z = { l =   0, r = 188 }
}

function Initialize()
    mDay = SKIN:GetMeasure('MeasureDay')
end

-- Reads a skin variable that may contain a Rainmeter formula (e.g. "(12*4.4)")
-- and returns its numeric value. GetVariable alone does not evaluate formulas.
local function numVar(name, def)
    local raw = SKIN:GetVariable(name)
    if not raw or raw == '' then return def end
    local v = tonumber(raw)
    if v then return v end
    -- Resolve #Var# references ourselves; ParseFormula does not substitute them.
    local expr = raw:gsub('#([%w_]+)#', function(n)
        local r = SKIN:GetVariable(n)
        if r and r ~= '' then return r end
        return '0'
    end)
    local parsed = SKIN:ParseFormula('(' .. expr .. ')')
    if parsed then return parsed end
    return def
end

function Update()
    local day = mDay:GetStringValue()
    if not day then return end
    day = day:gsub('^%s+', ''):gsub('%s+$', '')
    if day == '' then return end
    day = day:upper()

    local fontSize = numVar('FontSize', 229)
    local origin   = numVar('OriginX', 150)
    local gap      = numVar('Gap', 55)
    -- FontSize is points; glyph metrics are pixels. Convert points -> pixels.
    local scale    = (fontSize * POINTS_TO_PIXELS) / REF_SIZE

    -- Ink start position of the current letter. The first glyph's ink begins
    -- exactly at OriginX; each subsequent glyph begins `gap` after the
    -- previous glyph's ink ends. At the end we re-centre the whole word inside
    -- the background meter so the day name sits dead-centre on screen.
    local inkStart = origin

    local positions = {}

    for i = 1, #day do
        local ch = day:sub(i, i)
        local g  = GLYPH[ch] or { l = 0, r = 180 }

        local inkW = (g.r - g.l + 1) * scale

        -- StringAlign=Left puts the pen at X, and the ink starts leftSideBearing
        -- to the right of the pen. Solve for the X that makes ink start at inkStart.
        local x = inkStart - (g.l * scale)

        positions[i] = { ch = ch, x = x, inkL = g.l * scale, inkW = inkW }

        -- Gap before the NEXT letter. A tiny per-pair tweak lets specific
        -- letters sit closer together where the shapes look better tight.
        -- "DA" is pulled in by 1.5px: in DAY, A tucks a little nearer to D.
        local step = gap
        local next = day:sub(i + 1, i + 1)
        if ch == 'D' and next == 'A' then
            step = gap - 1.5
        end

        inkStart = inkStart + inkW + step
    end

    -- Centre the day name inside the background meter. The ink-based layout
    -- above lays letters out left-to-right from OriginX; this re-centres the
    -- whole word so it is symmetric within the background. The background
    -- itself is sized to the word (DayWordW = word ink width + 2*pad) and pushed
    -- into the skin variable that [MeterBackground] reads via W=#DayWordW#.
    local n = #day
    local last = positions[n]
    local totalW = last.x + last.inkL + last.inkW - origin
    local pad = 30 * numVar('Scale', 4.4)
    local bgW = totalW + 2 * pad
    SKIN:Bang('!SetVariable', 'DayWordW', bgW)
    -- bgW = word ink width + 2*pad, so the ink must sit at pad..bgW-pad:
    -- shift = 0 puts it there exactly (the old formula added an ink-bearing
    -- offset that made the before/after gaps differ by 2*inkL_1).
    local shift = 0
    for i = 1, n do
        SKIN:Bang('!SetVariable', 'Day' .. i, day:sub(i, i))
        SKIN:Bang('!SetVariable', 'DayX' .. i, positions[i].x + shift)
    end

    -- Vertical: height = cap ink height + 2*padV -> equal gaps top/bottom.
    local capH = 251.0
    local padV = 43.0
    local bgH = capH + 2 * padV
    SKIN:Bang('!SetVariable', 'DayH', bgH)

    -- Blank unused slots so a short name cannot retain letters from a longer
    -- previous day (e.g. SUNDAY after WEDNESDAY).
    for i = #day + 1, 9 do
        SKIN:Bang('!SetVariable', 'Day' .. i, '')
        SKIN:Bang('!SetVariable', 'DayX' .. i, 0)
    end

    return day
end
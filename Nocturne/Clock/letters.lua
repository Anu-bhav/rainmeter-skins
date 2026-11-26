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

-- Last window position applied by RepositionWindow, so we only move the window
-- when the target actually changes (avoids a bang and an ini write every tick).
local lastWinX = nil
local lastWinY = nil

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
    -- Reset the last-applied window position so the first Update after a load
    -- or refresh always repositions the window.
    lastWinX = nil
    lastWinY = nil
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
    -- Move the laid-out word so its first ink starts exactly `pad` from the
    -- panel's left edge. bgW then leaves the same `pad` on the right. With the
    -- default OriginX == pad this is 0; the explicit form keeps the margins
    -- equal even if OriginX is changed independently.
    local shift = pad - origin
    -- Vertical: place equal gaps above and below the glyph ink. GDI+ path
    -- metrics at REF_SIZE give ink top = 41.22 and ink height = 187.78; both
    -- scale with the rendered em. Rainmeter puts the pen at TextY and the ink
    -- starts (TextY + topBearing) below the panel top, so the panel height that
    -- centres the ink is:
    --     bgH = 2 * (TextY + topBearing * scale) + inkHeight * scale
    local textY = numVar('TextY', 0)
    local topBearing = 41.22 * scale
    local inkH = 187.78 * scale
    local bgH = 2 * (textY + topBearing) + inkH
    SKIN:Bang('!SetVariable', 'DayWordW', bgW)
    SKIN:Bang('!SetVariable', 'DayH', bgH)
    for i = 1, n do
        SKIN:Bang('!SetVariable', 'Day' .. i, day:sub(i, i))
        SKIN:Bang('!SetVariable', 'DayX' .. i, positions[i].x + shift)
    end

    -- Blank unused slots so a short name cannot retain letters from a longer
    -- previous day (e.g. SUNDAY after WEDNESDAY).
    for i = #day + 1, 9 do
        SKIN:Bang('!SetVariable', 'Day' .. i, '')
        SKIN:Bang('!SetVariable', 'DayX' .. i, 0)
    end

    -- Keep the window centred horizontally with its top edge a fixed fraction
    -- down the work area. bgW is the exact window width for this day, so the
    -- centring stays correct as the word changes width.
    RepositionWindow(bgW)

    return day
end

-- Moves the skin window so it is horizontally centred on the work area and its
-- top edge sits at TopFraction of the work-area height. Only issues a bang when
-- the target pixel position changes, so a static day costs nothing.
function RepositionWindow(winW)
    if numVar('CenterWindow', 1) ~= 1 then return end

    -- Built-in variables give the primary screen / work area. The Nocturne
    -- skins live on the primary monitor, so centring there is correct.
    local workW = numVar('WORKAREAWIDTH', numVar('SCREENWIDTH', 2560))
    local workH = numVar('WORKAREAHEIGHT', numVar('SCREENHEIGHT', 1440))
    local frac  = numVar('TopFraction', 0.2)

    local targetX = math.floor((workW - winW) / 2 + 0.5)
    local targetY = math.floor(workH * frac + 0.5)

    if targetX ~= lastWinX or targetY ~= lastWinY then
        SKIN:Bang('!SetWindowPosition', targetX, targetY)
        lastWinX = targetX
        lastWinY = targetY
    end
end
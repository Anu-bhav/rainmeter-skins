-- Simplic - even letter spacing for the day name
--
-- WHY THIS EXISTS
-- Anurati's glyph advance widths vary enormously. Measured at 229 px:
--     I=32   T=117  V=136  ...  W=324
-- That is a 10x spread. Rainmeter's CharacterSpacing adds the SAME amount to
-- every glyph, so uneven glyphs stay uneven: "SATURDAY" measured gaps of
-- 60px to 131px.
--
-- THE FIX
-- Give every letter an identical advance slot and centre each glyph inside
-- its slot. Because every glyph gets the same slot, the visual gap between
-- consecutive letters is constant by construction.
--
-- The slot is sized from the WIDEST glyph in the current word so that wide
-- letters (W, M, O, Q) fit without crowding their neighbours. That is what
-- makes the gaps even rather than merely equal-advance.

-- Natural advance width of each uppercase Anurati glyph, measured at 229px
-- with GDI+ (GenericTypographic). Scales linearly with font size.
local BASE_ADVANCE = {
    A = 205.9, B = 185.3, C = 164.9, D = 190.1, E = 145.6,
    F = 145.9, G = 200.4, H = 187.6, I =  32.1, J = 140.8,
    K = 161.4, L = 141.1, M = 218.5, N = 187.3, O = 221.0,
    P = 178.6, Q = 220.3, R = 174.3, S = 183.9, T = 116.8,
    U = 179.1, V = 135.6, W = 324.5, X = 210.5, Y = 203.4,
    Z = 193.7
}
local REF_SIZE = 229.0

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
    local parsed = SKIN:ParseFormula('(' .. raw .. ')')
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
    local scale    = fontSize / REF_SIZE

    -- Find the widest glyph in this word; every slot is sized to fit it.
    local widest = 0
    for i = 1, #day do
        local ch = day:sub(i, i)
        local w = BASE_ADVANCE[ch]
        if w and w > widest then widest = w end
    end
    if widest <= 0 then widest = 200 end

    -- Slot = widest glyph + a fixed gap. Sizing from the widest glyph means
    -- no letter can crowd its neighbour, so every visible gap is the same.
    local slot = (widest * scale) + gap

    for i = 1, #day do
        SKIN:Bang('!SetVariable', 'Day' .. i, day:sub(i, i))
        -- Centre of slot i; meters use StringAlign=Center
        SKIN:Bang('!SetVariable', 'DayX' .. i, origin + (i - 1) * slot + (slot / 2))
    end

    -- Blank unused slots so a short name cannot retain letters from a
    -- longer previous day (e.g. SUNDAY after WEDNESDAY).
    for i = #day + 1, 9 do
        SKIN:Bang('!SetVariable', 'Day' .. i, '')
        SKIN:Bang('!SetVariable', 'DayX' .. i, 0)
    end

    return day
end
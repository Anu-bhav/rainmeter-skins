function FadeIn()
    local idle = tonumber(SKIN:GetVariable('TransparencyIdle')) or 100
    local hover = tonumber(SKIN:GetVariable('TransparencyHover')) or 255

    SKIN:FadeWindow(idle, hover)
end

function FadeOut()
    local idle = tonumber(SKIN:GetVariable('TransparencyIdle')) or 100
    local hover = tonumber(SKIN:GetVariable('TransparencyHover')) or 255

    SKIN:FadeWindow(hover, idle)
end
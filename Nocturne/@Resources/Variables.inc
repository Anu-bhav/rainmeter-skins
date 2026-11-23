[Variables]
Language=English
Location=Poste de Flacq
Unit=m
Hidden=0

; Clean, modern font selections that render '0' beautifully:
FontFace="MesloLGS NF Regular"
; FontFace="Segoe UI"  ; (Windows native fallback)
; FontFace="Ubuntu"    ; (Excellent rounded alternative)

; --- Text and graph lines -----------------------------------------------
; Alpha 100 (~39%) is deliberate: it lifts the palette off the wallpaper
; without shouting. Used for readout text AND the crisp Line overlay.
FontColor1=0,202,255,100
FontColor2=255,43,255,100
FontColor3=255,255,255,100

; --- Neon variants ------------------------------------------------------
; The same hues at alpha 51 (~20%). A solid fill needs to stay translucent
; or the graph becomes a flat block of colour, so the "neon" here comes
; from saturation, not opacity - 0,202,255 and 255,43,255 are already at
; maximum chroma for their hue.
FontColor1_Neon=0,202,255,51
FontColor2_Neon=255,43,255,51

; Hue of the Nocturne CPU/RAM line graph (205 deg) at full saturation and
; value - the vivid azure that graph draws.
NeonBlue=0,149,255,51

; --- Area fills ---------------------------------------------------------
FontColor1_Area=0,202,255,51
FontColor2_Area=255,43,255,51


; --- ISP Speed Limits (Bits per second) ---
; 100 Mbps = 100,000,000 bits
MaxDownload=100000000
; 20 Mbps = 20,000,000 bits
MaxUpload=20000000
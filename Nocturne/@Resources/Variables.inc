[Variables]
Language=English
Location=Poste de Flacq
Unit=m
Hidden=0

; Clean, modern font selections that render '0' beautifully:
FontFace="MesloLGS NF Regular"
; FontFace="Segoe UI"  ; (Windows native fallback)
; FontFace="Ubuntu"    ; (Excellent rounded alternative)

; --- Text ---------------------------------------------------------------
; Alpha 100 (~39%) is deliberate for text: it lifts the palette off the
; wallpaper without shouting. Used for the readout.
FontColor1=0,202,255,100
FontColor2=255,43,255,100
FontColor3=255,255,255,100

; --- Neon variants ------------------------------------------------------
; Same hues as BasiliskPower's neon set, at alpha 70 (~27%) instead of
; BasiliskPower's 255. These are the vivid colours the graph lines draw:
; bright enough to read as neon against the wallpaper, translucent enough
; that the fill underneath still shows through.
;
;   FontColor1_Neon hue 192 -> #00CAFF  (theme cyan)
;   FontColor2_Neon hue 300 -> #FF2BFF  (theme magenta)
;   NeonBlue                -> #0095FF  (the azure the CPU/RAM line uses)
FontColor1_Neon=0,202,255,70
FontColor2_Neon=255,43,255,70
NeonBlue=0,149,255,70

; --- Area fills ---------------------------------------------------------
; Deliberately much fainter than the neon lines: two overlapping fills at
; higher alpha turn to mud, and the area is only there to give the line
; something to sit on.
FontColor1_Area=0,202,255,51
FontColor2_Area=255,43,255,51


; --- ISP Speed Limits (Bits per second) ---
; 100 Mbps = 100,000,000 bits
MaxDownload=100000000
; 20 Mbps = 20,000,000 bits
MaxUpload=20000000
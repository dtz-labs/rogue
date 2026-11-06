; The port paints every character itself through the ZX renderer and the ROM
; font, so nothing ever reaches the C library console.  z88dk still wires
; fputc_cons to its generic driver unless somebody else claims the symbol
; first, and that driver drags in a 768-byte 4x8 font, the VT52/ZX control
; parser and the generic console back end -- around 1.6 KiB of resident RAM
; that is never executed.
;
; The link redirects fputc_cons here (see LDFLAGS) rather than the object
; simply defining the name: crt0 resolves its own fallback while assembling,
; long before this object is seen, so claiming the symbol directly collides
; with it.  The routine must remain real code -- the CRT publishes
; _fputc_cons as an alias of whatever fputc_cons points at, so redirecting to
; address zero would aim a call at the bottom of the ROM.

	SECTION code_compiler

	PUBLIC	zx_fputc_cons_null

zx_fputc_cons_null:
	ret

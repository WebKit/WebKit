struc base, -7
.a: resb 1
.b: resb 1
endstruc

; Expect base and base.a to appear at -7, base.b at -6
db base
db base.a
db base.b
; The size should be '2' here
db base_size

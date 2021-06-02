.text
.file 1 "inserted_by_delocate.c"
.loc 1 1 0
BORINGSSL_bcm_text_start:
	.type foo, %function
	.globl foo
.Lfoo_local_target:
foo:
	// GOT load
// WAS adrp x1, :got:stderr
	sub sp, sp, 128
	stp x0, lr, [sp, #-16]!
	bl .Lboringssl_loadgot_stderr
	mov x1, x0
	ldp x0, lr, [sp], #16
	add sp, sp, 128
// WAS ldr x0, [x1, :got_lo12:stderr]
	mov x0, x1

	// GOT load to x0
// WAS adrp x0, :got:stderr
	sub sp, sp, 128
	stp x0, lr, [sp, #-16]!
	bl .Lboringssl_loadgot_stderr
	ldp xzr, lr, [sp], #16
	add sp, sp, 128
// WAS ldr x1, [x0, :got_lo12:stderr]
	mov x1, x0

	// GOT load with no register move
// WAS adrp x0, :got:stderr
	sub sp, sp, 128
	stp x0, lr, [sp, #-16]!
	bl .Lboringssl_loadgot_stderr
	ldp xzr, lr, [sp], #16
	add sp, sp, 128
// WAS ldr x0, [x0, :got_lo12:stderr]

	// Address load
// WAS adrp x0, .Llocal_data
	adr x0, .Llocal_data
// WAS add x1, x0, :lo12:.Llocal_data
	add	x1, x0, #0

	// Address of local symbol with offset
// WAS adrp x10, .Llocal_data2+16
	adr x10, .Llocal_data2+16
// WAS add x11, x10, :lo12:.Llocal_data2+16
	add	x11, x10, #0

	// Address load with no-op add instruction
// WAS adrp x0, .Llocal_data
	adr x0, .Llocal_data
// WAS add x0, x0, :lo12:.Llocal_data

	// armcap
// WAS adrp x1, OPENSSL_armcap_P
	sub sp, sp, 128
	stp x0, lr, [sp, #-16]!
	bl .LOPENSSL_armcap_P_addr
	mov x1, x0
	ldp x0, lr, [sp], #16
	add sp, sp, 128
// WAS ldr w2, [x1, :lo12:OPENSSL_armcap_P]
	ldr	w2, [x1]

	// armcap to w0
// WAS adrp x0, OPENSSL_armcap_P
	sub sp, sp, 128
	stp x0, lr, [sp, #-16]!
	bl .LOPENSSL_armcap_P_addr
	ldp xzr, lr, [sp], #16
	add sp, sp, 128
// WAS ldr w1, [x1, :lo12:OPENSSL_armcap_P]
	ldr	w1, [x1]

	// Load from local symbol
// WAS adrp x10, .Llocal_data2
	adr x10, .Llocal_data2
// WAS ldr q0, [x10, :lo12:.Llocal_data2]
	ldr	q0, [x10]

// WAS bl local_function
	bl	.Llocal_function_local_target

// WAS bl remote_function
	bl	bcm_redirector_remote_function

	bl bss_symbol_bss_get

	# Regression test for a two-digit index.
	ld1 { v1.b }[10], [x9]


.Llocal_function_local_target:
local_function:

// BSS data
.type bss_symbol,@object
.section .bss.bss_symbol,"aw",@nobits
bss_symbol:
.Lbss_symbol_local_target:

.word 0
.size bss_symbol, 4
.text
.loc 1 2 0
BORINGSSL_bcm_text_end:
.p2align 2
.hidden bcm_redirector_remote_function
.type bcm_redirector_remote_function, @function
bcm_redirector_remote_function:
.cfi_startproc
	b remote_function
.cfi_endproc
.size bcm_redirector_remote_function, .-bcm_redirector_remote_function
.p2align 2
.hidden bss_symbol_bss_get
.type bss_symbol_bss_get, @function
bss_symbol_bss_get:
.cfi_startproc
	adrp x0, .Lbss_symbol_local_target
	add x0, x0, :lo12:.Lbss_symbol_local_target
	ret
.cfi_endproc
.size bss_symbol_bss_get, .-bss_symbol_bss_get
.p2align 2
.hidden .Lboringssl_loadgot_stderr
.type .Lboringssl_loadgot_stderr, @function
.Lboringssl_loadgot_stderr:
.cfi_startproc
	adrp x0, :got:stderr
	ldr x0, [x0, :got_lo12:stderr]
	ret
.cfi_endproc
.size .Lboringssl_loadgot_stderr, .-.Lboringssl_loadgot_stderr
.p2align 2
.hidden .LOPENSSL_armcap_P_addr
.type .LOPENSSL_armcap_P_addr, @function
.LOPENSSL_armcap_P_addr:
.cfi_startproc
	adrp x0, OPENSSL_armcap_P
	add x0, x0, :lo12:OPENSSL_armcap_P
	ret
.cfi_endproc
.size .LOPENSSL_armcap_P_addr, .-.LOPENSSL_armcap_P_addr
.type BORINGSSL_bcm_text_hash, @object
.size BORINGSSL_bcm_text_hash, 64
BORINGSSL_bcm_text_hash:
.byte 0xae
.byte 0x2c
.byte 0xea
.byte 0x2a
.byte 0xbd
.byte 0xa6
.byte 0xf3
.byte 0xec
.byte 0x97
.byte 0x7f
.byte 0x9b
.byte 0xf6
.byte 0x94
.byte 0x9a
.byte 0xfc
.byte 0x83
.byte 0x68
.byte 0x27
.byte 0xcb
.byte 0xa0
.byte 0xa0
.byte 0x9f
.byte 0x6b
.byte 0x6f
.byte 0xde
.byte 0x52
.byte 0xcd
.byte 0xe2
.byte 0xcd
.byte 0xff
.byte 0x31
.byte 0x80
.byte 0xa2
.byte 0xd4
.byte 0xc3
.byte 0x66
.byte 0xf
.byte 0xc2
.byte 0x6a
.byte 0x7b
.byte 0xf4
.byte 0xbe
.byte 0x39
.byte 0xa2
.byte 0xd7
.byte 0x25
.byte 0xdb
.byte 0x21
.byte 0x98
.byte 0xe9
.byte 0xd5
.byte 0x53
.byte 0xbf
.byte 0x5c
.byte 0x32
.byte 0x6
.byte 0x83
.byte 0x34
.byte 0xc
.byte 0x65
.byte 0x89
.byte 0x52
.byte 0xbd
.byte 0x1f

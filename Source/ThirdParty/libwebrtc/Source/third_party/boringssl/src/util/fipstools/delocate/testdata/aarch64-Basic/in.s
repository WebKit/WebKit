	.type foo, %function
	.globl foo
foo:
	// GOT load
	adrp x1, :got:stderr
	ldr x0, [x1, :got_lo12:stderr]

	// GOT load to x0
	adrp x0, :got:stderr
	ldr x1, [x0, :got_lo12:stderr]

	// GOT load with no register move
	adrp x0, :got:stderr
	ldr x0, [x0, :got_lo12:stderr]

	// Address load
	adrp x0, .Llocal_data
	add x1, x0, :lo12:.Llocal_data

	// Address of local symbol with offset
	adrp x10, .Llocal_data2+16
	add x11, x10, :lo12:.Llocal_data2+16

	// Address load with no-op add instruction
	adrp x0, .Llocal_data
	add x0, x0, :lo12:.Llocal_data

	// armcap
	adrp x1, OPENSSL_armcap_P
	ldr w2, [x1, :lo12:OPENSSL_armcap_P]

	// armcap to w0
	adrp x0, OPENSSL_armcap_P
	ldr w1, [x1, :lo12:OPENSSL_armcap_P]

	// Load from local symbol
	adrp x10, .Llocal_data2
	ldr q0, [x10, :lo12:.Llocal_data2]

	bl local_function

	bl remote_function

	bl bss_symbol_bss_get

	# Regression test for a two-digit index.
	ld1 { v1.b }[10], [x9]


local_function:

// BSS data
.type bss_symbol,@object
.section .bss.bss_symbol,"aw",@nobits
bss_symbol:
.word 0
.size bss_symbol, 4

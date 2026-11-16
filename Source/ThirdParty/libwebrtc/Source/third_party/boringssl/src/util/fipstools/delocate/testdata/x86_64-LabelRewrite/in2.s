	# References to local labels are rewritten in subsequent files.
.Llocal_label:
	jbe .Llocal_label
	leaq .Llocal_label+2048(%rip), %r14
	leaq .Llocal_label+2048+1024(%rip), %r14

	.section .rodata
.L1:
	.quad 42
.L2:
	.quad .L2-.L1
	.uleb128 .L2-.L1
	.sleb128 .L2-.L1

# .byte was not parsed as a symbol-containing directive on the
# assumption that it's too small to hold a pointer. But Clang
# will store offsets in it.
.byte   (.LBB231_40-.LBB231_19)>>2, 4, .Lfoo, (.Lfoo), .Lfoo<<400, (   .Lfoo ) <<  66
.byte   421

# .set directives defining local symbols should be rewritten.
.set .Llocally_set_symbol1, 1
.equ .Llocally_set_symbol2, 2
.equiv .Llocally_set_symbol3, 3
.Llocally_set_symbol4 = 4

# References to local symbols in .set directives should be rewritten.
.set alias_to_local_label, .Llocal_label
.equ alias_to_local_label, .Llocal_label
.equiv alias_to_local_label, .Llocal_label
alias_to_local_label = .LLocal_label
.set .Llocal_alias_to_local_label, .Llocal_label
.equ .Llocal_alias_to_local_label, .Llocal_label
.equiv .Llocal_alias_to_local_label, .Llocal_label
.Llocal_alias_to_local_label = .Llocal_label

	# When rewritten, AVX-512 tokens are preserved.
	vpcmpneqq .Llabel(%rip){1to8}, %zmm1, %k0

.Ltmp0:
# The first operand (the "place") of a .reloc should be rewritten.
	.reloc .Ltmp0, R_AARCH64_PATCHINST, ds

	.prefalign 2
	.prefalign 4, .Lfunc_end, nop
	.type prefalign_func2, @function
prefalign_func2:
	ret
.Lfunc_end:
	.size prefalign_func2, .Lfunc_end-prefalign_func2

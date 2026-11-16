.intel_syntax noprefix
.section	.rodata
.LC0:
.string	"Hello"
.text
	lea	ecx, 4 [esp]
	and	esp, -16
	push	DWORD PTR [ecx-4]
	push	ebp
	fstp	st(0)
	ffree	st(1)
	mov	ebp, esp
	push	ecx
	sub	esp, 4
	mov	DWORD PTR [esp], OFFSET FLAT:.LC0
	add	esp, 4
	pop	ebp
	lea	esp, [ecx-4]
	ret

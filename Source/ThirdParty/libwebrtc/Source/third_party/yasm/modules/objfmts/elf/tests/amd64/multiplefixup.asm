[section .data]
foobar:
	dq	42
[section .text]
foo:
	times 4 mov rax, [rel foobar]
	times 4 mov rax, [foobar]
	times 4 jmp foo

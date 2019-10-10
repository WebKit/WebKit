[section .text]
[bits 64]
;[global foo]
;[global bar]
times (foo-bar+$$-$)&7 nop
foo: inc eax
bar: inc ecx


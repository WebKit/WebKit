; At present, nasm doesn't seem to support PIC generation for Mach-O.
; The PIC support code below is a little tricky.
[extern func]
        SECTION .rodata
const_base:

%define GOTOFF(got,sym) (got) + (sym) - const_base

%imacro get_GOT 1
        ; NOTE: this macro destroys ecx resister.
        call    %%geteip
        add     ecx, byte (%%ref - $)
        jmp     short %%adjust
%%geteip:
        mov     ecx, [esp]
        ret
%%adjust:
        push    ebp
        xor     ebp,ebp         ; ebp = 0
%ifidni %1,ebx  ; (%1 == ebx)
        ; db 0x8D,0x9C + jmp near const_base =
        ;   lea ebx, [ecx+ebp*8+(const_base-%%ref)] ; 8D,9C,E9,(offset32)
        db      0x8D,0x9C               ; 8D,9C
        jmp     near const_base         ; E9,(const_base-%%ref)
%%ref:
%else  ; (%1 != ebx)
        ; db 0x8D,0x8C + jmp near const_base =
        ;   lea ecx, [ecx+ebp*8+(const_base-%%ref)] ; 8D,8C,E9,(offset32)
        db      0x8D,0x8C               ; 8D,8C
        jmp     strict near const_base          ; E9,(const_base-%%ref)
%%ref:  mov     %1, ecx
%endif ; (%1 == ebx)
        pop     ebp
%endmacro

SECTION .text

jmp const_base

get_GOT ebx

jmp const_base

call func

ret

.section .llvm_addrsig, "", @llvm_addrsig
.addrsig
.addrsig_sym foo

.text

.global foo
foo:
        movq %rax, %rbx

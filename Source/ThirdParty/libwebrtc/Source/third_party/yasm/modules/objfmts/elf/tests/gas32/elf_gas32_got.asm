.text

.extern a

.globl tst
tst:
   call L1
L1:
   pop %eax
   add $_GLOBAL_OFFSET_TABLE_+(.-L1), %eax
   mov (a@GOT)(%eax), %eax
   movl $5, (%eax)
   ret

.macro foo arg1, arg2
.byte \arg1
.byte \arg2
.endm

.macro bar x y
.byte \x-\y
.endm

.macro def a=5 b
.byte \a + \b
.endm

.macro nest x=9
.macro zap y
.byte \y
.endm
zap \x
.endm

foo 5 6
bar 3, 2
def ,3
def 3 2
nest

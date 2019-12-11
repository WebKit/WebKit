dw foo
dw (foo+((1<<9)-1))
dw (foo+((1<<9)-1)) >> 9
foo:
dw (bar+foo)>>(0+1)
bar:

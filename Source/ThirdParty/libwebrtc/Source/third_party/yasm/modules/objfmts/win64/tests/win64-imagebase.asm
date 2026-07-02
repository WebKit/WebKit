[section .text]
handler: ret
func:	ret
func_end:

[section .pdata]
	dd func
	dd func_end
	dd myunwnd

[section .xdata]
myunwnd:
	db 9,0,0,0
	dd handler

[section .foo]
	dd handler wrt ..imagebase

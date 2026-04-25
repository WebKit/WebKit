# [oformat win32]
.text

	.ascii ">>>>"
pre04:	.ascii "<<<<"
	.ascii ">>>>>"
pre0d:	.ascii "<<<"
	.ascii ">>>>>>"
pre16:	.ascii "<<"
	.ascii ">>>>>>>"
pre1f:	.ascii "<"

.data

	.ascii ">>>>"
sam04:	.ascii "<<<<"
	.ascii ">>>>>"
sam0d:	.ascii "<<<"
	.ascii ">>>>>>"
sam16:	.ascii "<<"
	.ascii ">>>>>>>"
sam1f:	.ascii "<"

	.ascii ">>>>"
	.secrel32 pre04
	.byte 0x11
	.secrel32 pre0d
	.byte 0x11
	.secrel32 pre16
	.byte 0x11
	.secrel32 pre1f
	.byte 0x11
	.ascii "<<<<<<<<"

	.ascii ">>>>"
	.secrel32 sam04
	.byte 0x11
	.secrel32 sam0d
	.byte 0x11
	.secrel32 sam16
	.byte 0x11
	.secrel32 sam1f
	.byte 0x11
	.ascii "<<<<<<<<"

	.ascii ">>>>"
	.secrel32 nex04
	.byte 0x11
	.secrel32 nex0d
	.byte 0x11
	.secrel32 nex16
	.byte 0x11
	.secrel32 nex1f
	.byte 0x11
	.ascii "<<<<<<<<"

	.ascii ">>>>"
	.secrel32 ext24
	.byte 0x11
	.secrel32 ext2d
	.byte 0x11
	.secrel32 ext36
	.byte 0x11
	.secrel32 ext3f
	.byte 0x11
	.ascii "<<<<<<<<"

.section .rdata

	.ascii ">>>>"
nex04:	.ascii "<<<<"
	.ascii ">>>>>"
nex0d:	.ascii "<<<"
	.ascii ">>>>>>"
nex16:	.ascii "<<"
	.ascii ">>>>>>>"
nex1f:	.ascii "<"
	.ascii ">>>>"

	.p2align 4,0

#!/usr/bin/env ruby

out = Array.new(256, "NULL");
code_list = <<EOF
MOV_i	#imm,Rn	1110nnnniiiiiiii
MOV.W	@(disp*,PC),Rn	1001nnnndddddddd
MOV.L	@(disp*,PC),Rn	1101nnnndddddddd
MOV	Rm,Rn	0110nnnnmmmm0011
MOV.B	Rm,@Rn	0010nnnnmmmm0000
MOV.W	Rm,@Rn	0010nnnnmmmm0001
MOV.L	Rm,@Rn	0010nnnnmmmm0010
MOV.B	@Rm,Rn	0110nnnnmmmm0000
MOV.W	@Rm,Rn	0110nnnnmmmm0001
MOV.L	@Rm,Rn	0110nnnnmmmm0010
MOV.B	Rm,@-Rn	0010nnnnmmmm0100
MOV.W	Rm,@-Rn	0010nnnnmmmm0101
MOV.L	Rm,@-Rn	0010nnnnmmmm0110
MOV.B	@Rm+,Rn	0110nnnnmmmm0100
MOV.W	@Rm+,Rn	0110nnnnmmmm0101
MOV.L	@Rm+,Rn	0110nnnnmmmm0110
MOV.B	R0,@(disp*,Rn)	10000000nnnndddd
MOV.W	R0,@(disp*,Rn)	10000001nnnndddd
MOV.L	Rm,@(disp*,Rn)	0001nnnnmmmmdddd
MOV.B	@(disp*,Rm),R0	10000100mmmmdddd
MOV.W	@(disp*,Rm),R0	10000101mmmmdddd
MOV.L	@(disp*,Rm),Rn	0101nnnnmmmmdddd
MOV.B	Rm,@(R0,Rn)	0000nnnnmmmm0100
MOV.W	Rm,@(R0,Rn)	0000nnnnmmmm0101
MOV.L	Rm,@(R0,Rn)	0000nnnnmmmm0110
MOV.B	@(R0,Rm),Rn	0000nnnnmmmm1100
MOV.W	@(R0,Rm),Rn	0000nnnnmmmm1101
MOV.L	@(R0,Rm),Rn	0000nnnnmmmm1110
MOV.B	R0,@(disp*,GBR)	11000000dddddddd
MOV.W	R0,@(disp*,GBR)	11000001dddddddd
MOV.L	R0,@(disp*,GBR)	11000010dddddddd
MOV.B	@(disp*,GBR),R0	11000100dddddddd
MOV.W	@(disp*,GBR),R0	11000101dddddddd
MOV.L	@(disp*,GBR),R0	11000110dddddddd
MOVA	@(disp*,PC),R0	11000111dddddddd
MOVCO.L	R0,@Rn	0000nnnn01110011
MOVLI.L	@Rm,R0	0000mmmm01100011
MOVUA.L	@Rm,R0	0100mmmm10101001
MOVUA.L	@Rm+,R0	0100mmmm11101001
MOVT	Rn	0000nnnn00101001
SWAP.B	Rm,Rn	0110nnnnmmmm1000
SWAP.W	Rm,Rn	0110nnnnmmmm1001
XTRCT	Rm,Rn	0010nnnnmmmm1101
ADD_r	Rm,Rn	0011nnnnmmmm1100
ADD_i	#imm,Rn	0111nnnniiiiiiii
ADDC	Rm,Rn	0011nnnnmmmm1110
ADDV	Rm,Rn	0011nnnnmmmm1111
CMP/EQ	#imm,R0	10001000iiiiiiii
CMP/EQ	Rm,Rn	0011nnnnmmmm0000
CMP/HS	Rm,Rn	0011nnnnmmmm0010
CMP/GE	Rm,Rn	0011nnnnmmmm0011
CMP/HI	Rm,Rn	0011nnnnmmmm0110
CMP/GT	Rm,Rn	0011nnnnmmmm0111
CMP/PZ	Rn	0100nnnn00010001
CMP/PL	Rn	0100nnnn00010101
CMP/STR	Rm,Rn	0010nnnnmmmm1100
DIV1	Rm,Rn	0011nnnnmmmm0100
DIV0S	Rm,Rn	0010nnnnmmmm0111
DIV0U		0000000000011001
DMULS.L	Rm,Rn	0011nnnnmmmm1101
DMULU.L	Rm,Rn	0011nnnnmmmm0101
DT	Rn	0100nnnn00010000
EXTS.B	Rm,Rn	0110nnnnmmmm1110
EXTS.W	Rm,Rn	0110nnnnmmmm1111
EXTU.B	Rm,Rn	0110nnnnmmmm1100
EXTU.W	Rm,Rn	0110nnnnmmmm1101
MAC.L	@Rm+,@Rn+	0000nnnnmmmm1111
MAC.W	@Rm+,@Rn+	0100nnnnmmmm1111
MUL.L	Rm,Rn	0000nnnnmmmm0111
MULS.W	Rm,Rn	0010nnnnmmmm1111
MULU.W	Rm,Rn	0010nnnnmmmm1110
NEG	Rm,Rn	0110nnnnmmmm1011
NEGC	Rm,Rn	0110nnnnmmmm1010
SUB	Rm,Rn	0011nnnnmmmm1000
SUBC	Rm,Rn	0011nnnnmmmm1010
SUBV	Rm,Rn	0011nnnnmmmm1011
AND	Rm,Rn	0010nnnnmmmm1001
AND_i	#imm,R0	11001001iiiiiiii
AND.B	#imm,@(R0,GBR)	11001101iiiiiiii
NOT	Rm,Rn	0110nnnnmmmm0111
OR	Rm,Rn	0010nnnnmmmm1011
OR_i	#imm,R0	11001011iiiiiiii
OR.B	#imm,@(R0,GBR)	11001111iiiiiiii
TAS.B	@Rn	0100nnnn00011011
TST	Rm,Rn	0010nnnnmmmm1000
TST_i	#imm,R0	11001000iiiiiiii
TST.B	#imm,@(R0,GBR)	11001100iiiiiiii
XOR	Rm,Rn	0010nnnnmmmm1010
XOR_i	#imm,R0	11001010iiiiiiii
XOR.B	#imm,@(R0,GBR)	11001110iiiiiiii
ROTL	Rn	0100nnnn00000100
ROTR	Rn	0100nnnn00000101
ROTCL	Rn	0100nnnn00100100
ROTCR	Rn	0100nnnn00100101
SHAD	Rm,Rn	0100nnnnmmmm1100
SHAL	Rn	0100nnnn00100000
SHAR	Rn	0100nnnn00100001
SHLD	Rm,Rn	0100nnnnmmmm1101
SHLL	Rn	0100nnnn00000000
SHLR	Rn	0100nnnn00000001
SHLL2	Rn	0100nnnn00001000
SHLR2	Rn	0100nnnn00001001
SHLL8	Rn	0100nnnn00011000
SHLR8	Rn	0100nnnn00011001
SHLL16	Rn	0100nnnn00101000
SHLR16	Rn	0100nnnn00101001
BF	label	10001011dddddddd
BF/S	label	10001111dddddddd
BT	label	10001001dddddddd
BT/S	label	10001101dddddddd
BRA	label	1010dddddddddddd
BRAF	Rn	0000nnnn00100011
BSR	label	1011dddddddddddd
BSRF	Rn	0000nnnn00000011
JMP	@Rn	0100nnnn00101011
JSR	@Rn	0100nnnn00001011
RTS		0000000000001011
CLRMAC		0000000000101000
CLRS		0000000001001000
CLRT		0000000000001000
ICBI	@Rn	0000nnnn11100011
LDC	Rm,SR	0100mmmm00001110
LDC	Rm,GBR	0100mmmm00011110
LDC	Rm,VBR	0100mmmm00101110
LDC	Rm,SGR	0100mmmm00111010
LDC	Rm,SSR	0100mmmm00111110
LDC	Rm,SPC	0100mmmm01001110
LDC	Rm,DBR	0100mmmm11111010
LDC	Rm,Rn_BANK	0100mmmm1nnn1110
LDC.L	@Rm+,SR	0100mmmm00000111
LDC.L	@Rm+,GBR	0100mmmm00010111
LDC.L	@Rm+,VBR	0100mmmm00100111
LDC.L	@Rm+,SGR	0100mmmm00110110
LDC.L	@Rm+,SSR	0100mmmm00110111
LDC.L	@Rm+,SPC	0100mmmm01000111
LDC.L	@Rm+,DBR	0100mmmm11110110
LDC.L	@Rm+,Rn_BANK	0100mmmm1nnn0111
LDS	Rm,MACH	0100mmmm00001010
LDS	Rm,MACL	0100mmmm00011010
LDS	Rm,PR	0100mmmm00101010
LDS.L	@Rm+,MACH	0100mmmm00000110
LDS.L	@Rm+,MACL	0100mmmm00010110
LDS.L	@Rm+,PR	0100mmmm00100110
LDTLB		0000000000111000
MOVCA.L	R0,@Rn	0000nnnn11000011
NOP		0000000000001001
OCBI	@Rn	0000nnnn10010011
OCBP	@Rn	0000nnnn10100011
OCBWB	@Rn	0000nnnn10110011
PREF	@Rn	0000nnnn10000011
PREFI	@Rn	0000nnnn11010011
RTE		0000000000101011
SETS		0000000001011000
SETT		0000000000011000
SLEEP		0000000000011011
STC	SR,Rn	0000nnnn00000010
STC	GBR,Rn	0000nnnn00010010
STC	VBR,Rn	0000nnnn00100010
STC	SSR,Rn	0000nnnn00110010
STC	SPC,Rn	0000nnnn01000010
STC	SGR,Rn	0000nnnn00111010
STC	DBR,Rn	0000nnnn11111010
STC	Rm_BANK,Rn	0000nnnn1mmm0010
STC.L	SR,@-Rn	0100nnnn00000011
STC.L	GBR,@-Rn	0100nnnn00010011
STC.L	VBR,@-Rn	0100nnnn00100011
STC.L	SSR,@-Rn	0100nnnn00110011
STC.L	SPC,@-Rn	0100nnnn01000011
STC.L	SGR,@-Rn	0100nnnn00110010
STC.L	DBR,@-Rn	0100nnnn11110010
STC.L	Rm_BANK,@-Rn	0100nnnn1mmm0011
STS	MACH,Rn	0000nnnn00001010
STS	MACL,Rn	0000nnnn00011010
STS	PR,Rn	0000nnnn00101010
STS.L	MACH,@-Rn	0100nnnn00000010
STS.L	MACL,@-Rn	0100nnnn00010010
STS.L	PR,@-Rn	0100nnnn00100010
SYNCO		0000000010101011
TRAPA	#imm	11000011iiiiiiii
FLDI0	FRn	1111nnnn10001101
FLDI1	FRn	1111nnnn10011101
FMOV	FRm,FRn	1111nnnnmmmm1100
FMOV.S	@Rm,FRn	1111nnnnmmmm1000
FMOV.S	@(R0,Rm),FRn	1111nnnnmmmm0110
FMOV.S	@Rm+,FRn	1111nnnnmmmm1001
FMOV.S	FRm,@Rn	1111nnnnmmmm1010
FMOV.S	FRm,@-Rn	1111nnnnmmmm1011
FMOV.S	FRm,@(R0,Rn)	1111nnnnmmmm0111
FMOV	DRm,DRn	1111nnn0mmm01100
FMOV	@Rm,DRn	1111nnn0mmmm1000
FMOV	@(R0,Rm),DRn	1111nnn0mmmm0110
FMOV	@Rm+,DRn	1111nnn0mmmm1001
FMOV	DRm,@Rn	1111nnnnmmm01010
FMOV	DRm,@-Rn	1111nnnnmmm01011
FMOV	DRm,@(R0,Rn)	1111nnnnmmm00111
FLDS	FRm,FPUL	1111mmmm00011101
FSTS	FPUL,FRn	1111nnnn00001101
FABS	FRn	1111nnnn01011101
FADD	FRm,FRn	1111nnnnmmmm0000
FCMP/EQ	FRm,FRn	1111nnnnmmmm0100
FCMP/GT	FRm,FRn	1111nnnnmmmm0101
FDIV	FRm,FRn	1111nnnnmmmm0011
FLOAT	FPUL,FRn	1111nnnn00101101
FMAC	FR0,FRm,FRn	1111nnnnmmmm1110
FMUL	FRm,FRn	1111nnnnmmmm0010
FNEG	FRn	1111nnnn01001101
FSQRT	FRn	1111nnnn01101101
FSUB	FRm,FRn	1111nnnnmmmm0001
FTRC	FRm,FPUL	1111mmmm00111101
FABS	DRn	1111nnn001011101
FADD	DRm,DRn	1111nnn0mmm00000
FCMP/EQ	DRm,DRn	1111nnn0mmm00100
FCMP/GT	DRm,DRn	1111nnn0mmm00101
FDIV	DRm,DRn	1111nnn0mmm00011
FCNVDS	DRm,FPUL	1111mmm010111101
FCNVSD	FPUL,DRn	1111nnn010101101
FLOAT	FPUL,DRn	1111nnn000101101
FMUL	DRm,DRn	1111nnn0mmm00010
FNEG	DRn	1111nnn001001101
FSQRT	DRn	1111nnn001101101
FSUB	DRm,DRn	1111nnn0mmm00001
FTRC	DRm,FPUL	1111mmm000111101
LDS	Rm,FPSCR	0100mmmm01101010
LDS	Rm,FPUL	0100mmmm01011010
LDS.L	@Rm+,FPSCR	0100mmmm01100110
LDS.L	@Rm+,FPUL	0100mmmm01010110
STS	FPSCR,Rn	0000nnnn01101010
STS	FPUL,Rn	0000nnnn01011010
STS.L	FPSCR,@-Rn	0100nnnn01100010
STS.L	FPUL,@-Rn	0100nnnn01010010
FMOV	DRm,XDn	1111nnn1mmm01100
FMOV	XDm,DRn	1111nnn0mmm11100
FMOV	XDm,XDn	1111nnn1mmm11100
FMOV	@Rm,XDn	1111nnn1mmmm1000
FMOV	@Rm+,XDn	1111nnn1mmmm1001
FMOV	@(R0,Rm),XDn	1111nnn1mmmm0110
FMOV	XDm,@Rn	1111nnnnmmm11010
FMOV	XDm,@-Rn	1111nnnnmmm11011
FMOV	XDm,@(R0,Rn)	1111nnnnmmm10111
FIPR	FVm,FVn	1111nnmm11101101
FTRV	XMTRX,FVn	1111nn0111111101
FRCHG		1111101111111101
FSCHG		1111001111111101
FPCHG		1111011111111101
FSRRA	FRn	1111nnnn01111101
FSCA	FPUL,DRn	1111nnn011111101
MOV.B	R0,@Rn+	0100nnnn10001011
MOV.W	R0,@Rn+	0100nnnn10011011
MOV.L	R0,@Rn+	0100nnnn10101011
MOV.B	@-Rm,R0	0100mmmm11001011
MOV.W	@-Rm,R0	0100mmmm11011011
MOV.L	@-Rm,R0	0100mmmm11101011
MOV.B	Rm,@(disp12,Rn)	0011nnnnmmmm00010000dddddddddddd
MOV.W	Rm,@(disp12,Rn)	0011nnnnmmmm00010001dddddddddddd
MOV.L	Rm,@(disp12,Rn)	0011nnnnmmmm00010010dddddddddddd
MOV.B	@(disp12,Rm),Rn	0011nnnnmmmm00010100dddddddddddd
MOV.W	@(disp12,Rm),Rn	0011nnnnmmmm00010101dddddddddddd
MOV.L	@(disp12,Rm),Rn	0011nnnnmmmm00010110dddddddddddd
MOVI20	#imm20,Rn	0000nnnniiii0000iiiiiiiiiiiiiiii
MOVI20S	#imm20,Rn	0000nnnniiii0001iiiiiiiiiiiiiiii
MOVML.L	Rm,@-R15	0100mmmm11110001
MOVML.L	@R15+,Rn	0100nnnn11110101
MOVMU.L	Rm,@-R15	0100mmmm11110000
MOVMU.L	@R15+,Rn	0100nnnn11110100
MOVRT	Rn	0000nnnn00111001
MOVU.B	@(disp12,Rm),Rn	0011nnnnmmmm00011000dddddddddddd
MOVU.W	@(disp12,Rm),Rn	0011nnnnmmmm00011001dddddddddddd
NOTT		0000000001101000
CLIPS.B	Rn	0100nnnn10010001
CLIPS.W	Rn	0100nnnn10010101
CLIPU.B	Rn	0100nnnn10000001
CLIPU.W	Rn	0100nnnn10000101
DIVS	R0,Rn	0100nnnn10010100
DIVU	R0,Rn	0100nnnn10000100
MULR	R0,Rn	0100nnnn10000000
JSR/N	@Rm	0100mmmm01001011
JSR/N	@@(disp8,TBR)	10000011dddddddd
RTS/N		0000000001101011
RTV/N	Rm	0000mmmm01111011
LDBANK	@Rm,R0	0100mmmm11100101
LDC	Rm,TBR	0100mmmm01001010
RESBANK		0000000001011011
STBANK	R0,@Rn	0100nnnn11100001
STC	TBR,Rn	0000nnnn01001010
FMOV.S	@(disp12,Rm),FRn	0011nnnnmmmm00010111dddddddddddd
FMOV.D	@(disp12,Rm),DRn	0011nnn0mmmm00010111dddddddddddd
FMOV.S	FRm,@(disp12,Rn)	0011nnnnmmmm00010011dddddddddddd
FMOV.D	DRm,@(disp12,Rn)	0011nnnnmmm000010011dddddddddddd
FMOV.S	FRm,@(disp12,Rn)	0011nnnnmmmm00010011dddddddddddd
FMOV.D	DRm,@(disp12,Rn)	0011nnnnmmm000010011dddddddddddd
BAND.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010100dddddddddddd
BANDNOT.B	#imm3,@(disp12,Rn)	0011nnnn0iii10011100dddddddddddd
BCLR.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010000dddddddddddd
BCLR	#imm3,Rn	10000110nnnn0iii
BLD.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010011dddddddddddd
BLD	#imm3,Rn	10000111nnnn1iii
BLDNOT.B	#imm3,@(disp12,Rn)	0011nnnn0iii10011011dddddddddddd
BOR.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010101dddddddddddd
BORNOT.B	#imm3,@(disp12,Rn)	0011nnnn0iii10011101dddddddddddd
BSET.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010001dddddddddddd
BSET	#imm3,Rn	10000110nnnn1iii
BST.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010010dddddddddddd
BST	#imm3,Rn	10000111nnnn0iii
BXOR.B	#imm3,@(disp12,Rn)	0011nnnn0iii10010110dddddddddddd
LDRE	@(disp,PC)	10001110dddddddd
LDRS	@(disp,PC)	10001100dddddddd
SETRC	Rm	0100mmmm00010100
SETRC 	#imm	10000010iiiiiiii
LDRC 	#imm	10001010iiiiiiii
EOF

code_list.each_line { |line|
  l = line.split
  l[2] = l[1] if (l.length < 3)
  if l[2].length > 16 then
    l[2] = l[2][0..15]
  end
  if l[2][0..3].to_i(2) < 8 || l[2][0..3].to_i(2) == 15 then
    b = l[2][0..3] + l[2][12..15]
  else
    b = l[2][0..7]
  end
  if b =~ /^\d+$/ then
    no = b.to_i(2)
    if no == 0x00 || no == 0x01 || no == 0x31 || no == 0x39 then
      # SH2A 32bit instructions prefix
      next
    end
    next if out[no] == "op" + l[0]
    if (no >= 0x20 && no <= 0x22) || (no >= 0x60 && no <= 0x62)then
      l[0] = "MOV_rind"
    end
    if no >= 0x24 && no <= 0x26 then
      l[0] = "MOV_rpd"
    end
    if no >= 0x64 && no <= 0x66 then
      l[0] = "MOV_rpi"
    end
    if no == 0x80 || no == 0x81 || no == 0x84 || no == 0x85 then
      l[0] = "MOV_BW_dsp"
    end
    if no == 0x88 then
      l[0] = "CMP_EQi"
    end
    if no == 0xc0 || no == 0xc1 || no == 0xc2 || no == 0xc4 || no == 0xc5 || no == 0xc6 then
      l[0] = "MOV_gbr"
    end  
    if out[no] == "NULL" then
      out[no] = "op" + l[0]
    else
      hi = b.to_i(2) / 16
      lo = b.to_i(2) % 16
      if (hi < 0x8) || (hi >= 0x0f) then
        out[no] = "op" + hi.to_s(16) + "xx" + lo.to_s(16)
      else
        out[no] = "op" + hi.to_s(16) + lo.to_s(16) + "xx"
      end
    end
  else
    n = (l[2][0..3].to_i(2)) * 16
    if n != 0x80 && n != 0xc0 then
      if n == 0x10 || n == 0x50 then
        l[0] = "MOV_L_dsp"
      end
      if n == 0x90 || n == 0xd0 then
        l[0] = "MOV_pc"
      end
      16.times { |i|
        out[n + i] = "op" + l[0]
      }
    end
  end
}
code = 0
print "bool (*decode[])(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode, sh_info *info, cs_detail *detail) = {\n"
(256 / 8).times { |i|
  bit = "0000000" + code.to_s(2)
  print "\t/// ", bit[-8,8], "\n\t"
  8.times { |j|
    o = out[i * 8 + j].gsub(/[\.\/]/, '_')
    print o, ", "
    code = code.succ
  }
  print "\n"
}
print "};\n"

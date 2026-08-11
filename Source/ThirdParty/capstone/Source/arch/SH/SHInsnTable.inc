bool (*decode[])(uint16_t code, uint64_t address, MCInst *MI, cs_mode mode, sh_info *info, cs_detail *detail) = {
	/// 00000000
	NULL, NULL, opSTC, op0xx3, opMOV_B, opMOV_W, opMOV_L, opMUL_L, 
	/// 00001000
	op0xx8, op0xx9, op0xxa, op0xxb, opMOV_B, opMOV_W, opMOV_L, opMAC_L, 
	/// 00010000
	opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, 
	/// 00011000
	opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, 
	/// 00100000
	opMOV_rind, opMOV_rind, opMOV_rind, NULL, opMOV_rpd, opMOV_rpd, opMOV_rpd, opDIV0S, 
	/// 00101000
	opTST, opAND, opXOR, opOR, opCMP_STR, opXTRCT, opMULU_W, opMULS_W, 
	/// 00110000
	opCMP_EQ, NULL, opCMP_HS, opCMP_GE, opDIV1, opDMULU_L, opCMP_HI, opCMP_GT, 
	/// 00111000
	opSUB, NULL, opSUBC, opSUBV, opADD_r, opDMULS_L, opADDC, opADDV, 
	/// 01000000
	op4xx0, op4xx1, op4xx2, opSTC_L, op4xx4, op4xx5, op4xx6, opLDC_L, 
	/// 01001000
	op4xx8, op4xx9, op4xxa, op4xxb, opSHAD, opSHLD, opLDC, opMAC_W, 
	/// 01010000
	opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, 
	/// 01011000
	opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, opMOV_L_dsp, 
	/// 01100000
	opMOV_rind, opMOV_rind, opMOV_rind, opMOV, opMOV_rpi, opMOV_rpi, opMOV_rpi, opNOT, 
	/// 01101000
	opSWAP_B, opSWAP_W, opNEGC, opNEG, opEXTU_B, opEXTU_W, opEXTS_B, opEXTS_W, 
	/// 01110000
	opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, 
	/// 01111000
	opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, opADD_i, 
	/// 10000000
	opMOV_BW_dsp, opMOV_BW_dsp, opSETRC, opJSR_N, opMOV_BW_dsp, opMOV_BW_dsp, op86xx, op87xx, 
	/// 10001000
	opCMP_EQi, opBT, opLDRC, opBF, opLDRS, opBT_S, opLDRE, opBF_S, 
	/// 10010000
	opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, 
	/// 10011000
	opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, 
	/// 10100000
	opBRA, opBRA, opBRA, opBRA, opBRA, opBRA, opBRA, opBRA, 
	/// 10101000
	opBRA, opBRA, opBRA, opBRA, opBRA, opBRA, opBRA, opBRA, 
	/// 10110000
	opBSR, opBSR, opBSR, opBSR, opBSR, opBSR, opBSR, opBSR, 
	/// 10111000
	opBSR, opBSR, opBSR, opBSR, opBSR, opBSR, opBSR, opBSR, 
	/// 11000000
	opMOV_gbr, opMOV_gbr, opMOV_gbr, opTRAPA, opMOV_gbr, opMOV_gbr, opMOV_gbr, opMOVA, 
	/// 11001000
	opTST_i, opAND_i, opXOR_i, opOR_i, opTST_B, opAND_B, opXOR_B, opOR_B, 
	/// 11010000
	opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, 
	/// 11011000
	opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, opMOV_pc, 
	/// 11100000
	opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, 
	/// 11101000
	opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, opMOV_i, 
	/// 11110000
	opFADD, opFSUB, opFMUL, opFDIV, opFCMP_EQ, opFCMP_GT, opfxx6, opfxx7, 
	/// 11111000
	opfxx8, opfxx9, opfxxa, opfxxb, opFMOV, opfxxd, opFMAC, NULL, 
};


/* Capstone Disassembly Engine, http://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

/*===- TableGen'erated file -------------------------------------*- C++ -*-===*\
|*                                                                            *|
|* Target Register Enum Values                                                *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

#ifdef GET_REGINFO_ENUM
#undef GET_REGINFO_ENUM

enum {
  PPC_NoRegister,
  PPC_BP = 1,
  PPC_CARRY = 2,
  PPC_CTR = 3,
  PPC_FP = 4,
  PPC_LR = 5,
  PPC_RM = 6,
  PPC_SPEFSCR = 7,
  PPC_VRSAVE = 8,
  PPC_XER = 9,
  PPC_ZERO = 10,
  PPC_BP8 = 11,
  PPC_CR0 = 12,
  PPC_CR1 = 13,
  PPC_CR2 = 14,
  PPC_CR3 = 15,
  PPC_CR4 = 16,
  PPC_CR5 = 17,
  PPC_CR6 = 18,
  PPC_CR7 = 19,
  PPC_CTR8 = 20,
  PPC_F0 = 21,
  PPC_F1 = 22,
  PPC_F2 = 23,
  PPC_F3 = 24,
  PPC_F4 = 25,
  PPC_F5 = 26,
  PPC_F6 = 27,
  PPC_F7 = 28,
  PPC_F8 = 29,
  PPC_F9 = 30,
  PPC_F10 = 31,
  PPC_F11 = 32,
  PPC_F12 = 33,
  PPC_F13 = 34,
  PPC_F14 = 35,
  PPC_F15 = 36,
  PPC_F16 = 37,
  PPC_F17 = 38,
  PPC_F18 = 39,
  PPC_F19 = 40,
  PPC_F20 = 41,
  PPC_F21 = 42,
  PPC_F22 = 43,
  PPC_F23 = 44,
  PPC_F24 = 45,
  PPC_F25 = 46,
  PPC_F26 = 47,
  PPC_F27 = 48,
  PPC_F28 = 49,
  PPC_F29 = 50,
  PPC_F30 = 51,
  PPC_F31 = 52,
  PPC_FP8 = 53,
  PPC_LR8 = 54,
  PPC_QF0 = 55,
  PPC_QF1 = 56,
  PPC_QF2 = 57,
  PPC_QF3 = 58,
  PPC_QF4 = 59,
  PPC_QF5 = 60,
  PPC_QF6 = 61,
  PPC_QF7 = 62,
  PPC_QF8 = 63,
  PPC_QF9 = 64,
  PPC_QF10 = 65,
  PPC_QF11 = 66,
  PPC_QF12 = 67,
  PPC_QF13 = 68,
  PPC_QF14 = 69,
  PPC_QF15 = 70,
  PPC_QF16 = 71,
  PPC_QF17 = 72,
  PPC_QF18 = 73,
  PPC_QF19 = 74,
  PPC_QF20 = 75,
  PPC_QF21 = 76,
  PPC_QF22 = 77,
  PPC_QF23 = 78,
  PPC_QF24 = 79,
  PPC_QF25 = 80,
  PPC_QF26 = 81,
  PPC_QF27 = 82,
  PPC_QF28 = 83,
  PPC_QF29 = 84,
  PPC_QF30 = 85,
  PPC_QF31 = 86,
  PPC_R0 = 87,
  PPC_R1 = 88,
  PPC_R2 = 89,
  PPC_R3 = 90,
  PPC_R4 = 91,
  PPC_R5 = 92,
  PPC_R6 = 93,
  PPC_R7 = 94,
  PPC_R8 = 95,
  PPC_R9 = 96,
  PPC_R10 = 97,
  PPC_R11 = 98,
  PPC_R12 = 99,
  PPC_R13 = 100,
  PPC_R14 = 101,
  PPC_R15 = 102,
  PPC_R16 = 103,
  PPC_R17 = 104,
  PPC_R18 = 105,
  PPC_R19 = 106,
  PPC_R20 = 107,
  PPC_R21 = 108,
  PPC_R22 = 109,
  PPC_R23 = 110,
  PPC_R24 = 111,
  PPC_R25 = 112,
  PPC_R26 = 113,
  PPC_R27 = 114,
  PPC_R28 = 115,
  PPC_R29 = 116,
  PPC_R30 = 117,
  PPC_R31 = 118,
  PPC_S0 = 119,
  PPC_S1 = 120,
  PPC_S2 = 121,
  PPC_S3 = 122,
  PPC_S4 = 123,
  PPC_S5 = 124,
  PPC_S6 = 125,
  PPC_S7 = 126,
  PPC_S8 = 127,
  PPC_S9 = 128,
  PPC_S10 = 129,
  PPC_S11 = 130,
  PPC_S12 = 131,
  PPC_S13 = 132,
  PPC_S14 = 133,
  PPC_S15 = 134,
  PPC_S16 = 135,
  PPC_S17 = 136,
  PPC_S18 = 137,
  PPC_S19 = 138,
  PPC_S20 = 139,
  PPC_S21 = 140,
  PPC_S22 = 141,
  PPC_S23 = 142,
  PPC_S24 = 143,
  PPC_S25 = 144,
  PPC_S26 = 145,
  PPC_S27 = 146,
  PPC_S28 = 147,
  PPC_S29 = 148,
  PPC_S30 = 149,
  PPC_S31 = 150,
  PPC_V0 = 151,
  PPC_V1 = 152,
  PPC_V2 = 153,
  PPC_V3 = 154,
  PPC_V4 = 155,
  PPC_V5 = 156,
  PPC_V6 = 157,
  PPC_V7 = 158,
  PPC_V8 = 159,
  PPC_V9 = 160,
  PPC_V10 = 161,
  PPC_V11 = 162,
  PPC_V12 = 163,
  PPC_V13 = 164,
  PPC_V14 = 165,
  PPC_V15 = 166,
  PPC_V16 = 167,
  PPC_V17 = 168,
  PPC_V18 = 169,
  PPC_V19 = 170,
  PPC_V20 = 171,
  PPC_V21 = 172,
  PPC_V22 = 173,
  PPC_V23 = 174,
  PPC_V24 = 175,
  PPC_V25 = 176,
  PPC_V26 = 177,
  PPC_V27 = 178,
  PPC_V28 = 179,
  PPC_V29 = 180,
  PPC_V30 = 181,
  PPC_V31 = 182,
  PPC_VF0 = 183,
  PPC_VF1 = 184,
  PPC_VF2 = 185,
  PPC_VF3 = 186,
  PPC_VF4 = 187,
  PPC_VF5 = 188,
  PPC_VF6 = 189,
  PPC_VF7 = 190,
  PPC_VF8 = 191,
  PPC_VF9 = 192,
  PPC_VF10 = 193,
  PPC_VF11 = 194,
  PPC_VF12 = 195,
  PPC_VF13 = 196,
  PPC_VF14 = 197,
  PPC_VF15 = 198,
  PPC_VF16 = 199,
  PPC_VF17 = 200,
  PPC_VF18 = 201,
  PPC_VF19 = 202,
  PPC_VF20 = 203,
  PPC_VF21 = 204,
  PPC_VF22 = 205,
  PPC_VF23 = 206,
  PPC_VF24 = 207,
  PPC_VF25 = 208,
  PPC_VF26 = 209,
  PPC_VF27 = 210,
  PPC_VF28 = 211,
  PPC_VF29 = 212,
  PPC_VF30 = 213,
  PPC_VF31 = 214,
  PPC_VSL0 = 215,
  PPC_VSL1 = 216,
  PPC_VSL2 = 217,
  PPC_VSL3 = 218,
  PPC_VSL4 = 219,
  PPC_VSL5 = 220,
  PPC_VSL6 = 221,
  PPC_VSL7 = 222,
  PPC_VSL8 = 223,
  PPC_VSL9 = 224,
  PPC_VSL10 = 225,
  PPC_VSL11 = 226,
  PPC_VSL12 = 227,
  PPC_VSL13 = 228,
  PPC_VSL14 = 229,
  PPC_VSL15 = 230,
  PPC_VSL16 = 231,
  PPC_VSL17 = 232,
  PPC_VSL18 = 233,
  PPC_VSL19 = 234,
  PPC_VSL20 = 235,
  PPC_VSL21 = 236,
  PPC_VSL22 = 237,
  PPC_VSL23 = 238,
  PPC_VSL24 = 239,
  PPC_VSL25 = 240,
  PPC_VSL26 = 241,
  PPC_VSL27 = 242,
  PPC_VSL28 = 243,
  PPC_VSL29 = 244,
  PPC_VSL30 = 245,
  PPC_VSL31 = 246,
  PPC_VSX32 = 247,
  PPC_VSX33 = 248,
  PPC_VSX34 = 249,
  PPC_VSX35 = 250,
  PPC_VSX36 = 251,
  PPC_VSX37 = 252,
  PPC_VSX38 = 253,
  PPC_VSX39 = 254,
  PPC_VSX40 = 255,
  PPC_VSX41 = 256,
  PPC_VSX42 = 257,
  PPC_VSX43 = 258,
  PPC_VSX44 = 259,
  PPC_VSX45 = 260,
  PPC_VSX46 = 261,
  PPC_VSX47 = 262,
  PPC_VSX48 = 263,
  PPC_VSX49 = 264,
  PPC_VSX50 = 265,
  PPC_VSX51 = 266,
  PPC_VSX52 = 267,
  PPC_VSX53 = 268,
  PPC_VSX54 = 269,
  PPC_VSX55 = 270,
  PPC_VSX56 = 271,
  PPC_VSX57 = 272,
  PPC_VSX58 = 273,
  PPC_VSX59 = 274,
  PPC_VSX60 = 275,
  PPC_VSX61 = 276,
  PPC_VSX62 = 277,
  PPC_VSX63 = 278,
  PPC_X0 = 279,
  PPC_X1 = 280,
  PPC_X2 = 281,
  PPC_X3 = 282,
  PPC_X4 = 283,
  PPC_X5 = 284,
  PPC_X6 = 285,
  PPC_X7 = 286,
  PPC_X8 = 287,
  PPC_X9 = 288,
  PPC_X10 = 289,
  PPC_X11 = 290,
  PPC_X12 = 291,
  PPC_X13 = 292,
  PPC_X14 = 293,
  PPC_X15 = 294,
  PPC_X16 = 295,
  PPC_X17 = 296,
  PPC_X18 = 297,
  PPC_X19 = 298,
  PPC_X20 = 299,
  PPC_X21 = 300,
  PPC_X22 = 301,
  PPC_X23 = 302,
  PPC_X24 = 303,
  PPC_X25 = 304,
  PPC_X26 = 305,
  PPC_X27 = 306,
  PPC_X28 = 307,
  PPC_X29 = 308,
  PPC_X30 = 309,
  PPC_X31 = 310,
  PPC_ZERO8 = 311,
  PPC_CR0EQ = 312,
  PPC_CR1EQ = 313,
  PPC_CR2EQ = 314,
  PPC_CR3EQ = 315,
  PPC_CR4EQ = 316,
  PPC_CR5EQ = 317,
  PPC_CR6EQ = 318,
  PPC_CR7EQ = 319,
  PPC_CR0GT = 320,
  PPC_CR1GT = 321,
  PPC_CR2GT = 322,
  PPC_CR3GT = 323,
  PPC_CR4GT = 324,
  PPC_CR5GT = 325,
  PPC_CR6GT = 326,
  PPC_CR7GT = 327,
  PPC_CR0LT = 328,
  PPC_CR1LT = 329,
  PPC_CR2LT = 330,
  PPC_CR3LT = 331,
  PPC_CR4LT = 332,
  PPC_CR5LT = 333,
  PPC_CR6LT = 334,
  PPC_CR7LT = 335,
  PPC_CR0UN = 336,
  PPC_CR1UN = 337,
  PPC_CR2UN = 338,
  PPC_CR3UN = 339,
  PPC_CR4UN = 340,
  PPC_CR5UN = 341,
  PPC_CR6UN = 342,
  PPC_CR7UN = 343,
  PPC_NUM_TARGET_REGS 	// 344
};

// Register classes
enum {
  PPC_VSSRCRegClassID = 0,
  PPC_GPRCRegClassID = 1,
  PPC_GPRC_NOR0RegClassID = 2,
  PPC_SPE4RCRegClassID = 3,
  PPC_GPRC_and_GPRC_NOR0RegClassID = 4,
  PPC_CRBITRCRegClassID = 5,
  PPC_F4RCRegClassID = 6,
  PPC_CRRCRegClassID = 7,
  PPC_CARRYRCRegClassID = 8,
  PPC_CTRRCRegClassID = 9,
  PPC_VRSAVERCRegClassID = 10,
  PPC_SPILLTOVSRRCRegClassID = 11,
  PPC_VSFRCRegClassID = 12,
  PPC_G8RCRegClassID = 13,
  PPC_G8RC_NOX0RegClassID = 14,
  PPC_SPILLTOVSRRC_and_VSFRCRegClassID = 15,
  PPC_G8RC_and_G8RC_NOX0RegClassID = 16,
  PPC_F8RCRegClassID = 17,
  PPC_SPERCRegClassID = 18,
  PPC_VFRCRegClassID = 19,
  PPC_SPERC_with_sub_32_in_GPRC_NOR0RegClassID = 20,
  PPC_SPILLTOVSRRC_and_VFRCRegClassID = 21,
  PPC_SPILLTOVSRRC_and_F4RCRegClassID = 22,
  PPC_CTRRC8RegClassID = 23,
  PPC_VSRCRegClassID = 24,
  PPC_VSRC_with_sub_64_in_SPILLTOVSRRCRegClassID = 25,
  PPC_QSRCRegClassID = 26,
  PPC_VRRCRegClassID = 27,
  PPC_VSLRCRegClassID = 28,
  PPC_VRRC_with_sub_64_in_SPILLTOVSRRCRegClassID = 29,
  PPC_QSRC_with_sub_64_in_SPILLTOVSRRCRegClassID = 30,
  PPC_VSLRC_with_sub_64_in_SPILLTOVSRRCRegClassID = 31,
  PPC_QBRCRegClassID = 32,
  PPC_QFRCRegClassID = 33,
  PPC_QBRC_with_sub_64_in_SPILLTOVSRRCRegClassID = 34,
};

#endif // GET_REGINFO_ENUM

#ifdef GET_REGINFO_MC_DESC
#undef GET_REGINFO_MC_DESC


static const MCPhysReg PPCRegDiffLists[] = {
  /* 0 */ 0, 0,
  /* 2 */ 65497, 1, 1, 1, 0,
  /* 7 */ 3, 0,
  /* 9 */ 10, 0,
  /* 11 */ 21, 0,
  /* 13 */ 316, 65528, 65528, 24, 0,
  /* 18 */ 32, 0,
  /* 20 */ 49, 0,
  /* 22 */ 74, 0,
  /* 24 */ 32, 160, 0,
  /* 27 */ 34, 160, 0,
  /* 30 */ 301, 0,
  /* 32 */ 64204, 0,
  /* 34 */ 64233, 0,
  /* 36 */ 64266, 0,
  /* 38 */ 64299, 0,
  /* 40 */ 64611, 0,
  /* 42 */ 65212, 0,
  /* 44 */ 65220, 0,
  /* 46 */ 65228, 0,
  /* 48 */ 65235, 0,
  /* 50 */ 65236, 0,
  /* 52 */ 65332, 0,
  /* 54 */ 65342, 0,
  /* 56 */ 65344, 0,
  /* 58 */ 65363, 0,
  /* 60 */ 65428, 0,
  /* 62 */ 65460, 0,
  /* 64 */ 65474, 0,
  /* 66 */ 65487, 0,
  /* 68 */ 65492, 0,
  /* 70 */ 65502, 0,
  /* 72 */ 65504, 0,
  /* 74 */ 65523, 0,
  /* 76 */ 65524, 0,
  /* 78 */ 65526, 0,
  /* 80 */ 65535, 0,
};

static const uint16_t PPCSubRegIdxLists[] = {
  /* 0 */ 1, 0,
  /* 2 */ 2, 0,
  /* 4 */ 5, 4, 3, 6, 0,
};

static const MCRegisterDesc PPCRegDesc[] = {
  { 4, 0, 0, 0, 0, 0 },
  { 1237, 1, 9, 1, 1281, 0 },
  { 1406, 1, 1, 1, 1281, 0 },
  { 1306, 1, 1, 1, 1281, 0 },
  { 1240, 1, 20, 1, 1281, 0 },
  { 1303, 1, 1, 1, 1281, 0 },
  { 1181, 1, 1, 1, 1281, 0 },
  { 1291, 1, 1, 1, 1281, 0 },
  { 1174, 1, 1, 1, 1281, 0 },
  { 1299, 1, 1, 1, 1031, 0 },
  { 1232, 1, 30, 1, 1031, 0 },
  { 1041, 78, 1, 0, 0, 2 },
  { 127, 13, 1, 4, 36, 6 },
  { 267, 13, 1, 4, 36, 6 },
  { 381, 13, 1, 4, 36, 6 },
  { 495, 13, 1, 4, 36, 6 },
  { 603, 13, 1, 4, 36, 6 },
  { 711, 13, 1, 4, 36, 6 },
  { 819, 13, 1, 4, 36, 6 },
  { 927, 13, 1, 4, 36, 6 },
  { 1053, 1, 1, 1, 177, 0 },
  { 115, 1, 27, 1, 177, 0 },
  { 255, 1, 27, 1, 177, 0 },
  { 369, 1, 27, 1, 177, 0 },
  { 483, 1, 27, 1, 177, 0 },
  { 591, 1, 27, 1, 177, 0 },
  { 699, 1, 27, 1, 177, 0 },
  { 807, 1, 27, 1, 177, 0 },
  { 915, 1, 27, 1, 177, 0 },
  { 1023, 1, 27, 1, 177, 0 },
  { 1150, 1, 27, 1, 177, 0 },
  { 1, 1, 27, 1, 177, 0 },
  { 141, 1, 27, 1, 177, 0 },
  { 281, 1, 27, 1, 177, 0 },
  { 395, 1, 27, 1, 177, 0 },
  { 509, 1, 27, 1, 177, 0 },
  { 617, 1, 27, 1, 177, 0 },
  { 725, 1, 27, 1, 177, 0 },
  { 833, 1, 27, 1, 177, 0 },
  { 941, 1, 27, 1, 177, 0 },
  { 1068, 1, 27, 1, 177, 0 },
  { 33, 1, 27, 1, 177, 0 },
  { 173, 1, 27, 1, 177, 0 },
  { 313, 1, 27, 1, 177, 0 },
  { 427, 1, 27, 1, 177, 0 },
  { 541, 1, 27, 1, 177, 0 },
  { 649, 1, 27, 1, 177, 0 },
  { 757, 1, 27, 1, 177, 0 },
  { 865, 1, 27, 1, 177, 0 },
  { 973, 1, 27, 1, 177, 0 },
  { 1100, 1, 27, 1, 177, 0 },
  { 65, 1, 27, 1, 177, 0 },
  { 205, 1, 27, 1, 177, 0 },
  { 1045, 66, 1, 0, 112, 2 },
  { 1049, 1, 1, 1, 352, 0 },
  { 114, 70, 1, 2, 1185, 4 },
  { 254, 70, 1, 2, 1185, 4 },
  { 368, 70, 1, 2, 1185, 4 },
  { 482, 70, 1, 2, 1185, 4 },
  { 590, 70, 1, 2, 1185, 4 },
  { 698, 70, 1, 2, 1185, 4 },
  { 806, 70, 1, 2, 1185, 4 },
  { 914, 70, 1, 2, 1185, 4 },
  { 1022, 70, 1, 2, 1185, 4 },
  { 1149, 70, 1, 2, 1185, 4 },
  { 0, 70, 1, 2, 1185, 4 },
  { 140, 70, 1, 2, 1185, 4 },
  { 280, 70, 1, 2, 1185, 4 },
  { 394, 70, 1, 2, 1185, 4 },
  { 508, 70, 1, 2, 1185, 4 },
  { 616, 70, 1, 2, 1185, 4 },
  { 724, 70, 1, 2, 1185, 4 },
  { 832, 70, 1, 2, 1185, 4 },
  { 940, 70, 1, 2, 1185, 4 },
  { 1067, 70, 1, 2, 1185, 4 },
  { 32, 70, 1, 2, 1185, 4 },
  { 172, 70, 1, 2, 1185, 4 },
  { 312, 70, 1, 2, 1185, 4 },
  { 426, 70, 1, 2, 1185, 4 },
  { 540, 70, 1, 2, 1185, 4 },
  { 648, 70, 1, 2, 1185, 4 },
  { 756, 70, 1, 2, 1185, 4 },
  { 864, 70, 1, 2, 1185, 4 },
  { 972, 70, 1, 2, 1185, 4 },
  { 1099, 70, 1, 2, 1185, 4 },
  { 64, 70, 1, 2, 1185, 4 },
  { 204, 70, 1, 2, 1185, 4 },
  { 128, 1, 24, 1, 1217, 0 },
  { 268, 1, 24, 1, 1217, 0 },
  { 382, 1, 24, 1, 1217, 0 },
  { 496, 1, 24, 1, 1217, 0 },
  { 604, 1, 24, 1, 1217, 0 },
  { 712, 1, 24, 1, 1217, 0 },
  { 820, 1, 24, 1, 1217, 0 },
  { 928, 1, 24, 1, 1217, 0 },
  { 1050, 1, 24, 1, 1217, 0 },
  { 1162, 1, 24, 1, 1217, 0 },
  { 16, 1, 24, 1, 1217, 0 },
  { 156, 1, 24, 1, 1217, 0 },
  { 296, 1, 24, 1, 1217, 0 },
  { 410, 1, 24, 1, 1217, 0 },
  { 524, 1, 24, 1, 1217, 0 },
  { 632, 1, 24, 1, 1217, 0 },
  { 740, 1, 24, 1, 1217, 0 },
  { 848, 1, 24, 1, 1217, 0 },
  { 956, 1, 24, 1, 1217, 0 },
  { 1083, 1, 24, 1, 1217, 0 },
  { 48, 1, 24, 1, 1217, 0 },
  { 188, 1, 24, 1, 1217, 0 },
  { 328, 1, 24, 1, 1217, 0 },
  { 442, 1, 24, 1, 1217, 0 },
  { 556, 1, 24, 1, 1217, 0 },
  { 664, 1, 24, 1, 1217, 0 },
  { 772, 1, 24, 1, 1217, 0 },
  { 880, 1, 24, 1, 1217, 0 },
  { 988, 1, 24, 1, 1217, 0 },
  { 1115, 1, 24, 1, 1217, 0 },
  { 80, 1, 24, 1, 1217, 0 },
  { 220, 1, 24, 1, 1217, 0 },
  { 131, 72, 1, 0, 1089, 2 },
  { 271, 72, 1, 0, 1089, 2 },
  { 385, 72, 1, 0, 1089, 2 },
  { 499, 72, 1, 0, 1089, 2 },
  { 607, 72, 1, 0, 1089, 2 },
  { 715, 72, 1, 0, 1089, 2 },
  { 823, 72, 1, 0, 1089, 2 },
  { 931, 72, 1, 0, 1089, 2 },
  { 1058, 72, 1, 0, 1089, 2 },
  { 1165, 72, 1, 0, 1089, 2 },
  { 20, 72, 1, 0, 1089, 2 },
  { 160, 72, 1, 0, 1089, 2 },
  { 300, 72, 1, 0, 1089, 2 },
  { 414, 72, 1, 0, 1089, 2 },
  { 528, 72, 1, 0, 1089, 2 },
  { 636, 72, 1, 0, 1089, 2 },
  { 744, 72, 1, 0, 1089, 2 },
  { 852, 72, 1, 0, 1089, 2 },
  { 960, 72, 1, 0, 1089, 2 },
  { 1087, 72, 1, 0, 1089, 2 },
  { 52, 72, 1, 0, 1089, 2 },
  { 192, 72, 1, 0, 1089, 2 },
  { 332, 72, 1, 0, 1089, 2 },
  { 446, 72, 1, 0, 1089, 2 },
  { 560, 72, 1, 0, 1089, 2 },
  { 668, 72, 1, 0, 1089, 2 },
  { 776, 72, 1, 0, 1089, 2 },
  { 884, 72, 1, 0, 1089, 2 },
  { 992, 72, 1, 0, 1089, 2 },
  { 1119, 72, 1, 0, 1089, 2 },
  { 84, 72, 1, 0, 1089, 2 },
  { 224, 72, 1, 0, 1089, 2 },
  { 134, 18, 1, 2, 1089, 4 },
  { 274, 18, 1, 2, 1089, 4 },
  { 388, 18, 1, 2, 1089, 4 },
  { 502, 18, 1, 2, 1089, 4 },
  { 610, 18, 1, 2, 1089, 4 },
  { 718, 18, 1, 2, 1089, 4 },
  { 826, 18, 1, 2, 1089, 4 },
  { 934, 18, 1, 2, 1089, 4 },
  { 1061, 18, 1, 2, 1089, 4 },
  { 1168, 18, 1, 2, 1089, 4 },
  { 24, 18, 1, 2, 1089, 4 },
  { 164, 18, 1, 2, 1089, 4 },
  { 304, 18, 1, 2, 1089, 4 },
  { 418, 18, 1, 2, 1089, 4 },
  { 532, 18, 1, 2, 1089, 4 },
  { 640, 18, 1, 2, 1089, 4 },
  { 748, 18, 1, 2, 1089, 4 },
  { 856, 18, 1, 2, 1089, 4 },
  { 964, 18, 1, 2, 1089, 4 },
  { 1091, 18, 1, 2, 1089, 4 },
  { 56, 18, 1, 2, 1089, 4 },
  { 196, 18, 1, 2, 1089, 4 },
  { 336, 18, 1, 2, 1089, 4 },
  { 450, 18, 1, 2, 1089, 4 },
  { 564, 18, 1, 2, 1089, 4 },
  { 672, 18, 1, 2, 1089, 4 },
  { 780, 18, 1, 2, 1089, 4 },
  { 888, 18, 1, 2, 1089, 4 },
  { 996, 18, 1, 2, 1089, 4 },
  { 1123, 18, 1, 2, 1089, 4 },
  { 88, 18, 1, 2, 1089, 4 },
  { 228, 18, 1, 2, 1089, 4 },
  { 118, 1, 72, 1, 993, 0 },
  { 258, 1, 72, 1, 993, 0 },
  { 372, 1, 72, 1, 993, 0 },
  { 486, 1, 72, 1, 993, 0 },
  { 594, 1, 72, 1, 993, 0 },
  { 702, 1, 72, 1, 993, 0 },
  { 810, 1, 72, 1, 993, 0 },
  { 918, 1, 72, 1, 993, 0 },
  { 1026, 1, 72, 1, 993, 0 },
  { 1153, 1, 72, 1, 993, 0 },
  { 5, 1, 72, 1, 993, 0 },
  { 145, 1, 72, 1, 993, 0 },
  { 285, 1, 72, 1, 993, 0 },
  { 399, 1, 72, 1, 993, 0 },
  { 513, 1, 72, 1, 993, 0 },
  { 621, 1, 72, 1, 993, 0 },
  { 729, 1, 72, 1, 993, 0 },
  { 837, 1, 72, 1, 993, 0 },
  { 945, 1, 72, 1, 993, 0 },
  { 1072, 1, 72, 1, 993, 0 },
  { 37, 1, 72, 1, 993, 0 },
  { 177, 1, 72, 1, 993, 0 },
  { 317, 1, 72, 1, 993, 0 },
  { 431, 1, 72, 1, 993, 0 },
  { 545, 1, 72, 1, 993, 0 },
  { 653, 1, 72, 1, 993, 0 },
  { 761, 1, 72, 1, 993, 0 },
  { 869, 1, 72, 1, 993, 0 },
  { 977, 1, 72, 1, 993, 0 },
  { 1104, 1, 72, 1, 993, 0 },
  { 69, 1, 72, 1, 993, 0 },
  { 209, 1, 72, 1, 993, 0 },
  { 122, 54, 1, 2, 929, 4 },
  { 262, 54, 1, 2, 929, 4 },
  { 376, 54, 1, 2, 929, 4 },
  { 490, 54, 1, 2, 929, 4 },
  { 598, 54, 1, 2, 929, 4 },
  { 706, 54, 1, 2, 929, 4 },
  { 814, 54, 1, 2, 929, 4 },
  { 922, 54, 1, 2, 929, 4 },
  { 1030, 54, 1, 2, 929, 4 },
  { 1157, 54, 1, 2, 929, 4 },
  { 10, 54, 1, 2, 929, 4 },
  { 150, 54, 1, 2, 929, 4 },
  { 290, 54, 1, 2, 929, 4 },
  { 404, 54, 1, 2, 929, 4 },
  { 518, 54, 1, 2, 929, 4 },
  { 626, 54, 1, 2, 929, 4 },
  { 734, 54, 1, 2, 929, 4 },
  { 842, 54, 1, 2, 929, 4 },
  { 950, 54, 1, 2, 929, 4 },
  { 1077, 54, 1, 2, 929, 4 },
  { 42, 54, 1, 2, 929, 4 },
  { 182, 54, 1, 2, 929, 4 },
  { 322, 54, 1, 2, 929, 4 },
  { 436, 54, 1, 2, 929, 4 },
  { 550, 54, 1, 2, 929, 4 },
  { 658, 54, 1, 2, 929, 4 },
  { 766, 54, 1, 2, 929, 4 },
  { 874, 54, 1, 2, 929, 4 },
  { 982, 54, 1, 2, 929, 4 },
  { 1109, 54, 1, 2, 929, 4 },
  { 74, 54, 1, 2, 929, 4 },
  { 214, 54, 1, 2, 929, 4 },
  { 344, 1, 1, 1, 961, 0 },
  { 458, 1, 1, 1, 961, 0 },
  { 572, 1, 1, 1, 961, 0 },
  { 680, 1, 1, 1, 961, 0 },
  { 788, 1, 1, 1, 961, 0 },
  { 896, 1, 1, 1, 961, 0 },
  { 1004, 1, 1, 1, 961, 0 },
  { 1131, 1, 1, 1, 961, 0 },
  { 96, 1, 1, 1, 961, 0 },
  { 236, 1, 1, 1, 961, 0 },
  { 350, 1, 1, 1, 961, 0 },
  { 464, 1, 1, 1, 961, 0 },
  { 578, 1, 1, 1, 961, 0 },
  { 686, 1, 1, 1, 961, 0 },
  { 794, 1, 1, 1, 961, 0 },
  { 902, 1, 1, 1, 961, 0 },
  { 1010, 1, 1, 1, 961, 0 },
  { 1137, 1, 1, 1, 961, 0 },
  { 102, 1, 1, 1, 961, 0 },
  { 242, 1, 1, 1, 961, 0 },
  { 356, 1, 1, 1, 961, 0 },
  { 470, 1, 1, 1, 961, 0 },
  { 584, 1, 1, 1, 961, 0 },
  { 692, 1, 1, 1, 961, 0 },
  { 800, 1, 1, 1, 961, 0 },
  { 908, 1, 1, 1, 961, 0 },
  { 1016, 1, 1, 1, 961, 0 },
  { 1143, 1, 1, 1, 961, 0 },
  { 108, 1, 1, 1, 961, 0 },
  { 248, 1, 1, 1, 961, 0 },
  { 362, 1, 1, 1, 961, 0 },
  { 476, 1, 1, 1, 961, 0 },
  { 137, 56, 1, 0, 833, 2 },
  { 277, 56, 1, 0, 833, 2 },
  { 391, 56, 1, 0, 833, 2 },
  { 505, 56, 1, 0, 833, 2 },
  { 613, 56, 1, 0, 833, 2 },
  { 721, 56, 1, 0, 833, 2 },
  { 829, 56, 1, 0, 833, 2 },
  { 937, 56, 1, 0, 833, 2 },
  { 1064, 56, 1, 0, 833, 2 },
  { 1171, 56, 1, 0, 833, 2 },
  { 28, 56, 1, 0, 833, 2 },
  { 168, 56, 1, 0, 833, 2 },
  { 308, 56, 1, 0, 833, 2 },
  { 422, 56, 1, 0, 833, 2 },
  { 536, 56, 1, 0, 833, 2 },
  { 644, 56, 1, 0, 833, 2 },
  { 752, 56, 1, 0, 833, 2 },
  { 860, 56, 1, 0, 833, 2 },
  { 968, 56, 1, 0, 833, 2 },
  { 1095, 56, 1, 0, 833, 2 },
  { 60, 56, 1, 0, 833, 2 },
  { 200, 56, 1, 0, 833, 2 },
  { 340, 56, 1, 0, 833, 2 },
  { 454, 56, 1, 0, 833, 2 },
  { 568, 56, 1, 0, 833, 2 },
  { 676, 56, 1, 0, 833, 2 },
  { 784, 56, 1, 0, 833, 2 },
  { 892, 56, 1, 0, 833, 2 },
  { 1000, 56, 1, 0, 833, 2 },
  { 1127, 56, 1, 0, 833, 2 },
  { 92, 56, 1, 0, 833, 2 },
  { 232, 56, 1, 0, 833, 2 },
  { 1035, 48, 1, 0, 643, 2 },
  { 1243, 1, 50, 1, 643, 0 },
  { 1249, 1, 50, 1, 612, 0 },
  { 1255, 1, 50, 1, 612, 0 },
  { 1261, 1, 50, 1, 612, 0 },
  { 1267, 1, 50, 1, 612, 0 },
  { 1273, 1, 50, 1, 612, 0 },
  { 1279, 1, 50, 1, 612, 0 },
  { 1285, 1, 50, 1, 612, 0 },
  { 1310, 1, 46, 1, 580, 0 },
  { 1316, 1, 46, 1, 580, 0 },
  { 1322, 1, 46, 1, 580, 0 },
  { 1328, 1, 46, 1, 580, 0 },
  { 1334, 1, 46, 1, 580, 0 },
  { 1340, 1, 46, 1, 580, 0 },
  { 1346, 1, 46, 1, 580, 0 },
  { 1352, 1, 46, 1, 580, 0 },
  { 1358, 1, 44, 1, 548, 0 },
  { 1364, 1, 44, 1, 548, 0 },
  { 1370, 1, 44, 1, 548, 0 },
  { 1376, 1, 44, 1, 548, 0 },
  { 1382, 1, 44, 1, 548, 0 },
  { 1388, 1, 44, 1, 548, 0 },
  { 1394, 1, 44, 1, 548, 0 },
  { 1400, 1, 44, 1, 548, 0 },
  { 1184, 1, 42, 1, 516, 0 },
  { 1190, 1, 42, 1, 516, 0 },
  { 1196, 1, 42, 1, 516, 0 },
  { 1202, 1, 42, 1, 516, 0 },
  { 1208, 1, 42, 1, 516, 0 },
  { 1214, 1, 42, 1, 516, 0 },
  { 1220, 1, 42, 1, 516, 0 },
  { 1226, 1, 42, 1, 516, 0 },
};

  // VSSRC Register Class...
  static const MCPhysReg VSSRC[] = {
    PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13, PPC_F31, PPC_F30, PPC_F29, PPC_F28, PPC_F27, PPC_F26, PPC_F25, PPC_F24, PPC_F23, PPC_F22, PPC_F21, PPC_F20, PPC_F19, PPC_F18, PPC_F17, PPC_F16, PPC_F15, PPC_F14, PPC_VF2, PPC_VF3, PPC_VF4, PPC_VF5, PPC_VF0, PPC_VF1, PPC_VF6, PPC_VF7, PPC_VF8, PPC_VF9, PPC_VF10, PPC_VF11, PPC_VF12, PPC_VF13, PPC_VF14, PPC_VF15, PPC_VF16, PPC_VF17, PPC_VF18, PPC_VF19, PPC_VF31, PPC_VF30, PPC_VF29, PPC_VF28, PPC_VF27, PPC_VF26, PPC_VF25, PPC_VF24, PPC_VF23, PPC_VF22, PPC_VF21, PPC_VF20,
  };
  // VSSRC Bit set.
  static const uint8_t VSSRCBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // GPRC Register Class...
  static const MCPhysReg GPRC[] = {
    PPC_R2, PPC_R3, PPC_R4, PPC_R5, PPC_R6, PPC_R7, PPC_R8, PPC_R9, PPC_R10, PPC_R11, PPC_R12, PPC_R30, PPC_R29, PPC_R28, PPC_R27, PPC_R26, PPC_R25, PPC_R24, PPC_R23, PPC_R22, PPC_R21, PPC_R20, PPC_R19, PPC_R18, PPC_R17, PPC_R16, PPC_R15, PPC_R14, PPC_R13, PPC_R31, PPC_R0, PPC_R1, PPC_FP, PPC_BP,
  };
  // GPRC Bit set.
  static const uint8_t GPRCBits[] = {
    0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // GPRC_NOR0 Register Class...
  static const MCPhysReg GPRC_NOR0[] = {
    PPC_R2, PPC_R3, PPC_R4, PPC_R5, PPC_R6, PPC_R7, PPC_R8, PPC_R9, PPC_R10, PPC_R11, PPC_R12, PPC_R30, PPC_R29, PPC_R28, PPC_R27, PPC_R26, PPC_R25, PPC_R24, PPC_R23, PPC_R22, PPC_R21, PPC_R20, PPC_R19, PPC_R18, PPC_R17, PPC_R16, PPC_R15, PPC_R14, PPC_R13, PPC_R31, PPC_R1, PPC_FP, PPC_BP, PPC_ZERO,
  };
  // GPRC_NOR0 Bit set.
  static const uint8_t GPRC_NOR0Bits[] = {
    0x12, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x7f,
  };
  // SPE4RC Register Class...
  static const MCPhysReg SPE4RC[] = {
    PPC_R2, PPC_R3, PPC_R4, PPC_R5, PPC_R6, PPC_R7, PPC_R8, PPC_R9, PPC_R10, PPC_R11, PPC_R12, PPC_R30, PPC_R29, PPC_R28, PPC_R27, PPC_R26, PPC_R25, PPC_R24, PPC_R23, PPC_R22, PPC_R21, PPC_R20, PPC_R19, PPC_R18, PPC_R17, PPC_R16, PPC_R15, PPC_R14, PPC_R13, PPC_R31, PPC_R0, PPC_R1, PPC_FP, PPC_BP,
  };
  // SPE4RC Bit set.
  static const uint8_t SPE4RCBits[] = {
    0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // GPRC_and_GPRC_NOR0 Register Class...
  static const MCPhysReg GPRC_and_GPRC_NOR0[] = {
    PPC_R2, PPC_R3, PPC_R4, PPC_R5, PPC_R6, PPC_R7, PPC_R8, PPC_R9, PPC_R10, PPC_R11, PPC_R12, PPC_R30, PPC_R29, PPC_R28, PPC_R27, PPC_R26, PPC_R25, PPC_R24, PPC_R23, PPC_R22, PPC_R21, PPC_R20, PPC_R19, PPC_R18, PPC_R17, PPC_R16, PPC_R15, PPC_R14, PPC_R13, PPC_R31, PPC_R1, PPC_FP, PPC_BP,
  };
  // GPRC_and_GPRC_NOR0 Bit set.
  static const uint8_t GPRC_and_GPRC_NOR0Bits[] = {
    0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x7f,
  };
  // CRBITRC Register Class...
  static const MCPhysReg CRBITRC[] = {
    PPC_CR2LT, PPC_CR2GT, PPC_CR2EQ, PPC_CR2UN, PPC_CR3LT, PPC_CR3GT, PPC_CR3EQ, PPC_CR3UN, PPC_CR4LT, PPC_CR4GT, PPC_CR4EQ, PPC_CR4UN, PPC_CR5LT, PPC_CR5GT, PPC_CR5EQ, PPC_CR5UN, PPC_CR6LT, PPC_CR6GT, PPC_CR6EQ, PPC_CR6UN, PPC_CR7LT, PPC_CR7GT, PPC_CR7EQ, PPC_CR7UN, PPC_CR1LT, PPC_CR1GT, PPC_CR1EQ, PPC_CR1UN, PPC_CR0LT, PPC_CR0GT, PPC_CR0EQ, PPC_CR0UN,
  };
  // CRBITRC Bit set.
  static const uint8_t CRBITRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  };
  // F4RC Register Class...
  static const MCPhysReg F4RC[] = {
    PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13, PPC_F31, PPC_F30, PPC_F29, PPC_F28, PPC_F27, PPC_F26, PPC_F25, PPC_F24, PPC_F23, PPC_F22, PPC_F21, PPC_F20, PPC_F19, PPC_F18, PPC_F17, PPC_F16, PPC_F15, PPC_F14,
  };
  // F4RC Bit set.
  static const uint8_t F4RCBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // CRRC Register Class...
  static const MCPhysReg CRRC[] = {
    PPC_CR0, PPC_CR1, PPC_CR5, PPC_CR6, PPC_CR7, PPC_CR2, PPC_CR3, PPC_CR4,
  };
  // CRRC Bit set.
  static const uint8_t CRRCBits[] = {
    0x00, 0xf0, 0x0f,
  };
  // CARRYRC Register Class...
  static const MCPhysReg CARRYRC[] = {
    PPC_CARRY, PPC_XER,
  };
  // CARRYRC Bit set.
  static const uint8_t CARRYRCBits[] = {
    0x04, 0x02,
  };
  // CTRRC Register Class...
  static const MCPhysReg CTRRC[] = {
    PPC_CTR,
  };
  // CTRRC Bit set.
  static const uint8_t CTRRCBits[] = {
    0x08,
  };
  // VRSAVERC Register Class...
  static const MCPhysReg VRSAVERC[] = {
    PPC_VRSAVE,
  };
  // VRSAVERC Bit set.
  static const uint8_t VRSAVERCBits[] = {
    0x00, 0x01,
  };
  // SPILLTOVSRRC Register Class...
  static const MCPhysReg SPILLTOVSRRC[] = {
    PPC_X2, PPC_X3, PPC_X4, PPC_X5, PPC_X6, PPC_X7, PPC_X8, PPC_X9, PPC_X10, PPC_X11, PPC_X12, PPC_X30, PPC_X29, PPC_X28, PPC_X27, PPC_X26, PPC_X25, PPC_X24, PPC_X23, PPC_X22, PPC_X21, PPC_X20, PPC_X19, PPC_X18, PPC_X17, PPC_X16, PPC_X15, PPC_X14, PPC_X31, PPC_X13, PPC_X0, PPC_X1, PPC_FP8, PPC_BP8, PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13, PPC_VF2, PPC_VF3, PPC_VF4, PPC_VF5, PPC_VF0, PPC_VF1, PPC_VF6, PPC_VF7, PPC_VF8, PPC_VF9, PPC_VF10, PPC_VF11, PPC_VF12, PPC_VF13, PPC_VF14, PPC_VF15, PPC_VF16, PPC_VF17, PPC_VF18, PPC_VF19,
  };
  // SPILLTOVSRRC Bit set.
  static const uint8_t SPILLTOVSRRCBits[] = {
    0x00, 0x08, 0xe0, 0xff, 0x07, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // VSFRC Register Class...
  static const MCPhysReg VSFRC[] = {
    PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13, PPC_F31, PPC_F30, PPC_F29, PPC_F28, PPC_F27, PPC_F26, PPC_F25, PPC_F24, PPC_F23, PPC_F22, PPC_F21, PPC_F20, PPC_F19, PPC_F18, PPC_F17, PPC_F16, PPC_F15, PPC_F14, PPC_VF2, PPC_VF3, PPC_VF4, PPC_VF5, PPC_VF0, PPC_VF1, PPC_VF6, PPC_VF7, PPC_VF8, PPC_VF9, PPC_VF10, PPC_VF11, PPC_VF12, PPC_VF13, PPC_VF14, PPC_VF15, PPC_VF16, PPC_VF17, PPC_VF18, PPC_VF19, PPC_VF31, PPC_VF30, PPC_VF29, PPC_VF28, PPC_VF27, PPC_VF26, PPC_VF25, PPC_VF24, PPC_VF23, PPC_VF22, PPC_VF21, PPC_VF20,
  };
  // VSFRC Bit set.
  static const uint8_t VSFRCBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // G8RC Register Class...
  static const MCPhysReg G8RC[] = {
    PPC_X2, PPC_X3, PPC_X4, PPC_X5, PPC_X6, PPC_X7, PPC_X8, PPC_X9, PPC_X10, PPC_X11, PPC_X12, PPC_X30, PPC_X29, PPC_X28, PPC_X27, PPC_X26, PPC_X25, PPC_X24, PPC_X23, PPC_X22, PPC_X21, PPC_X20, PPC_X19, PPC_X18, PPC_X17, PPC_X16, PPC_X15, PPC_X14, PPC_X31, PPC_X13, PPC_X0, PPC_X1, PPC_FP8, PPC_BP8,
  };
  // G8RC Bit set.
  static const uint8_t G8RCBits[] = {
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // G8RC_NOX0 Register Class...
  static const MCPhysReg G8RC_NOX0[] = {
    PPC_X2, PPC_X3, PPC_X4, PPC_X5, PPC_X6, PPC_X7, PPC_X8, PPC_X9, PPC_X10, PPC_X11, PPC_X12, PPC_X30, PPC_X29, PPC_X28, PPC_X27, PPC_X26, PPC_X25, PPC_X24, PPC_X23, PPC_X22, PPC_X21, PPC_X20, PPC_X19, PPC_X18, PPC_X17, PPC_X16, PPC_X15, PPC_X14, PPC_X31, PPC_X13, PPC_X1, PPC_FP8, PPC_BP8, PPC_ZERO8,
  };
  // G8RC_NOX0 Bit set.
  static const uint8_t G8RC_NOX0Bits[] = {
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  };
  // SPILLTOVSRRC_and_VSFRC Register Class...
  static const MCPhysReg SPILLTOVSRRC_and_VSFRC[] = {
    PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13, PPC_VF2, PPC_VF3, PPC_VF4, PPC_VF5, PPC_VF0, PPC_VF1, PPC_VF6, PPC_VF7, PPC_VF8, PPC_VF9, PPC_VF10, PPC_VF11, PPC_VF12, PPC_VF13, PPC_VF14, PPC_VF15, PPC_VF16, PPC_VF17, PPC_VF18, PPC_VF19,
  };
  // SPILLTOVSRRC_and_VSFRC Bit set.
  static const uint8_t SPILLTOVSRRC_and_VSFRCBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x07,
  };
  // G8RC_and_G8RC_NOX0 Register Class...
  static const MCPhysReg G8RC_and_G8RC_NOX0[] = {
    PPC_X2, PPC_X3, PPC_X4, PPC_X5, PPC_X6, PPC_X7, PPC_X8, PPC_X9, PPC_X10, PPC_X11, PPC_X12, PPC_X30, PPC_X29, PPC_X28, PPC_X27, PPC_X26, PPC_X25, PPC_X24, PPC_X23, PPC_X22, PPC_X21, PPC_X20, PPC_X19, PPC_X18, PPC_X17, PPC_X16, PPC_X15, PPC_X14, PPC_X31, PPC_X13, PPC_X1, PPC_FP8, PPC_BP8,
  };
  // G8RC_and_G8RC_NOX0 Bit set.
  static const uint8_t G8RC_and_G8RC_NOX0Bits[] = {
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x7f,
  };
  // F8RC Register Class...
  static const MCPhysReg F8RC[] = {
    PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13, PPC_F31, PPC_F30, PPC_F29, PPC_F28, PPC_F27, PPC_F26, PPC_F25, PPC_F24, PPC_F23, PPC_F22, PPC_F21, PPC_F20, PPC_F19, PPC_F18, PPC_F17, PPC_F16, PPC_F15, PPC_F14,
  };
  // F8RC Bit set.
  static const uint8_t F8RCBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0xff, 0xff, 0x1f,
  };
  // SPERC Register Class...
  static const MCPhysReg SPERC[] = {
    PPC_S2, PPC_S3, PPC_S4, PPC_S5, PPC_S6, PPC_S7, PPC_S8, PPC_S9, PPC_S10, PPC_S11, PPC_S12, PPC_S30, PPC_S29, PPC_S28, PPC_S27, PPC_S26, PPC_S25, PPC_S24, PPC_S23, PPC_S22, PPC_S21, PPC_S20, PPC_S19, PPC_S18, PPC_S17, PPC_S16, PPC_S15, PPC_S14, PPC_S13, PPC_S31, PPC_S0, PPC_S1,
  };
  // SPERC Bit set.
  static const uint8_t SPERCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // VFRC Register Class...
  static const MCPhysReg VFRC[] = {
    PPC_VF2, PPC_VF3, PPC_VF4, PPC_VF5, PPC_VF0, PPC_VF1, PPC_VF6, PPC_VF7, PPC_VF8, PPC_VF9, PPC_VF10, PPC_VF11, PPC_VF12, PPC_VF13, PPC_VF14, PPC_VF15, PPC_VF16, PPC_VF17, PPC_VF18, PPC_VF19, PPC_VF31, PPC_VF30, PPC_VF29, PPC_VF28, PPC_VF27, PPC_VF26, PPC_VF25, PPC_VF24, PPC_VF23, PPC_VF22, PPC_VF21, PPC_VF20,
  };
  // VFRC Bit set.
  static const uint8_t VFRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // SPERC_with_sub_32_in_GPRC_NOR0 Register Class...
  static const MCPhysReg SPERC_with_sub_32_in_GPRC_NOR0[] = {
    PPC_S2, PPC_S3, PPC_S4, PPC_S5, PPC_S6, PPC_S7, PPC_S8, PPC_S9, PPC_S10, PPC_S11, PPC_S12, PPC_S30, PPC_S29, PPC_S28, PPC_S27, PPC_S26, PPC_S25, PPC_S24, PPC_S23, PPC_S22, PPC_S21, PPC_S20, PPC_S19, PPC_S18, PPC_S17, PPC_S16, PPC_S15, PPC_S14, PPC_S13, PPC_S31, PPC_S1,
  };
  // SPERC_with_sub_32_in_GPRC_NOR0 Bit set.
  static const uint8_t SPERC_with_sub_32_in_GPRC_NOR0Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x7f,
  };
  // SPILLTOVSRRC_and_VFRC Register Class...
  static const MCPhysReg SPILLTOVSRRC_and_VFRC[] = {
    PPC_VF2, PPC_VF3, PPC_VF4, PPC_VF5, PPC_VF0, PPC_VF1, PPC_VF6, PPC_VF7, PPC_VF8, PPC_VF9, PPC_VF10, PPC_VF11, PPC_VF12, PPC_VF13, PPC_VF14, PPC_VF15, PPC_VF16, PPC_VF17, PPC_VF18, PPC_VF19,
  };
  // SPILLTOVSRRC_and_VFRC Bit set.
  static const uint8_t SPILLTOVSRRC_and_VFRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x07,
  };
  // SPILLTOVSRRC_and_F4RC Register Class...
  static const MCPhysReg SPILLTOVSRRC_and_F4RC[] = {
    PPC_F0, PPC_F1, PPC_F2, PPC_F3, PPC_F4, PPC_F5, PPC_F6, PPC_F7, PPC_F8, PPC_F9, PPC_F10, PPC_F11, PPC_F12, PPC_F13,
  };
  // SPILLTOVSRRC_and_F4RC Bit set.
  static const uint8_t SPILLTOVSRRC_and_F4RCBits[] = {
    0x00, 0x00, 0xe0, 0xff, 0x07,
  };
  // CTRRC8 Register Class...
  static const MCPhysReg CTRRC8[] = {
    PPC_CTR8,
  };
  // CTRRC8 Bit set.
  static const uint8_t CTRRC8Bits[] = {
    0x00, 0x00, 0x10,
  };
  // VSRC Register Class...
  static const MCPhysReg VSRC[] = {
    PPC_VSL0, PPC_VSL1, PPC_VSL2, PPC_VSL3, PPC_VSL4, PPC_VSL5, PPC_VSL6, PPC_VSL7, PPC_VSL8, PPC_VSL9, PPC_VSL10, PPC_VSL11, PPC_VSL12, PPC_VSL13, PPC_VSL31, PPC_VSL30, PPC_VSL29, PPC_VSL28, PPC_VSL27, PPC_VSL26, PPC_VSL25, PPC_VSL24, PPC_VSL23, PPC_VSL22, PPC_VSL21, PPC_VSL20, PPC_VSL19, PPC_VSL18, PPC_VSL17, PPC_VSL16, PPC_VSL15, PPC_VSL14, PPC_V2, PPC_V3, PPC_V4, PPC_V5, PPC_V0, PPC_V1, PPC_V6, PPC_V7, PPC_V8, PPC_V9, PPC_V10, PPC_V11, PPC_V12, PPC_V13, PPC_V14, PPC_V15, PPC_V16, PPC_V17, PPC_V18, PPC_V19, PPC_V31, PPC_V30, PPC_V29, PPC_V28, PPC_V27, PPC_V26, PPC_V25, PPC_V24, PPC_V23, PPC_V22, PPC_V21, PPC_V20,
  };
  // VSRC Bit set.
  static const uint8_t VSRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // VSRC_with_sub_64_in_SPILLTOVSRRC Register Class...
  static const MCPhysReg VSRC_with_sub_64_in_SPILLTOVSRRC[] = {
    PPC_VSL0, PPC_VSL1, PPC_VSL2, PPC_VSL3, PPC_VSL4, PPC_VSL5, PPC_VSL6, PPC_VSL7, PPC_VSL8, PPC_VSL9, PPC_VSL10, PPC_VSL11, PPC_VSL12, PPC_VSL13, PPC_V2, PPC_V3, PPC_V4, PPC_V5, PPC_V0, PPC_V1, PPC_V6, PPC_V7, PPC_V8, PPC_V9, PPC_V10, PPC_V11, PPC_V12, PPC_V13, PPC_V14, PPC_V15, PPC_V16, PPC_V17, PPC_V18, PPC_V19,
  };
  // VSRC_with_sub_64_in_SPILLTOVSRRC Bit set.
  static const uint8_t VSRC_with_sub_64_in_SPILLTOVSRRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x07, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x1f,
  };
  // QSRC Register Class...
  static const MCPhysReg QSRC[] = {
    PPC_QF0, PPC_QF1, PPC_QF2, PPC_QF3, PPC_QF4, PPC_QF5, PPC_QF6, PPC_QF7, PPC_QF8, PPC_QF9, PPC_QF10, PPC_QF11, PPC_QF12, PPC_QF13, PPC_QF31, PPC_QF30, PPC_QF29, PPC_QF28, PPC_QF27, PPC_QF26, PPC_QF25, PPC_QF24, PPC_QF23, PPC_QF22, PPC_QF21, PPC_QF20, PPC_QF19, PPC_QF18, PPC_QF17, PPC_QF16, PPC_QF15, PPC_QF14,
  };
  // QSRC Bit set.
  static const uint8_t QSRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // VRRC Register Class...
  static const MCPhysReg VRRC[] = {
    PPC_V2, PPC_V3, PPC_V4, PPC_V5, PPC_V0, PPC_V1, PPC_V6, PPC_V7, PPC_V8, PPC_V9, PPC_V10, PPC_V11, PPC_V12, PPC_V13, PPC_V14, PPC_V15, PPC_V16, PPC_V17, PPC_V18, PPC_V19, PPC_V31, PPC_V30, PPC_V29, PPC_V28, PPC_V27, PPC_V26, PPC_V25, PPC_V24, PPC_V23, PPC_V22, PPC_V21, PPC_V20,
  };
  // VRRC Bit set.
  static const uint8_t VRRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // VSLRC Register Class...
  static const MCPhysReg VSLRC[] = {
    PPC_VSL0, PPC_VSL1, PPC_VSL2, PPC_VSL3, PPC_VSL4, PPC_VSL5, PPC_VSL6, PPC_VSL7, PPC_VSL8, PPC_VSL9, PPC_VSL10, PPC_VSL11, PPC_VSL12, PPC_VSL13, PPC_VSL31, PPC_VSL30, PPC_VSL29, PPC_VSL28, PPC_VSL27, PPC_VSL26, PPC_VSL25, PPC_VSL24, PPC_VSL23, PPC_VSL22, PPC_VSL21, PPC_VSL20, PPC_VSL19, PPC_VSL18, PPC_VSL17, PPC_VSL16, PPC_VSL15, PPC_VSL14,
  };
  // VSLRC Bit set.
  static const uint8_t VSLRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // VRRC_with_sub_64_in_SPILLTOVSRRC Register Class...
  static const MCPhysReg VRRC_with_sub_64_in_SPILLTOVSRRC[] = {
    PPC_V2, PPC_V3, PPC_V4, PPC_V5, PPC_V0, PPC_V1, PPC_V6, PPC_V7, PPC_V8, PPC_V9, PPC_V10, PPC_V11, PPC_V12, PPC_V13, PPC_V14, PPC_V15, PPC_V16, PPC_V17, PPC_V18, PPC_V19,
  };
  // VRRC_with_sub_64_in_SPILLTOVSRRC Bit set.
  static const uint8_t VRRC_with_sub_64_in_SPILLTOVSRRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0x07,
  };
  // QSRC_with_sub_64_in_SPILLTOVSRRC Register Class...
  static const MCPhysReg QSRC_with_sub_64_in_SPILLTOVSRRC[] = {
    PPC_QF0, PPC_QF1, PPC_QF2, PPC_QF3, PPC_QF4, PPC_QF5, PPC_QF6, PPC_QF7, PPC_QF8, PPC_QF9, PPC_QF10, PPC_QF11, PPC_QF12, PPC_QF13,
  };
  // QSRC_with_sub_64_in_SPILLTOVSRRC Bit set.
  static const uint8_t QSRC_with_sub_64_in_SPILLTOVSRRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x1f,
  };
  // VSLRC_with_sub_64_in_SPILLTOVSRRC Register Class...
  static const MCPhysReg VSLRC_with_sub_64_in_SPILLTOVSRRC[] = {
    PPC_VSL0, PPC_VSL1, PPC_VSL2, PPC_VSL3, PPC_VSL4, PPC_VSL5, PPC_VSL6, PPC_VSL7, PPC_VSL8, PPC_VSL9, PPC_VSL10, PPC_VSL11, PPC_VSL12, PPC_VSL13,
  };
  // VSLRC_with_sub_64_in_SPILLTOVSRRC Bit set.
  static const uint8_t VSLRC_with_sub_64_in_SPILLTOVSRRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x1f,
  };
  // QBRC Register Class...
  static const MCPhysReg QBRC[] = {
    PPC_QF0, PPC_QF1, PPC_QF2, PPC_QF3, PPC_QF4, PPC_QF5, PPC_QF6, PPC_QF7, PPC_QF8, PPC_QF9, PPC_QF10, PPC_QF11, PPC_QF12, PPC_QF13, PPC_QF31, PPC_QF30, PPC_QF29, PPC_QF28, PPC_QF27, PPC_QF26, PPC_QF25, PPC_QF24, PPC_QF23, PPC_QF22, PPC_QF21, PPC_QF20, PPC_QF19, PPC_QF18, PPC_QF17, PPC_QF16, PPC_QF15, PPC_QF14,
  };
  // QBRC Bit set.
  static const uint8_t QBRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // QFRC Register Class...
  static const MCPhysReg QFRC[] = {
    PPC_QF0, PPC_QF1, PPC_QF2, PPC_QF3, PPC_QF4, PPC_QF5, PPC_QF6, PPC_QF7, PPC_QF8, PPC_QF9, PPC_QF10, PPC_QF11, PPC_QF12, PPC_QF13, PPC_QF31, PPC_QF30, PPC_QF29, PPC_QF28, PPC_QF27, PPC_QF26, PPC_QF25, PPC_QF24, PPC_QF23, PPC_QF22, PPC_QF21, PPC_QF20, PPC_QF19, PPC_QF18, PPC_QF17, PPC_QF16, PPC_QF15, PPC_QF14,
  };
  // QFRC Bit set.
  static const uint8_t QFRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // QBRC_with_sub_64_in_SPILLTOVSRRC Register Class...
  static const MCPhysReg QBRC_with_sub_64_in_SPILLTOVSRRC[] = {
    PPC_QF0, PPC_QF1, PPC_QF2, PPC_QF3, PPC_QF4, PPC_QF5, PPC_QF6, PPC_QF7, PPC_QF8, PPC_QF9, PPC_QF10, PPC_QF11, PPC_QF12, PPC_QF13,
  };
  // QBRC_with_sub_64_in_SPILLTOVSRRC Bit set.
  static const uint8_t QBRC_with_sub_64_in_SPILLTOVSRRCBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x1f,
  };


static const MCRegisterClass PPCMCRegisterClasses[] = {
  { VSSRC, VSSRCBits, sizeof(VSSRCBits) },
  { GPRC, GPRCBits, sizeof(GPRCBits) },
  { GPRC_NOR0, GPRC_NOR0Bits, sizeof(GPRC_NOR0Bits) },
  { SPE4RC, SPE4RCBits, sizeof(SPE4RCBits) },
  { GPRC_and_GPRC_NOR0, GPRC_and_GPRC_NOR0Bits, sizeof(GPRC_and_GPRC_NOR0Bits) },
  { CRBITRC, CRBITRCBits, sizeof(CRBITRCBits) },
  { F4RC, F4RCBits, sizeof(F4RCBits) },
  { CRRC, CRRCBits, sizeof(CRRCBits) },
  { CARRYRC, CARRYRCBits, sizeof(CARRYRCBits) },
  { CTRRC, CTRRCBits, sizeof(CTRRCBits) },
  { VRSAVERC, VRSAVERCBits, sizeof(VRSAVERCBits) },
  { SPILLTOVSRRC, SPILLTOVSRRCBits, sizeof(SPILLTOVSRRCBits) },
  { VSFRC, VSFRCBits, sizeof(VSFRCBits) },
  { G8RC, G8RCBits, sizeof(G8RCBits) },
  { G8RC_NOX0, G8RC_NOX0Bits, sizeof(G8RC_NOX0Bits) },
  { SPILLTOVSRRC_and_VSFRC, SPILLTOVSRRC_and_VSFRCBits, sizeof(SPILLTOVSRRC_and_VSFRCBits) },
  { G8RC_and_G8RC_NOX0, G8RC_and_G8RC_NOX0Bits, sizeof(G8RC_and_G8RC_NOX0Bits) },
  { F8RC, F8RCBits, sizeof(F8RCBits) },
  { SPERC, SPERCBits, sizeof(SPERCBits) },
  { VFRC, VFRCBits, sizeof(VFRCBits) },
  { SPERC_with_sub_32_in_GPRC_NOR0, SPERC_with_sub_32_in_GPRC_NOR0Bits, sizeof(SPERC_with_sub_32_in_GPRC_NOR0Bits) },
  { SPILLTOVSRRC_and_VFRC, SPILLTOVSRRC_and_VFRCBits, sizeof(SPILLTOVSRRC_and_VFRCBits) },
  { SPILLTOVSRRC_and_F4RC, SPILLTOVSRRC_and_F4RCBits, sizeof(SPILLTOVSRRC_and_F4RCBits) },
  { CTRRC8, CTRRC8Bits, sizeof(CTRRC8Bits) },
  { VSRC, VSRCBits, sizeof(VSRCBits) },
  { VSRC_with_sub_64_in_SPILLTOVSRRC, VSRC_with_sub_64_in_SPILLTOVSRRCBits, sizeof(VSRC_with_sub_64_in_SPILLTOVSRRCBits) },
  { QSRC, QSRCBits, sizeof(QSRCBits) },
  { VRRC, VRRCBits, sizeof(VRRCBits) },
  { VSLRC, VSLRCBits, sizeof(VSLRCBits) },
  { VRRC_with_sub_64_in_SPILLTOVSRRC, VRRC_with_sub_64_in_SPILLTOVSRRCBits, sizeof(VRRC_with_sub_64_in_SPILLTOVSRRCBits) },
  { QSRC_with_sub_64_in_SPILLTOVSRRC, QSRC_with_sub_64_in_SPILLTOVSRRCBits, sizeof(QSRC_with_sub_64_in_SPILLTOVSRRCBits) },
  { VSLRC_with_sub_64_in_SPILLTOVSRRC, VSLRC_with_sub_64_in_SPILLTOVSRRCBits, sizeof(VSLRC_with_sub_64_in_SPILLTOVSRRCBits) },
  { QBRC, QBRCBits, sizeof(QBRCBits) },
  { QFRC, QFRCBits, sizeof(QFRCBits) },
  { QBRC_with_sub_64_in_SPILLTOVSRRC, QBRC_with_sub_64_in_SPILLTOVSRRCBits, sizeof(QBRC_with_sub_64_in_SPILLTOVSRRCBits) },
};

#endif // GET_REGINFO_MC_DESC

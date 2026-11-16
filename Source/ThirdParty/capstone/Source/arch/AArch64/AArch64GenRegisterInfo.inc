
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
  AArch64_NoRegister,
  AArch64_FFR = 1,
  AArch64_FP = 2,
  AArch64_LR = 3,
  AArch64_NZCV = 4,
  AArch64_SP = 5,
  AArch64_VG = 6,
  AArch64_WSP = 7,
  AArch64_WZR = 8,
  AArch64_XZR = 9,
  AArch64_ZA = 10,
  AArch64_B0 = 11,
  AArch64_B1 = 12,
  AArch64_B2 = 13,
  AArch64_B3 = 14,
  AArch64_B4 = 15,
  AArch64_B5 = 16,
  AArch64_B6 = 17,
  AArch64_B7 = 18,
  AArch64_B8 = 19,
  AArch64_B9 = 20,
  AArch64_B10 = 21,
  AArch64_B11 = 22,
  AArch64_B12 = 23,
  AArch64_B13 = 24,
  AArch64_B14 = 25,
  AArch64_B15 = 26,
  AArch64_B16 = 27,
  AArch64_B17 = 28,
  AArch64_B18 = 29,
  AArch64_B19 = 30,
  AArch64_B20 = 31,
  AArch64_B21 = 32,
  AArch64_B22 = 33,
  AArch64_B23 = 34,
  AArch64_B24 = 35,
  AArch64_B25 = 36,
  AArch64_B26 = 37,
  AArch64_B27 = 38,
  AArch64_B28 = 39,
  AArch64_B29 = 40,
  AArch64_B30 = 41,
  AArch64_B31 = 42,
  AArch64_D0 = 43,
  AArch64_D1 = 44,
  AArch64_D2 = 45,
  AArch64_D3 = 46,
  AArch64_D4 = 47,
  AArch64_D5 = 48,
  AArch64_D6 = 49,
  AArch64_D7 = 50,
  AArch64_D8 = 51,
  AArch64_D9 = 52,
  AArch64_D10 = 53,
  AArch64_D11 = 54,
  AArch64_D12 = 55,
  AArch64_D13 = 56,
  AArch64_D14 = 57,
  AArch64_D15 = 58,
  AArch64_D16 = 59,
  AArch64_D17 = 60,
  AArch64_D18 = 61,
  AArch64_D19 = 62,
  AArch64_D20 = 63,
  AArch64_D21 = 64,
  AArch64_D22 = 65,
  AArch64_D23 = 66,
  AArch64_D24 = 67,
  AArch64_D25 = 68,
  AArch64_D26 = 69,
  AArch64_D27 = 70,
  AArch64_D28 = 71,
  AArch64_D29 = 72,
  AArch64_D30 = 73,
  AArch64_D31 = 74,
  AArch64_H0 = 75,
  AArch64_H1 = 76,
  AArch64_H2 = 77,
  AArch64_H3 = 78,
  AArch64_H4 = 79,
  AArch64_H5 = 80,
  AArch64_H6 = 81,
  AArch64_H7 = 82,
  AArch64_H8 = 83,
  AArch64_H9 = 84,
  AArch64_H10 = 85,
  AArch64_H11 = 86,
  AArch64_H12 = 87,
  AArch64_H13 = 88,
  AArch64_H14 = 89,
  AArch64_H15 = 90,
  AArch64_H16 = 91,
  AArch64_H17 = 92,
  AArch64_H18 = 93,
  AArch64_H19 = 94,
  AArch64_H20 = 95,
  AArch64_H21 = 96,
  AArch64_H22 = 97,
  AArch64_H23 = 98,
  AArch64_H24 = 99,
  AArch64_H25 = 100,
  AArch64_H26 = 101,
  AArch64_H27 = 102,
  AArch64_H28 = 103,
  AArch64_H29 = 104,
  AArch64_H30 = 105,
  AArch64_H31 = 106,
  AArch64_P0 = 107,
  AArch64_P1 = 108,
  AArch64_P2 = 109,
  AArch64_P3 = 110,
  AArch64_P4 = 111,
  AArch64_P5 = 112,
  AArch64_P6 = 113,
  AArch64_P7 = 114,
  AArch64_P8 = 115,
  AArch64_P9 = 116,
  AArch64_P10 = 117,
  AArch64_P11 = 118,
  AArch64_P12 = 119,
  AArch64_P13 = 120,
  AArch64_P14 = 121,
  AArch64_P15 = 122,
  AArch64_Q0 = 123,
  AArch64_Q1 = 124,
  AArch64_Q2 = 125,
  AArch64_Q3 = 126,
  AArch64_Q4 = 127,
  AArch64_Q5 = 128,
  AArch64_Q6 = 129,
  AArch64_Q7 = 130,
  AArch64_Q8 = 131,
  AArch64_Q9 = 132,
  AArch64_Q10 = 133,
  AArch64_Q11 = 134,
  AArch64_Q12 = 135,
  AArch64_Q13 = 136,
  AArch64_Q14 = 137,
  AArch64_Q15 = 138,
  AArch64_Q16 = 139,
  AArch64_Q17 = 140,
  AArch64_Q18 = 141,
  AArch64_Q19 = 142,
  AArch64_Q20 = 143,
  AArch64_Q21 = 144,
  AArch64_Q22 = 145,
  AArch64_Q23 = 146,
  AArch64_Q24 = 147,
  AArch64_Q25 = 148,
  AArch64_Q26 = 149,
  AArch64_Q27 = 150,
  AArch64_Q28 = 151,
  AArch64_Q29 = 152,
  AArch64_Q30 = 153,
  AArch64_Q31 = 154,
  AArch64_S0 = 155,
  AArch64_S1 = 156,
  AArch64_S2 = 157,
  AArch64_S3 = 158,
  AArch64_S4 = 159,
  AArch64_S5 = 160,
  AArch64_S6 = 161,
  AArch64_S7 = 162,
  AArch64_S8 = 163,
  AArch64_S9 = 164,
  AArch64_S10 = 165,
  AArch64_S11 = 166,
  AArch64_S12 = 167,
  AArch64_S13 = 168,
  AArch64_S14 = 169,
  AArch64_S15 = 170,
  AArch64_S16 = 171,
  AArch64_S17 = 172,
  AArch64_S18 = 173,
  AArch64_S19 = 174,
  AArch64_S20 = 175,
  AArch64_S21 = 176,
  AArch64_S22 = 177,
  AArch64_S23 = 178,
  AArch64_S24 = 179,
  AArch64_S25 = 180,
  AArch64_S26 = 181,
  AArch64_S27 = 182,
  AArch64_S28 = 183,
  AArch64_S29 = 184,
  AArch64_S30 = 185,
  AArch64_S31 = 186,
  AArch64_W0 = 187,
  AArch64_W1 = 188,
  AArch64_W2 = 189,
  AArch64_W3 = 190,
  AArch64_W4 = 191,
  AArch64_W5 = 192,
  AArch64_W6 = 193,
  AArch64_W7 = 194,
  AArch64_W8 = 195,
  AArch64_W9 = 196,
  AArch64_W10 = 197,
  AArch64_W11 = 198,
  AArch64_W12 = 199,
  AArch64_W13 = 200,
  AArch64_W14 = 201,
  AArch64_W15 = 202,
  AArch64_W16 = 203,
  AArch64_W17 = 204,
  AArch64_W18 = 205,
  AArch64_W19 = 206,
  AArch64_W20 = 207,
  AArch64_W21 = 208,
  AArch64_W22 = 209,
  AArch64_W23 = 210,
  AArch64_W24 = 211,
  AArch64_W25 = 212,
  AArch64_W26 = 213,
  AArch64_W27 = 214,
  AArch64_W28 = 215,
  AArch64_W29 = 216,
  AArch64_W30 = 217,
  AArch64_X0 = 218,
  AArch64_X1 = 219,
  AArch64_X2 = 220,
  AArch64_X3 = 221,
  AArch64_X4 = 222,
  AArch64_X5 = 223,
  AArch64_X6 = 224,
  AArch64_X7 = 225,
  AArch64_X8 = 226,
  AArch64_X9 = 227,
  AArch64_X10 = 228,
  AArch64_X11 = 229,
  AArch64_X12 = 230,
  AArch64_X13 = 231,
  AArch64_X14 = 232,
  AArch64_X15 = 233,
  AArch64_X16 = 234,
  AArch64_X17 = 235,
  AArch64_X18 = 236,
  AArch64_X19 = 237,
  AArch64_X20 = 238,
  AArch64_X21 = 239,
  AArch64_X22 = 240,
  AArch64_X23 = 241,
  AArch64_X24 = 242,
  AArch64_X25 = 243,
  AArch64_X26 = 244,
  AArch64_X27 = 245,
  AArch64_X28 = 246,
  AArch64_Z0 = 247,
  AArch64_Z1 = 248,
  AArch64_Z2 = 249,
  AArch64_Z3 = 250,
  AArch64_Z4 = 251,
  AArch64_Z5 = 252,
  AArch64_Z6 = 253,
  AArch64_Z7 = 254,
  AArch64_Z8 = 255,
  AArch64_Z9 = 256,
  AArch64_Z10 = 257,
  AArch64_Z11 = 258,
  AArch64_Z12 = 259,
  AArch64_Z13 = 260,
  AArch64_Z14 = 261,
  AArch64_Z15 = 262,
  AArch64_Z16 = 263,
  AArch64_Z17 = 264,
  AArch64_Z18 = 265,
  AArch64_Z19 = 266,
  AArch64_Z20 = 267,
  AArch64_Z21 = 268,
  AArch64_Z22 = 269,
  AArch64_Z23 = 270,
  AArch64_Z24 = 271,
  AArch64_Z25 = 272,
  AArch64_Z26 = 273,
  AArch64_Z27 = 274,
  AArch64_Z28 = 275,
  AArch64_Z29 = 276,
  AArch64_Z30 = 277,
  AArch64_Z31 = 278,
  AArch64_ZAB0 = 279,
  AArch64_ZAD0 = 280,
  AArch64_ZAD1 = 281,
  AArch64_ZAD2 = 282,
  AArch64_ZAD3 = 283,
  AArch64_ZAD4 = 284,
  AArch64_ZAD5 = 285,
  AArch64_ZAD6 = 286,
  AArch64_ZAD7 = 287,
  AArch64_ZAH0 = 288,
  AArch64_ZAH1 = 289,
  AArch64_ZAQ0 = 290,
  AArch64_ZAQ1 = 291,
  AArch64_ZAQ2 = 292,
  AArch64_ZAQ3 = 293,
  AArch64_ZAQ4 = 294,
  AArch64_ZAQ5 = 295,
  AArch64_ZAQ6 = 296,
  AArch64_ZAQ7 = 297,
  AArch64_ZAQ8 = 298,
  AArch64_ZAQ9 = 299,
  AArch64_ZAQ10 = 300,
  AArch64_ZAQ11 = 301,
  AArch64_ZAQ12 = 302,
  AArch64_ZAQ13 = 303,
  AArch64_ZAQ14 = 304,
  AArch64_ZAQ15 = 305,
  AArch64_ZAS0 = 306,
  AArch64_ZAS1 = 307,
  AArch64_ZAS2 = 308,
  AArch64_ZAS3 = 309,
  AArch64_Z0_HI = 310,
  AArch64_Z1_HI = 311,
  AArch64_Z2_HI = 312,
  AArch64_Z3_HI = 313,
  AArch64_Z4_HI = 314,
  AArch64_Z5_HI = 315,
  AArch64_Z6_HI = 316,
  AArch64_Z7_HI = 317,
  AArch64_Z8_HI = 318,
  AArch64_Z9_HI = 319,
  AArch64_Z10_HI = 320,
  AArch64_Z11_HI = 321,
  AArch64_Z12_HI = 322,
  AArch64_Z13_HI = 323,
  AArch64_Z14_HI = 324,
  AArch64_Z15_HI = 325,
  AArch64_Z16_HI = 326,
  AArch64_Z17_HI = 327,
  AArch64_Z18_HI = 328,
  AArch64_Z19_HI = 329,
  AArch64_Z20_HI = 330,
  AArch64_Z21_HI = 331,
  AArch64_Z22_HI = 332,
  AArch64_Z23_HI = 333,
  AArch64_Z24_HI = 334,
  AArch64_Z25_HI = 335,
  AArch64_Z26_HI = 336,
  AArch64_Z27_HI = 337,
  AArch64_Z28_HI = 338,
  AArch64_Z29_HI = 339,
  AArch64_Z30_HI = 340,
  AArch64_Z31_HI = 341,
  AArch64_D0_D1 = 342,
  AArch64_D1_D2 = 343,
  AArch64_D2_D3 = 344,
  AArch64_D3_D4 = 345,
  AArch64_D4_D5 = 346,
  AArch64_D5_D6 = 347,
  AArch64_D6_D7 = 348,
  AArch64_D7_D8 = 349,
  AArch64_D8_D9 = 350,
  AArch64_D9_D10 = 351,
  AArch64_D10_D11 = 352,
  AArch64_D11_D12 = 353,
  AArch64_D12_D13 = 354,
  AArch64_D13_D14 = 355,
  AArch64_D14_D15 = 356,
  AArch64_D15_D16 = 357,
  AArch64_D16_D17 = 358,
  AArch64_D17_D18 = 359,
  AArch64_D18_D19 = 360,
  AArch64_D19_D20 = 361,
  AArch64_D20_D21 = 362,
  AArch64_D21_D22 = 363,
  AArch64_D22_D23 = 364,
  AArch64_D23_D24 = 365,
  AArch64_D24_D25 = 366,
  AArch64_D25_D26 = 367,
  AArch64_D26_D27 = 368,
  AArch64_D27_D28 = 369,
  AArch64_D28_D29 = 370,
  AArch64_D29_D30 = 371,
  AArch64_D30_D31 = 372,
  AArch64_D31_D0 = 373,
  AArch64_D0_D1_D2_D3 = 374,
  AArch64_D1_D2_D3_D4 = 375,
  AArch64_D2_D3_D4_D5 = 376,
  AArch64_D3_D4_D5_D6 = 377,
  AArch64_D4_D5_D6_D7 = 378,
  AArch64_D5_D6_D7_D8 = 379,
  AArch64_D6_D7_D8_D9 = 380,
  AArch64_D7_D8_D9_D10 = 381,
  AArch64_D8_D9_D10_D11 = 382,
  AArch64_D9_D10_D11_D12 = 383,
  AArch64_D10_D11_D12_D13 = 384,
  AArch64_D11_D12_D13_D14 = 385,
  AArch64_D12_D13_D14_D15 = 386,
  AArch64_D13_D14_D15_D16 = 387,
  AArch64_D14_D15_D16_D17 = 388,
  AArch64_D15_D16_D17_D18 = 389,
  AArch64_D16_D17_D18_D19 = 390,
  AArch64_D17_D18_D19_D20 = 391,
  AArch64_D18_D19_D20_D21 = 392,
  AArch64_D19_D20_D21_D22 = 393,
  AArch64_D20_D21_D22_D23 = 394,
  AArch64_D21_D22_D23_D24 = 395,
  AArch64_D22_D23_D24_D25 = 396,
  AArch64_D23_D24_D25_D26 = 397,
  AArch64_D24_D25_D26_D27 = 398,
  AArch64_D25_D26_D27_D28 = 399,
  AArch64_D26_D27_D28_D29 = 400,
  AArch64_D27_D28_D29_D30 = 401,
  AArch64_D28_D29_D30_D31 = 402,
  AArch64_D29_D30_D31_D0 = 403,
  AArch64_D30_D31_D0_D1 = 404,
  AArch64_D31_D0_D1_D2 = 405,
  AArch64_D0_D1_D2 = 406,
  AArch64_D1_D2_D3 = 407,
  AArch64_D2_D3_D4 = 408,
  AArch64_D3_D4_D5 = 409,
  AArch64_D4_D5_D6 = 410,
  AArch64_D5_D6_D7 = 411,
  AArch64_D6_D7_D8 = 412,
  AArch64_D7_D8_D9 = 413,
  AArch64_D8_D9_D10 = 414,
  AArch64_D9_D10_D11 = 415,
  AArch64_D10_D11_D12 = 416,
  AArch64_D11_D12_D13 = 417,
  AArch64_D12_D13_D14 = 418,
  AArch64_D13_D14_D15 = 419,
  AArch64_D14_D15_D16 = 420,
  AArch64_D15_D16_D17 = 421,
  AArch64_D16_D17_D18 = 422,
  AArch64_D17_D18_D19 = 423,
  AArch64_D18_D19_D20 = 424,
  AArch64_D19_D20_D21 = 425,
  AArch64_D20_D21_D22 = 426,
  AArch64_D21_D22_D23 = 427,
  AArch64_D22_D23_D24 = 428,
  AArch64_D23_D24_D25 = 429,
  AArch64_D24_D25_D26 = 430,
  AArch64_D25_D26_D27 = 431,
  AArch64_D26_D27_D28 = 432,
  AArch64_D27_D28_D29 = 433,
  AArch64_D28_D29_D30 = 434,
  AArch64_D29_D30_D31 = 435,
  AArch64_D30_D31_D0 = 436,
  AArch64_D31_D0_D1 = 437,
  AArch64_Q0_Q1 = 438,
  AArch64_Q1_Q2 = 439,
  AArch64_Q2_Q3 = 440,
  AArch64_Q3_Q4 = 441,
  AArch64_Q4_Q5 = 442,
  AArch64_Q5_Q6 = 443,
  AArch64_Q6_Q7 = 444,
  AArch64_Q7_Q8 = 445,
  AArch64_Q8_Q9 = 446,
  AArch64_Q9_Q10 = 447,
  AArch64_Q10_Q11 = 448,
  AArch64_Q11_Q12 = 449,
  AArch64_Q12_Q13 = 450,
  AArch64_Q13_Q14 = 451,
  AArch64_Q14_Q15 = 452,
  AArch64_Q15_Q16 = 453,
  AArch64_Q16_Q17 = 454,
  AArch64_Q17_Q18 = 455,
  AArch64_Q18_Q19 = 456,
  AArch64_Q19_Q20 = 457,
  AArch64_Q20_Q21 = 458,
  AArch64_Q21_Q22 = 459,
  AArch64_Q22_Q23 = 460,
  AArch64_Q23_Q24 = 461,
  AArch64_Q24_Q25 = 462,
  AArch64_Q25_Q26 = 463,
  AArch64_Q26_Q27 = 464,
  AArch64_Q27_Q28 = 465,
  AArch64_Q28_Q29 = 466,
  AArch64_Q29_Q30 = 467,
  AArch64_Q30_Q31 = 468,
  AArch64_Q31_Q0 = 469,
  AArch64_Q0_Q1_Q2_Q3 = 470,
  AArch64_Q1_Q2_Q3_Q4 = 471,
  AArch64_Q2_Q3_Q4_Q5 = 472,
  AArch64_Q3_Q4_Q5_Q6 = 473,
  AArch64_Q4_Q5_Q6_Q7 = 474,
  AArch64_Q5_Q6_Q7_Q8 = 475,
  AArch64_Q6_Q7_Q8_Q9 = 476,
  AArch64_Q7_Q8_Q9_Q10 = 477,
  AArch64_Q8_Q9_Q10_Q11 = 478,
  AArch64_Q9_Q10_Q11_Q12 = 479,
  AArch64_Q10_Q11_Q12_Q13 = 480,
  AArch64_Q11_Q12_Q13_Q14 = 481,
  AArch64_Q12_Q13_Q14_Q15 = 482,
  AArch64_Q13_Q14_Q15_Q16 = 483,
  AArch64_Q14_Q15_Q16_Q17 = 484,
  AArch64_Q15_Q16_Q17_Q18 = 485,
  AArch64_Q16_Q17_Q18_Q19 = 486,
  AArch64_Q17_Q18_Q19_Q20 = 487,
  AArch64_Q18_Q19_Q20_Q21 = 488,
  AArch64_Q19_Q20_Q21_Q22 = 489,
  AArch64_Q20_Q21_Q22_Q23 = 490,
  AArch64_Q21_Q22_Q23_Q24 = 491,
  AArch64_Q22_Q23_Q24_Q25 = 492,
  AArch64_Q23_Q24_Q25_Q26 = 493,
  AArch64_Q24_Q25_Q26_Q27 = 494,
  AArch64_Q25_Q26_Q27_Q28 = 495,
  AArch64_Q26_Q27_Q28_Q29 = 496,
  AArch64_Q27_Q28_Q29_Q30 = 497,
  AArch64_Q28_Q29_Q30_Q31 = 498,
  AArch64_Q29_Q30_Q31_Q0 = 499,
  AArch64_Q30_Q31_Q0_Q1 = 500,
  AArch64_Q31_Q0_Q1_Q2 = 501,
  AArch64_Q0_Q1_Q2 = 502,
  AArch64_Q1_Q2_Q3 = 503,
  AArch64_Q2_Q3_Q4 = 504,
  AArch64_Q3_Q4_Q5 = 505,
  AArch64_Q4_Q5_Q6 = 506,
  AArch64_Q5_Q6_Q7 = 507,
  AArch64_Q6_Q7_Q8 = 508,
  AArch64_Q7_Q8_Q9 = 509,
  AArch64_Q8_Q9_Q10 = 510,
  AArch64_Q9_Q10_Q11 = 511,
  AArch64_Q10_Q11_Q12 = 512,
  AArch64_Q11_Q12_Q13 = 513,
  AArch64_Q12_Q13_Q14 = 514,
  AArch64_Q13_Q14_Q15 = 515,
  AArch64_Q14_Q15_Q16 = 516,
  AArch64_Q15_Q16_Q17 = 517,
  AArch64_Q16_Q17_Q18 = 518,
  AArch64_Q17_Q18_Q19 = 519,
  AArch64_Q18_Q19_Q20 = 520,
  AArch64_Q19_Q20_Q21 = 521,
  AArch64_Q20_Q21_Q22 = 522,
  AArch64_Q21_Q22_Q23 = 523,
  AArch64_Q22_Q23_Q24 = 524,
  AArch64_Q23_Q24_Q25 = 525,
  AArch64_Q24_Q25_Q26 = 526,
  AArch64_Q25_Q26_Q27 = 527,
  AArch64_Q26_Q27_Q28 = 528,
  AArch64_Q27_Q28_Q29 = 529,
  AArch64_Q28_Q29_Q30 = 530,
  AArch64_Q29_Q30_Q31 = 531,
  AArch64_Q30_Q31_Q0 = 532,
  AArch64_Q31_Q0_Q1 = 533,
  AArch64_X22_X23_X24_X25_X26_X27_X28_FP = 534,
  AArch64_X0_X1_X2_X3_X4_X5_X6_X7 = 535,
  AArch64_X2_X3_X4_X5_X6_X7_X8_X9 = 536,
  AArch64_X4_X5_X6_X7_X8_X9_X10_X11 = 537,
  AArch64_X6_X7_X8_X9_X10_X11_X12_X13 = 538,
  AArch64_X8_X9_X10_X11_X12_X13_X14_X15 = 539,
  AArch64_X10_X11_X12_X13_X14_X15_X16_X17 = 540,
  AArch64_X12_X13_X14_X15_X16_X17_X18_X19 = 541,
  AArch64_X14_X15_X16_X17_X18_X19_X20_X21 = 542,
  AArch64_X16_X17_X18_X19_X20_X21_X22_X23 = 543,
  AArch64_X18_X19_X20_X21_X22_X23_X24_X25 = 544,
  AArch64_X20_X21_X22_X23_X24_X25_X26_X27 = 545,
  AArch64_W30_WZR = 546,
  AArch64_W0_W1 = 547,
  AArch64_W2_W3 = 548,
  AArch64_W4_W5 = 549,
  AArch64_W6_W7 = 550,
  AArch64_W8_W9 = 551,
  AArch64_W10_W11 = 552,
  AArch64_W12_W13 = 553,
  AArch64_W14_W15 = 554,
  AArch64_W16_W17 = 555,
  AArch64_W18_W19 = 556,
  AArch64_W20_W21 = 557,
  AArch64_W22_W23 = 558,
  AArch64_W24_W25 = 559,
  AArch64_W26_W27 = 560,
  AArch64_W28_W29 = 561,
  AArch64_LR_XZR = 562,
  AArch64_X28_FP = 563,
  AArch64_X0_X1 = 564,
  AArch64_X2_X3 = 565,
  AArch64_X4_X5 = 566,
  AArch64_X6_X7 = 567,
  AArch64_X8_X9 = 568,
  AArch64_X10_X11 = 569,
  AArch64_X12_X13 = 570,
  AArch64_X14_X15 = 571,
  AArch64_X16_X17 = 572,
  AArch64_X18_X19 = 573,
  AArch64_X20_X21 = 574,
  AArch64_X22_X23 = 575,
  AArch64_X24_X25 = 576,
  AArch64_X26_X27 = 577,
  AArch64_Z0_Z1 = 578,
  AArch64_Z1_Z2 = 579,
  AArch64_Z2_Z3 = 580,
  AArch64_Z3_Z4 = 581,
  AArch64_Z4_Z5 = 582,
  AArch64_Z5_Z6 = 583,
  AArch64_Z6_Z7 = 584,
  AArch64_Z7_Z8 = 585,
  AArch64_Z8_Z9 = 586,
  AArch64_Z9_Z10 = 587,
  AArch64_Z10_Z11 = 588,
  AArch64_Z11_Z12 = 589,
  AArch64_Z12_Z13 = 590,
  AArch64_Z13_Z14 = 591,
  AArch64_Z14_Z15 = 592,
  AArch64_Z15_Z16 = 593,
  AArch64_Z16_Z17 = 594,
  AArch64_Z17_Z18 = 595,
  AArch64_Z18_Z19 = 596,
  AArch64_Z19_Z20 = 597,
  AArch64_Z20_Z21 = 598,
  AArch64_Z21_Z22 = 599,
  AArch64_Z22_Z23 = 600,
  AArch64_Z23_Z24 = 601,
  AArch64_Z24_Z25 = 602,
  AArch64_Z25_Z26 = 603,
  AArch64_Z26_Z27 = 604,
  AArch64_Z27_Z28 = 605,
  AArch64_Z28_Z29 = 606,
  AArch64_Z29_Z30 = 607,
  AArch64_Z30_Z31 = 608,
  AArch64_Z31_Z0 = 609,
  AArch64_Z0_Z1_Z2_Z3 = 610,
  AArch64_Z1_Z2_Z3_Z4 = 611,
  AArch64_Z2_Z3_Z4_Z5 = 612,
  AArch64_Z3_Z4_Z5_Z6 = 613,
  AArch64_Z4_Z5_Z6_Z7 = 614,
  AArch64_Z5_Z6_Z7_Z8 = 615,
  AArch64_Z6_Z7_Z8_Z9 = 616,
  AArch64_Z7_Z8_Z9_Z10 = 617,
  AArch64_Z8_Z9_Z10_Z11 = 618,
  AArch64_Z9_Z10_Z11_Z12 = 619,
  AArch64_Z10_Z11_Z12_Z13 = 620,
  AArch64_Z11_Z12_Z13_Z14 = 621,
  AArch64_Z12_Z13_Z14_Z15 = 622,
  AArch64_Z13_Z14_Z15_Z16 = 623,
  AArch64_Z14_Z15_Z16_Z17 = 624,
  AArch64_Z15_Z16_Z17_Z18 = 625,
  AArch64_Z16_Z17_Z18_Z19 = 626,
  AArch64_Z17_Z18_Z19_Z20 = 627,
  AArch64_Z18_Z19_Z20_Z21 = 628,
  AArch64_Z19_Z20_Z21_Z22 = 629,
  AArch64_Z20_Z21_Z22_Z23 = 630,
  AArch64_Z21_Z22_Z23_Z24 = 631,
  AArch64_Z22_Z23_Z24_Z25 = 632,
  AArch64_Z23_Z24_Z25_Z26 = 633,
  AArch64_Z24_Z25_Z26_Z27 = 634,
  AArch64_Z25_Z26_Z27_Z28 = 635,
  AArch64_Z26_Z27_Z28_Z29 = 636,
  AArch64_Z27_Z28_Z29_Z30 = 637,
  AArch64_Z28_Z29_Z30_Z31 = 638,
  AArch64_Z29_Z30_Z31_Z0 = 639,
  AArch64_Z30_Z31_Z0_Z1 = 640,
  AArch64_Z31_Z0_Z1_Z2 = 641,
  AArch64_Z0_Z1_Z2 = 642,
  AArch64_Z1_Z2_Z3 = 643,
  AArch64_Z2_Z3_Z4 = 644,
  AArch64_Z3_Z4_Z5 = 645,
  AArch64_Z4_Z5_Z6 = 646,
  AArch64_Z5_Z6_Z7 = 647,
  AArch64_Z6_Z7_Z8 = 648,
  AArch64_Z7_Z8_Z9 = 649,
  AArch64_Z8_Z9_Z10 = 650,
  AArch64_Z9_Z10_Z11 = 651,
  AArch64_Z10_Z11_Z12 = 652,
  AArch64_Z11_Z12_Z13 = 653,
  AArch64_Z12_Z13_Z14 = 654,
  AArch64_Z13_Z14_Z15 = 655,
  AArch64_Z14_Z15_Z16 = 656,
  AArch64_Z15_Z16_Z17 = 657,
  AArch64_Z16_Z17_Z18 = 658,
  AArch64_Z17_Z18_Z19 = 659,
  AArch64_Z18_Z19_Z20 = 660,
  AArch64_Z19_Z20_Z21 = 661,
  AArch64_Z20_Z21_Z22 = 662,
  AArch64_Z21_Z22_Z23 = 663,
  AArch64_Z22_Z23_Z24 = 664,
  AArch64_Z23_Z24_Z25 = 665,
  AArch64_Z24_Z25_Z26 = 666,
  AArch64_Z25_Z26_Z27 = 667,
  AArch64_Z26_Z27_Z28 = 668,
  AArch64_Z27_Z28_Z29 = 669,
  AArch64_Z28_Z29_Z30 = 670,
  AArch64_Z29_Z30_Z31 = 671,
  AArch64_Z30_Z31_Z0 = 672,
  AArch64_Z31_Z0_Z1 = 673,
  AArch64_NUM_TARGET_REGS // 674
};

// Register classes
enum {
  AArch64_FPR8RegClassID = 0,
  AArch64_FPR16RegClassID = 1,
  AArch64_FPR16_loRegClassID = 2,
  AArch64_PPRRegClassID = 3,
  AArch64_PPR_3bRegClassID = 4,
  AArch64_GPR32allRegClassID = 5,
  AArch64_FPR32RegClassID = 6,
  AArch64_GPR32RegClassID = 7,
  AArch64_GPR32spRegClassID = 8,
  AArch64_GPR32commonRegClassID = 9,
  AArch64_FPR32_with_hsub_in_FPR16_loRegClassID = 10,
  AArch64_GPR32argRegClassID = 11,
  AArch64_MatrixIndexGPR32_12_15RegClassID = 12,
  AArch64_CCRRegClassID = 13,
  AArch64_GPR32sponlyRegClassID = 14,
  AArch64_WSeqPairsClassRegClassID = 15,
  AArch64_WSeqPairsClass_with_subo32_in_GPR32commonRegClassID = 16,
  AArch64_WSeqPairsClass_with_sube32_in_GPR32argRegClassID = 17,
  AArch64_WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15RegClassID = 18,
  AArch64_GPR64allRegClassID = 19,
  AArch64_FPR64RegClassID = 20,
  AArch64_GPR64RegClassID = 21,
  AArch64_GPR64spRegClassID = 22,
  AArch64_GPR64commonRegClassID = 23,
  AArch64_GPR64noipRegClassID = 24,
  AArch64_GPR64common_and_GPR64noipRegClassID = 25,
  AArch64_tcGPR64RegClassID = 26,
  AArch64_GPR64noip_and_tcGPR64RegClassID = 27,
  AArch64_FPR64_loRegClassID = 28,
  AArch64_GPR64argRegClassID = 29,
  AArch64_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 30,
  AArch64_rtcGPR64RegClassID = 31,
  AArch64_GPR64sponlyRegClassID = 32,
  AArch64_DDRegClassID = 33,
  AArch64_DD_with_dsub0_in_FPR64_loRegClassID = 34,
  AArch64_DD_with_dsub1_in_FPR64_loRegClassID = 35,
  AArch64_XSeqPairsClassRegClassID = 36,
  AArch64_DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_loRegClassID = 37,
  AArch64_XSeqPairsClass_with_subo64_in_GPR64commonRegClassID = 38,
  AArch64_XSeqPairsClass_with_subo64_in_GPR64noipRegClassID = 39,
  AArch64_XSeqPairsClass_with_sube64_in_GPR64noipRegClassID = 40,
  AArch64_XSeqPairsClass_with_sube64_in_tcGPR64RegClassID = 41,
  AArch64_XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64RegClassID = 42,
  AArch64_XSeqPairsClass_with_subo64_in_tcGPR64RegClassID = 43,
  AArch64_XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64RegClassID = 44,
  AArch64_XSeqPairsClass_with_sub_32_in_GPR32argRegClassID = 45,
  AArch64_XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 46,
  AArch64_XSeqPairsClass_with_sube64_in_rtcGPR64RegClassID = 47,
  AArch64_FPR128RegClassID = 48,
  AArch64_ZPRRegClassID = 49,
  AArch64_FPR128_loRegClassID = 50,
  AArch64_MPR128RegClassID = 51,
  AArch64_ZPR_4bRegClassID = 52,
  AArch64_ZPR_3bRegClassID = 53,
  AArch64_DDDRegClassID = 54,
  AArch64_DDD_with_dsub0_in_FPR64_loRegClassID = 55,
  AArch64_DDD_with_dsub1_in_FPR64_loRegClassID = 56,
  AArch64_DDD_with_dsub2_in_FPR64_loRegClassID = 57,
  AArch64_DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_loRegClassID = 58,
  AArch64_DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loRegClassID = 59,
  AArch64_DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loRegClassID = 60,
  AArch64_DDDDRegClassID = 61,
  AArch64_DDDD_with_dsub0_in_FPR64_loRegClassID = 62,
  AArch64_DDDD_with_dsub1_in_FPR64_loRegClassID = 63,
  AArch64_DDDD_with_dsub2_in_FPR64_loRegClassID = 64,
  AArch64_DDDD_with_dsub3_in_FPR64_loRegClassID = 65,
  AArch64_DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_loRegClassID = 66,
  AArch64_DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loRegClassID = 67,
  AArch64_DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loRegClassID = 68,
  AArch64_DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loRegClassID = 69,
  AArch64_DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loRegClassID = 70,
  AArch64_DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loRegClassID = 71,
  AArch64_QQRegClassID = 72,
  AArch64_ZPR2RegClassID = 73,
  AArch64_QQ_with_dsub_in_FPR64_loRegClassID = 74,
  AArch64_QQ_with_qsub1_in_FPR128_loRegClassID = 75,
  AArch64_ZPR2_with_dsub_in_FPR64_loRegClassID = 76,
  AArch64_ZPR2_with_zsub1_in_ZPR_4bRegClassID = 77,
  AArch64_QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_loRegClassID = 78,
  AArch64_ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4bRegClassID = 79,
  AArch64_ZPR2_with_zsub0_in_ZPR_3bRegClassID = 80,
  AArch64_ZPR2_with_zsub1_in_ZPR_3bRegClassID = 81,
  AArch64_ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3bRegClassID = 82,
  AArch64_MPR64RegClassID = 83,
  AArch64_QQQRegClassID = 84,
  AArch64_ZPR3RegClassID = 85,
  AArch64_QQQ_with_dsub_in_FPR64_loRegClassID = 86,
  AArch64_QQQ_with_qsub1_in_FPR128_loRegClassID = 87,
  AArch64_QQQ_with_qsub2_in_FPR128_loRegClassID = 88,
  AArch64_ZPR3_with_dsub_in_FPR64_loRegClassID = 89,
  AArch64_ZPR3_with_zsub1_in_ZPR_4bRegClassID = 90,
  AArch64_ZPR3_with_zsub2_in_ZPR_4bRegClassID = 91,
  AArch64_QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_loRegClassID = 92,
  AArch64_QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_loRegClassID = 93,
  AArch64_ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4bRegClassID = 94,
  AArch64_ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4bRegClassID = 95,
  AArch64_QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_loRegClassID = 96,
  AArch64_ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4bRegClassID = 97,
  AArch64_ZPR3_with_zsub0_in_ZPR_3bRegClassID = 98,
  AArch64_ZPR3_with_zsub1_in_ZPR_3bRegClassID = 99,
  AArch64_ZPR3_with_zsub2_in_ZPR_3bRegClassID = 100,
  AArch64_ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3bRegClassID = 101,
  AArch64_ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3bRegClassID = 102,
  AArch64_ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3bRegClassID = 103,
  AArch64_QQQQRegClassID = 104,
  AArch64_ZPR4RegClassID = 105,
  AArch64_QQQQ_with_dsub_in_FPR64_loRegClassID = 106,
  AArch64_QQQQ_with_qsub1_in_FPR128_loRegClassID = 107,
  AArch64_QQQQ_with_qsub2_in_FPR128_loRegClassID = 108,
  AArch64_QQQQ_with_qsub3_in_FPR128_loRegClassID = 109,
  AArch64_ZPR4_with_dsub_in_FPR64_loRegClassID = 110,
  AArch64_ZPR4_with_zsub1_in_ZPR_4bRegClassID = 111,
  AArch64_ZPR4_with_zsub2_in_ZPR_4bRegClassID = 112,
  AArch64_ZPR4_with_zsub3_in_ZPR_4bRegClassID = 113,
  AArch64_QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_loRegClassID = 114,
  AArch64_QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_loRegClassID = 115,
  AArch64_QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loRegClassID = 116,
  AArch64_ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4bRegClassID = 117,
  AArch64_ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4bRegClassID = 118,
  AArch64_ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bRegClassID = 119,
  AArch64_QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_loRegClassID = 120,
  AArch64_QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loRegClassID = 121,
  AArch64_ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4bRegClassID = 122,
  AArch64_ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bRegClassID = 123,
  AArch64_QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_loRegClassID = 124,
  AArch64_ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4bRegClassID = 125,
  AArch64_ZPR4_with_zsub0_in_ZPR_3bRegClassID = 126,
  AArch64_ZPR4_with_zsub1_in_ZPR_3bRegClassID = 127,
  AArch64_ZPR4_with_zsub2_in_ZPR_3bRegClassID = 128,
  AArch64_ZPR4_with_zsub3_in_ZPR_3bRegClassID = 129,
  AArch64_ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3bRegClassID = 130,
  AArch64_ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3bRegClassID = 131,
  AArch64_ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bRegClassID = 132,
  AArch64_ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3bRegClassID = 133,
  AArch64_ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bRegClassID = 134,
  AArch64_ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3bRegClassID = 135,
  AArch64_GPR64x8ClassRegClassID = 136,
  AArch64_GPR64x8Class_with_x8sub_0_in_GPR64noipRegClassID = 137,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noipRegClassID = 138,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 139,
  AArch64_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 140,
  AArch64_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipRegClassID = 141,
  AArch64_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 142,
  AArch64_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 143,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64RegClassID = 144,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 145,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 146,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 147,
  AArch64_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64RegClassID = 148,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noipRegClassID = 149,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 150,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 151,
  AArch64_GPR64x8Class_with_x8sub_1_in_tcGPR64RegClassID = 152,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 153,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 154,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 155,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 156,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipRegClassID = 157,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 158,
  AArch64_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 159,
  AArch64_GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64RegClassID = 160,
  AArch64_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 161,
  AArch64_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 162,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 163,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 164,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64RegClassID = 165,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 166,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 167,
  AArch64_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 168,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 169,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 170,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipRegClassID = 171,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 172,
  AArch64_GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64RegClassID = 173,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 174,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 175,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 176,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64RegClassID = 177,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 178,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 179,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 180,
  AArch64_GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64RegClassID = 181,
  AArch64_GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64RegClassID = 182,
  AArch64_GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64RegClassID = 183,
  AArch64_GPR64x8Class_with_sub_32_in_GPR32argRegClassID = 184,
  AArch64_MPR32RegClassID = 185,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64argRegClassID = 186,
  AArch64_GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 187,
  AArch64_GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 188,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 189,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64argRegClassID = 190,
  AArch64_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 191,
  AArch64_GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15RegClassID = 192,
  AArch64_GPR64x8Class_with_x8sub_0_in_rtcGPR64RegClassID = 193,
  AArch64_GPR64x8Class_with_x8sub_2_in_rtcGPR64RegClassID = 194,
  AArch64_GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noipRegClassID = 195,
  AArch64_GPR64x8Class_with_x8sub_4_in_rtcGPR64RegClassID = 196,
  AArch64_GPR64x8Class_with_x8sub_6_in_GPR64argRegClassID = 197,
  AArch64_GPR64x8Class_with_x8sub_6_in_rtcGPR64RegClassID = 198,
  AArch64_MPR16RegClassID = 199,
  AArch64_MPRRegClassID = 200,
  AArch64_MPR8RegClassID = 201,
};

// Register alternate name indices
enum {
  AArch64_NoRegAltName,	// 0
  AArch64_vlist1,	// 1
  AArch64_vreg,	// 2
  AArch64_NUM_TARGET_REG_ALT_NAMES = 3
};

// Subregister indices
enum {
  AArch64_NoSubRegister,
  AArch64_bsub,	// 1
  AArch64_dsub,	// 2
  AArch64_dsub0,	// 3
  AArch64_dsub1,	// 4
  AArch64_dsub2,	// 5
  AArch64_dsub3,	// 6
  AArch64_hsub,	// 7
  AArch64_qsub0,	// 8
  AArch64_qsub1,	// 9
  AArch64_qsub2,	// 10
  AArch64_qsub3,	// 11
  AArch64_ssub,	// 12
  AArch64_sub_32,	// 13
  AArch64_sube32,	// 14
  AArch64_sube64,	// 15
  AArch64_subo32,	// 16
  AArch64_subo64,	// 17
  AArch64_x8sub_0,	// 18
  AArch64_x8sub_1,	// 19
  AArch64_x8sub_2,	// 20
  AArch64_x8sub_3,	// 21
  AArch64_x8sub_4,	// 22
  AArch64_x8sub_5,	// 23
  AArch64_x8sub_6,	// 24
  AArch64_x8sub_7,	// 25
  AArch64_zasubb,	// 26
  AArch64_zasubd0,	// 27
  AArch64_zasubd1,	// 28
  AArch64_zasubh0,	// 29
  AArch64_zasubh1,	// 30
  AArch64_zasubq0,	// 31
  AArch64_zasubq1,	// 32
  AArch64_zasubs0,	// 33
  AArch64_zasubs1,	// 34
  AArch64_zsub,	// 35
  AArch64_zsub0,	// 36
  AArch64_zsub1,	// 37
  AArch64_zsub2,	// 38
  AArch64_zsub3,	// 39
  AArch64_zsub_hi,	// 40
  AArch64_zasubd1_then_zasubq0,	// 41
  AArch64_zasubd1_then_zasubq1,	// 42
  AArch64_zasubs1_then_zasubd0,	// 43
  AArch64_zasubs1_then_zasubd1,	// 44
  AArch64_zasubs1_then_zasubq0,	// 45
  AArch64_zasubs1_then_zasubq1,	// 46
  AArch64_zasubs1_then_zasubd1_then_zasubq0,	// 47
  AArch64_zasubs1_then_zasubd1_then_zasubq1,	// 48
  AArch64_zasubh1_then_zasubd0,	// 49
  AArch64_zasubh1_then_zasubd1,	// 50
  AArch64_zasubh1_then_zasubq0,	// 51
  AArch64_zasubh1_then_zasubq1,	// 52
  AArch64_zasubh1_then_zasubs0,	// 53
  AArch64_zasubh1_then_zasubs1,	// 54
  AArch64_zasubh1_then_zasubd1_then_zasubq0,	// 55
  AArch64_zasubh1_then_zasubd1_then_zasubq1,	// 56
  AArch64_zasubh1_then_zasubs1_then_zasubd0,	// 57
  AArch64_zasubh1_then_zasubs1_then_zasubd1,	// 58
  AArch64_zasubh1_then_zasubs1_then_zasubq0,	// 59
  AArch64_zasubh1_then_zasubs1_then_zasubq1,	// 60
  AArch64_zasubh1_then_zasubs1_then_zasubd1_then_zasubq0,	// 61
  AArch64_zasubh1_then_zasubs1_then_zasubd1_then_zasubq1,	// 62
  AArch64_dsub1_then_bsub,	// 63
  AArch64_dsub1_then_hsub,	// 64
  AArch64_dsub1_then_ssub,	// 65
  AArch64_dsub3_then_bsub,	// 66
  AArch64_dsub3_then_hsub,	// 67
  AArch64_dsub3_then_ssub,	// 68
  AArch64_dsub2_then_bsub,	// 69
  AArch64_dsub2_then_hsub,	// 70
  AArch64_dsub2_then_ssub,	// 71
  AArch64_qsub1_then_bsub,	// 72
  AArch64_qsub1_then_dsub,	// 73
  AArch64_qsub1_then_hsub,	// 74
  AArch64_qsub1_then_ssub,	// 75
  AArch64_qsub3_then_bsub,	// 76
  AArch64_qsub3_then_dsub,	// 77
  AArch64_qsub3_then_hsub,	// 78
  AArch64_qsub3_then_ssub,	// 79
  AArch64_qsub2_then_bsub,	// 80
  AArch64_qsub2_then_dsub,	// 81
  AArch64_qsub2_then_hsub,	// 82
  AArch64_qsub2_then_ssub,	// 83
  AArch64_x8sub_7_then_sub_32,	// 84
  AArch64_x8sub_6_then_sub_32,	// 85
  AArch64_x8sub_5_then_sub_32,	// 86
  AArch64_x8sub_4_then_sub_32,	// 87
  AArch64_x8sub_3_then_sub_32,	// 88
  AArch64_x8sub_2_then_sub_32,	// 89
  AArch64_x8sub_1_then_sub_32,	// 90
  AArch64_subo64_then_sub_32,	// 91
  AArch64_zsub1_then_bsub,	// 92
  AArch64_zsub1_then_dsub,	// 93
  AArch64_zsub1_then_hsub,	// 94
  AArch64_zsub1_then_ssub,	// 95
  AArch64_zsub1_then_zsub,	// 96
  AArch64_zsub1_then_zsub_hi,	// 97
  AArch64_zsub3_then_bsub,	// 98
  AArch64_zsub3_then_dsub,	// 99
  AArch64_zsub3_then_hsub,	// 100
  AArch64_zsub3_then_ssub,	// 101
  AArch64_zsub3_then_zsub,	// 102
  AArch64_zsub3_then_zsub_hi,	// 103
  AArch64_zsub2_then_bsub,	// 104
  AArch64_zsub2_then_dsub,	// 105
  AArch64_zsub2_then_hsub,	// 106
  AArch64_zsub2_then_ssub,	// 107
  AArch64_zsub2_then_zsub,	// 108
  AArch64_zsub2_then_zsub_hi,	// 109
  AArch64_dsub0_dsub1,	// 110
  AArch64_dsub0_dsub1_dsub2,	// 111
  AArch64_dsub1_dsub2,	// 112
  AArch64_dsub1_dsub2_dsub3,	// 113
  AArch64_dsub2_dsub3,	// 114
  AArch64_dsub_qsub1_then_dsub,	// 115
  AArch64_dsub_qsub1_then_dsub_qsub2_then_dsub_qsub3_then_dsub,	// 116
  AArch64_dsub_qsub1_then_dsub_qsub2_then_dsub,	// 117
  AArch64_qsub0_qsub1,	// 118
  AArch64_qsub0_qsub1_qsub2,	// 119
  AArch64_qsub1_qsub2,	// 120
  AArch64_qsub1_qsub2_qsub3,	// 121
  AArch64_qsub2_qsub3,	// 122
  AArch64_qsub1_then_dsub_qsub2_then_dsub,	// 123
  AArch64_qsub1_then_dsub_qsub2_then_dsub_qsub3_then_dsub,	// 124
  AArch64_qsub2_then_dsub_qsub3_then_dsub,	// 125
  AArch64_sub_32_x8sub_1_then_sub_32,	// 126
  AArch64_x8sub_0_x8sub_1,	// 127
  AArch64_x8sub_2_x8sub_3,	// 128
  AArch64_x8sub_4_x8sub_5,	// 129
  AArch64_x8sub_6_x8sub_7,	// 130
  AArch64_x8sub_6_then_sub_32_x8sub_7_then_sub_32,	// 131
  AArch64_x8sub_4_then_sub_32_x8sub_5_then_sub_32,	// 132
  AArch64_x8sub_2_then_sub_32_x8sub_3_then_sub_32,	// 133
  AArch64_sub_32_subo64_then_sub_32,	// 134
  AArch64_dsub_zsub1_then_dsub,	// 135
  AArch64_zsub_zsub1_then_zsub,	// 136
  AArch64_dsub_zsub1_then_dsub_zsub2_then_dsub_zsub3_then_dsub,	// 137
  AArch64_dsub_zsub1_then_dsub_zsub2_then_dsub,	// 138
  AArch64_zsub_zsub1_then_zsub_zsub2_then_zsub_zsub3_then_zsub,	// 139
  AArch64_zsub_zsub1_then_zsub_zsub2_then_zsub,	// 140
  AArch64_zsub0_zsub1,	// 141
  AArch64_zsub0_zsub1_zsub2,	// 142
  AArch64_zsub1_zsub2,	// 143
  AArch64_zsub1_zsub2_zsub3,	// 144
  AArch64_zsub2_zsub3,	// 145
  AArch64_zsub1_then_dsub_zsub2_then_dsub,	// 146
  AArch64_zsub1_then_dsub_zsub2_then_dsub_zsub3_then_dsub,	// 147
  AArch64_zsub1_then_zsub_zsub2_then_zsub,	// 148
  AArch64_zsub1_then_zsub_zsub2_then_zsub_zsub3_then_zsub,	// 149
  AArch64_zsub2_then_dsub_zsub3_then_dsub,	// 150
  AArch64_zsub2_then_zsub_zsub3_then_zsub,	// 151
  AArch64_NUM_TARGET_SUBREGS
};

#endif // GET_REGINFO_ENUM

#ifdef GET_REGINFO_MC_DESC
#undef GET_REGINFO_MC_DESC


static const MCPhysReg AArch64RegDiffLists[] = {
  /* 0 */ 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
  /* 17 */ 63239, 1, 1, 1, 1, 1, 1, 1, 0,
  /* 26 */ 64537, 1, 1, 1, 1, 1, 1, 1, 0,
  /* 35 */ 1, 92, 1, 1, 1, 1, 1, 1, 0,
  /* 44 */ 64949, 1, 1, 1, 74, 1, 1, 1, 0,
  /* 53 */ 63083, 1, 1, 1, 0,
  /* 58 */ 63095, 1, 1, 1, 0,
  /* 63 */ 65089, 1, 1, 1, 0,
  /* 68 */ 65185, 1, 1, 1, 0,
  /* 73 */ 31, 318, 17, 65504, 1, 1, 1, 0,
  /* 81 */ 31, 319, 17, 65504, 1, 1, 1, 0,
  /* 89 */ 31, 320, 17, 65504, 1, 1, 1, 0,
  /* 97 */ 31, 321, 17, 65504, 1, 1, 1, 0,
  /* 105 */ 31, 322, 17, 65504, 1, 1, 1, 0,
  /* 113 */ 31, 323, 17, 65504, 1, 1, 1, 0,
  /* 121 */ 31, 324, 17, 65504, 1, 1, 1, 0,
  /* 129 */ 31, 325, 17, 65504, 1, 1, 1, 0,
  /* 137 */ 31, 326, 17, 65504, 1, 1, 1, 0,
  /* 145 */ 335, 65504, 1, 1, 1, 0,
  /* 151 */ 336, 65504, 1, 1, 1, 0,
  /* 157 */ 337, 65504, 1, 1, 1, 0,
  /* 163 */ 338, 65504, 1, 1, 1, 0,
  /* 169 */ 339, 65504, 1, 1, 1, 0,
  /* 175 */ 340, 65504, 1, 1, 1, 0,
  /* 181 */ 341, 65504, 1, 1, 1, 0,
  /* 187 */ 342, 65504, 1, 1, 1, 0,
  /* 193 */ 343, 65504, 1, 1, 1, 0,
  /* 199 */ 31, 317, 17, 65495, 9, 1, 1, 0,
  /* 207 */ 31, 318, 17, 65495, 9, 1, 1, 0,
  /* 215 */ 334, 65495, 9, 1, 1, 0,
  /* 221 */ 335, 65495, 9, 1, 1, 0,
  /* 227 */ 23, 29, 1, 1, 0,
  /* 232 */ 23, 29, 1, 1, 46, 29, 1, 1, 0,
  /* 241 */ 64917, 1, 1, 75, 1, 1, 0,
  /* 248 */ 65057, 1, 1, 0,
  /* 252 */ 65153, 1, 1, 0,
  /* 256 */ 31, 326, 17, 65505, 1, 1, 0,
  /* 263 */ 31, 327, 17, 65505, 1, 1, 0,
  /* 270 */ 343, 65505, 1, 1, 0,
  /* 275 */ 344, 65505, 1, 1, 0,
  /* 280 */ 31, 316, 17, 65494, 10, 1, 0,
  /* 287 */ 31, 317, 17, 65494, 10, 1, 0,
  /* 294 */ 333, 65494, 10, 1, 0,
  /* 299 */ 334, 65494, 10, 1, 0,
  /* 304 */ 23, 1, 29, 1, 0,
  /* 309 */ 23, 1, 29, 1, 46, 1, 29, 1, 0,
  /* 318 */ 23, 30, 1, 0,
  /* 322 */ 23, 30, 1, 46, 30, 1, 0,
  /* 329 */ 64981, 1, 76, 1, 0,
  /* 334 */ 65173, 112, 65456, 65472, 33, 112, 65456, 65472, 33, 112, 65456, 65472, 329, 1, 0,
  /* 349 */ 65173, 112, 65456, 65472, 33, 112, 65456, 65472, 1, 112, 65456, 65472, 361, 1, 0,
  /* 364 */ 63261, 1, 0,
  /* 367 */ 63273, 1, 0,
  /* 370 */ 63291, 1, 0,
  /* 373 */ 63303, 1, 0,
  /* 376 */ 64479, 1, 0,
  /* 379 */ 64513, 1, 0,
  /* 382 */ 65121, 1, 0,
  /* 385 */ 65176, 1, 0,
  /* 388 */ 65177, 1, 0,
  /* 391 */ 65178, 1, 0,
  /* 394 */ 65179, 1, 0,
  /* 397 */ 65180, 1, 0,
  /* 400 */ 65181, 1, 0,
  /* 403 */ 65182, 1, 0,
  /* 406 */ 65183, 1, 0,
  /* 409 */ 65184, 1, 0,
  /* 412 */ 65185, 1, 0,
  /* 415 */ 65186, 1, 0,
  /* 418 */ 65187, 1, 0,
  /* 421 */ 65188, 1, 0,
  /* 424 */ 65189, 1, 0,
  /* 427 */ 65190, 1, 0,
  /* 430 */ 65217, 1, 0,
  /* 433 */ 64, 80, 65424, 80, 124, 94, 1, 62, 65503, 34, 65503, 34, 65503, 1, 63, 1, 62, 65503, 34, 65503, 34, 65503, 1, 107, 1, 62, 65503, 34, 65503, 34, 65503, 1, 0,
  /* 466 */ 124, 190, 1, 62, 65503, 34, 65503, 34, 65503, 1, 107, 1, 62, 65503, 34, 65503, 34, 65503, 1, 0,
  /* 486 */ 65473, 330, 1, 62, 65503, 34, 65503, 34, 65503, 1, 0,
  /* 497 */ 64, 80, 65424, 80, 124, 95, 31, 33, 65504, 62, 65503, 34, 65503, 1, 33, 31, 33, 65504, 62, 65503, 34, 65503, 1, 77, 31, 33, 65504, 62, 65503, 34, 65503, 1, 0,
  /* 530 */ 124, 191, 31, 33, 65504, 62, 65503, 34, 65503, 1, 77, 31, 33, 65504, 62, 65503, 34, 65503, 1, 0,
  /* 550 */ 65473, 331, 31, 33, 65504, 62, 65503, 34, 65503, 1, 0,
  /* 561 */ 63, 65503, 34, 65503, 1, 64, 63, 65503, 34, 65503, 1, 108, 63, 65503, 34, 65503, 1, 0,
  /* 579 */ 64, 80, 65424, 80, 124, 94, 1, 63, 1, 65503, 1, 62, 65503, 1, 33, 1, 63, 1, 65503, 1, 62, 65503, 1, 77, 1, 63, 1, 65503, 1, 62, 65503, 1, 0,
  /* 612 */ 124, 190, 1, 63, 1, 65503, 1, 62, 65503, 1, 77, 1, 63, 1, 65503, 1, 62, 65503, 1, 0,
  /* 632 */ 65473, 330, 1, 63, 1, 65503, 1, 62, 65503, 1, 0,
  /* 643 */ 64, 65504, 63, 65503, 1, 33, 64, 65504, 63, 65503, 1, 77, 64, 65504, 63, 65503, 1, 0,
  /* 661 */ 65503, 1, 128, 65503, 1, 172, 65503, 1, 0,
  /* 670 */ 31, 327, 17, 65506, 1, 0,
  /* 676 */ 31, 328, 17, 65506, 1, 0,
  /* 682 */ 344, 65506, 1, 0,
  /* 686 */ 345, 65506, 1, 0,
  /* 690 */ 2, 0,
  /* 692 */ 2, 4, 0,
  /* 695 */ 64976, 4, 0,
  /* 698 */ 6, 0,
  /* 700 */ 269, 9, 18, 65510, 10, 8, 65522, 10, 8, 6, 65510, 10, 8, 65522, 10, 8, 65521, 18, 65510, 10, 8, 65522, 10, 8, 6, 65510, 10, 8, 65522, 10, 8, 0,
  /* 732 */ 31, 315, 17, 65493, 11, 0,
  /* 738 */ 31, 316, 17, 65493, 11, 0,
  /* 744 */ 332, 65493, 11, 0,
  /* 748 */ 333, 65493, 11, 0,
  /* 752 */ 12, 0,
  /* 754 */ 1, 537, 16, 0,
  /* 758 */ 65322, 543, 16, 0,
  /* 762 */ 23, 1, 1, 29, 0,
  /* 767 */ 23, 1, 1, 29, 46, 1, 1, 29, 0,
  /* 776 */ 64, 80, 65424, 80, 124, 94, 1, 62, 1, 65503, 34, 65503, 1, 29, 34, 1, 62, 1, 65503, 34, 65503, 1, 29, 78, 1, 62, 1, 65503, 34, 65503, 1, 29, 0,
  /* 809 */ 124, 190, 1, 62, 1, 65503, 34, 65503, 1, 29, 78, 1, 62, 1, 65503, 34, 65503, 1, 29, 0,
  /* 829 */ 65473, 330, 1, 62, 1, 65503, 34, 65503, 1, 29, 0,
  /* 840 */ 23, 1, 30, 0,
  /* 844 */ 23, 1, 30, 46, 1, 30, 0,
  /* 851 */ 63, 1, 65503, 1, 30, 34, 63, 1, 65503, 1, 30, 78, 63, 1, 65503, 1, 30, 0,
  /* 869 */ 23, 31, 0,
  /* 872 */ 23, 31, 46, 31, 0,
  /* 877 */ 65504, 31, 97, 65504, 31, 141, 65504, 31, 0,
  /* 886 */ 65312, 77, 0,
  /* 889 */ 65205, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 31, 96, 0,
  /* 906 */ 65205, 65412, 65456, 112, 65456, 65472, 299, 65442, 65412, 65456, 112, 65456, 65472, 299, 63, 96, 0,
  /* 923 */ 65141, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 30, 96, 65504, 96, 76, 1, 65300, 96, 0,
  /* 953 */ 65141, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65442, 65412, 65456, 112, 65456, 65472, 299, 62, 96, 65504, 96, 76, 1, 65300, 96, 0,
  /* 983 */ 65141, 65412, 65456, 112, 65456, 65472, 299, 65442, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 62, 96, 65504, 96, 76, 65505, 65300, 96, 0,
  /* 1013 */ 65173, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65442, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 61, 96, 65472, 32, 64, 32, 76, 64, 65473, 64, 65441, 65331, 64, 32, 64, 65345, 96, 0,
  /* 1059 */ 65173, 65412, 65456, 112, 65456, 65472, 299, 65442, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 61, 96, 65472, 32, 64, 32, 76, 64, 65441, 64, 65473, 65299, 64, 32, 64, 65377, 96, 0,
  /* 1105 */ 65173, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 29, 96, 65472, 32, 64, 32, 76, 64, 65473, 64, 65473, 65299, 64, 32, 64, 65377, 96, 0,
  /* 1151 */ 65173, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65474, 65412, 65456, 112, 65456, 65472, 299, 65442, 65412, 65456, 112, 65456, 65472, 299, 61, 96, 65472, 32, 64, 32, 76, 64, 65473, 64, 65473, 65299, 64, 32, 64, 65377, 96, 0,
  /* 1197 */ 1, 98, 0,
  /* 1200 */ 64976, 98, 0,
  /* 1203 */ 96, 140, 0,
  /* 1206 */ 214, 0,
  /* 1208 */ 65412, 65456, 112, 65456, 65472, 299, 0,
  /* 1215 */ 65221, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 330, 0,
  /* 1227 */ 65219, 65505, 65323, 214, 345, 0,
  /* 1233 */ 65203, 65505, 32, 65505, 346, 0,
  /* 1239 */ 65202, 65505, 32, 65505, 347, 0,
  /* 1245 */ 65201, 65505, 32, 65505, 348, 0,
  /* 1251 */ 65200, 65505, 32, 65505, 349, 0,
  /* 1257 */ 65199, 65505, 32, 65505, 350, 0,
  /* 1263 */ 65198, 65505, 32, 65505, 351, 0,
  /* 1269 */ 65197, 65505, 32, 65505, 352, 0,
  /* 1275 */ 65196, 65505, 32, 65505, 353, 0,
  /* 1281 */ 65195, 65505, 32, 65505, 354, 0,
  /* 1287 */ 65194, 65505, 32, 65505, 355, 0,
  /* 1293 */ 65193, 65505, 32, 65505, 356, 0,
  /* 1299 */ 65192, 65505, 32, 65505, 357, 0,
  /* 1305 */ 65191, 65505, 32, 65505, 358, 0,
  /* 1311 */ 65190, 65505, 32, 65505, 359, 0,
  /* 1317 */ 65221, 65456, 112, 65456, 65472, 81, 65456, 112, 65456, 65472, 362, 0,
  /* 1329 */ 64977, 214, 65328, 65535, 538, 0,
  /* 1335 */ 553, 0,
  /* 1337 */ 559, 0,
  /* 1339 */ 63118, 0,
  /* 1341 */ 63130, 0,
  /* 1343 */ 63148, 0,
  /* 1345 */ 63160, 0,
  /* 1347 */ 63181, 0,
  /* 1349 */ 63193, 0,
  /* 1351 */ 63211, 0,
  /* 1353 */ 63223, 0,
  /* 1355 */ 65518, 22, 65516, 65526, 65267, 0,
  /* 1361 */ 65526, 22, 65516, 65526, 65267, 0,
  /* 1367 */ 65518, 26, 65516, 65526, 65267, 0,
  /* 1373 */ 65526, 26, 65516, 65526, 65267, 0,
  /* 1379 */ 65518, 22, 65518, 65526, 65267, 0,
  /* 1385 */ 65526, 22, 65518, 65526, 65267, 0,
  /* 1391 */ 65518, 26, 65518, 65526, 65267, 0,
  /* 1397 */ 65526, 26, 65518, 65526, 65267, 0,
  /* 1403 */ 65518, 22, 65516, 65527, 65267, 0,
  /* 1409 */ 65526, 22, 65516, 65527, 65267, 0,
  /* 1415 */ 65518, 26, 65516, 65527, 65267, 0,
  /* 1421 */ 65526, 26, 65516, 65527, 65267, 0,
  /* 1427 */ 65518, 22, 65518, 65527, 65267, 0,
  /* 1433 */ 65526, 22, 65518, 65527, 65267, 0,
  /* 1439 */ 65518, 26, 65518, 65527, 65267, 0,
  /* 1445 */ 65526, 26, 65518, 65527, 65267, 0,
  /* 1451 */ 65321, 0,
  /* 1453 */ 65326, 0,
  /* 1455 */ 65207, 65327, 0,
  /* 1458 */ 65389, 0,
  /* 1460 */ 65404, 0,
  /* 1462 */ 65420, 0,
  /* 1464 */ 65436, 0,
  /* 1466 */ 65157, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 329, 64, 32, 1, 65440, 0,
  /* 1487 */ 65157, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 81, 65456, 112, 65456, 65472, 361, 64, 32, 1, 65440, 0,
  /* 1508 */ 65157, 65456, 112, 65456, 65472, 81, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 361, 64, 32, 65505, 65440, 0,
  /* 1529 */ 65189, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 81, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 360, 32, 32, 32, 64, 65473, 64, 65441, 65471, 64, 65441, 0,
  /* 1561 */ 65205, 112, 65456, 65472, 33, 112, 65456, 65472, 1, 112, 65456, 65472, 33, 112, 65456, 65472, 360, 64, 65473, 64, 65441, 0,
  /* 1583 */ 65237, 112, 65456, 65472, 1, 112, 65456, 65472, 0,
  /* 1592 */ 65237, 112, 65456, 65472, 33, 112, 65456, 65472, 0,
  /* 1601 */ 65456, 112, 65456, 65472, 0,
  /* 1606 */ 65189, 65456, 112, 65456, 65472, 81, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 360, 32, 32, 32, 64, 65441, 64, 65473, 65439, 64, 65473, 0,
  /* 1638 */ 65189, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 328, 32, 32, 32, 64, 65473, 64, 65473, 65439, 64, 65473, 0,
  /* 1670 */ 65189, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 113, 65456, 112, 65456, 65472, 81, 65456, 112, 65456, 65472, 360, 32, 32, 32, 64, 65473, 64, 65473, 65439, 64, 65473, 0,
  /* 1702 */ 65205, 112, 65456, 65472, 1, 112, 65456, 65472, 33, 112, 65456, 65472, 33, 112, 65456, 65472, 360, 64, 65441, 64, 65473, 0,
  /* 1724 */ 65205, 112, 65456, 65472, 33, 112, 65456, 65472, 33, 112, 65456, 65472, 33, 112, 65456, 65472, 328, 64, 65473, 64, 65473, 0,
  /* 1746 */ 65205, 112, 65456, 65472, 33, 112, 65456, 65472, 33, 112, 65456, 65472, 1, 112, 65456, 65472, 360, 64, 65473, 64, 65473, 0,
  /* 1768 */ 65484, 0,
  /* 1770 */ 65173, 112, 65456, 65472, 1, 112, 65456, 65472, 33, 112, 65456, 65472, 361, 65505, 0,
  /* 1785 */ 31, 315, 2, 65507, 0,
  /* 1790 */ 65322, 559, 2, 65507, 0,
  /* 1795 */ 31, 328, 17, 65507, 0,
  /* 1800 */ 31, 329, 17, 65507, 0,
  /* 1805 */ 317, 65507, 0,
  /* 1808 */ 345, 65507, 0,
  /* 1811 */ 346, 65507, 0,
  /* 1814 */ 561, 65507, 0,
  /* 1817 */ 65516, 0,
  /* 1819 */ 65526, 0,
  /* 1821 */ 65534, 0,
  /* 1823 */ 65229, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 343, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1848 */ 65228, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 344, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1873 */ 65227, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 345, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1898 */ 65226, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 346, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1923 */ 65225, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 347, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1948 */ 65224, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 348, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1973 */ 65223, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 349, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 1998 */ 65222, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 350, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 2023 */ 65221, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 351, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 2048 */ 65220, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 352, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 2073 */ 65219, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 353, 17, 1, 1, 1, 65519, 65535, 65535, 0,
  /* 2098 */ 65242, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 32, 65505, 65323, 214, 342, 17, 1, 1, 65522, 65534, 65535, 65535, 0,
};

static const uint16_t AArch64SubRegIdxLists[] = {
  /* 0 */ 2, 12, 7, 1, 0,
  /* 5 */ 13, 0,
  /* 7 */ 14, 16, 0,
  /* 10 */ 31, 32, 0,
  /* 13 */ 35, 2, 12, 7, 1, 40, 0,
  /* 20 */ 27, 31, 32, 28, 41, 42, 0,
  /* 27 */ 33, 27, 31, 32, 28, 41, 42, 34, 43, 45, 46, 44, 47, 48, 0,
  /* 42 */ 26, 29, 33, 27, 31, 32, 28, 41, 42, 34, 43, 45, 46, 44, 47, 48, 30, 53, 49, 51, 52, 50, 55, 56, 54, 57, 59, 60, 58, 61, 62, 0,
  /* 74 */ 3, 12, 7, 1, 4, 65, 64, 63, 0,
  /* 83 */ 3, 12, 7, 1, 4, 65, 64, 63, 5, 71, 70, 69, 110, 112, 0,
  /* 98 */ 3, 12, 7, 1, 4, 65, 64, 63, 5, 71, 70, 69, 6, 68, 67, 66, 110, 111, 112, 113, 114, 0,
  /* 120 */ 8, 2, 12, 7, 1, 9, 73, 75, 74, 72, 115, 0,
  /* 132 */ 8, 2, 12, 7, 1, 9, 73, 75, 74, 72, 10, 81, 83, 82, 80, 115, 117, 118, 120, 123, 0,
  /* 153 */ 8, 2, 12, 7, 1, 9, 73, 75, 74, 72, 10, 81, 83, 82, 80, 11, 77, 79, 78, 76, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 0,
  /* 185 */ 18, 13, 19, 90, 20, 89, 21, 88, 22, 87, 23, 86, 24, 85, 25, 84, 126, 127, 128, 129, 130, 131, 132, 133, 0,
  /* 210 */ 15, 13, 17, 91, 134, 0,
  /* 216 */ 36, 35, 2, 12, 7, 1, 40, 37, 96, 93, 95, 94, 92, 97, 135, 136, 0,
  /* 233 */ 36, 35, 2, 12, 7, 1, 40, 37, 96, 93, 95, 94, 92, 97, 38, 108, 105, 107, 106, 104, 109, 135, 136, 138, 140, 141, 143, 146, 148, 0,
  /* 263 */ 36, 35, 2, 12, 7, 1, 40, 37, 96, 93, 95, 94, 92, 97, 38, 108, 105, 107, 106, 104, 109, 39, 102, 99, 101, 100, 98, 103, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 0,
};

static const MCRegisterDesc AArch64RegDesc[] = {
  { 3, 0, 0, 0, 0, 0 },
  { 2785, 16, 16, 4, 29537, 0 },
  { 2778, 1206, 1814, 5, 29537, 27 },
  { 2789, 1206, 1337, 5, 29537, 27 },
  { 2807, 16, 16, 4, 29537, 0 },
  { 2782, 690, 16, 5, 29537, 27 },
  { 2533, 16, 16, 4, 29537, 0 },
  { 2781, 16, 1821, 4, 29106, 0 },
  { 2796, 16, 754, 4, 11168, 0 },
  { 2803, 1846, 1335, 5, 11168, 27 },
  { 2530, 700, 16, 42, 0, 76 },
  { 203, 16, 497, 4, 12033, 0 },
  { 538, 16, 579, 4, 12033, 0 },
  { 761, 16, 776, 4, 12033, 0 },
  { 1036, 16, 433, 4, 12033, 0 },
  { 1254, 16, 433, 4, 12033, 0 },
  { 1526, 16, 433, 4, 12033, 0 },
  { 1732, 16, 433, 4, 12033, 0 },
  { 1996, 16, 433, 4, 12033, 0 },
  { 2220, 16, 433, 4, 12033, 0 },
  { 2447, 16, 433, 4, 12033, 0 },
  { 0, 16, 433, 4, 12033, 0 },
  { 280, 16, 433, 4, 12033, 0 },
  { 618, 16, 433, 4, 12033, 0 },
  { 830, 16, 433, 4, 12033, 0 },
  { 1108, 16, 433, 4, 12033, 0 },
  { 1318, 16, 433, 4, 12033, 0 },
  { 1596, 16, 433, 4, 12033, 0 },
  { 1796, 16, 433, 4, 12033, 0 },
  { 2084, 16, 433, 4, 12033, 0 },
  { 2279, 16, 433, 4, 12033, 0 },
  { 69, 16, 433, 4, 12033, 0 },
  { 378, 16, 433, 4, 12033, 0 },
  { 693, 16, 433, 4, 12033, 0 },
  { 936, 16, 433, 4, 12033, 0 },
  { 1186, 16, 433, 4, 12033, 0 },
  { 1426, 16, 433, 4, 12033, 0 },
  { 1664, 16, 433, 4, 12033, 0 },
  { 1896, 16, 433, 4, 12033, 0 },
  { 2152, 16, 433, 4, 12033, 0 },
  { 2379, 16, 433, 4, 12033, 0 },
  { 137, 16, 433, 4, 12033, 0 },
  { 478, 16, 433, 4, 12033, 0 },
  { 208, 1588, 500, 1, 29073, 3 },
  { 543, 1588, 582, 1, 29073, 3 },
  { 766, 1588, 779, 1, 29073, 3 },
  { 1041, 1588, 436, 1, 29073, 3 },
  { 1259, 1588, 436, 1, 29073, 3 },
  { 1531, 1588, 436, 1, 29073, 3 },
  { 1737, 1588, 436, 1, 29073, 3 },
  { 2001, 1588, 436, 1, 29073, 3 },
  { 2232, 1588, 436, 1, 29073, 3 },
  { 2459, 1588, 436, 1, 29073, 3 },
  { 13, 1588, 436, 1, 29073, 3 },
  { 294, 1588, 436, 1, 29073, 3 },
  { 633, 1588, 436, 1, 29073, 3 },
  { 846, 1588, 436, 1, 29073, 3 },
  { 1124, 1588, 436, 1, 29073, 3 },
  { 1334, 1588, 436, 1, 29073, 3 },
  { 1612, 1588, 436, 1, 29073, 3 },
  { 1812, 1588, 436, 1, 29073, 3 },
  { 2100, 1588, 436, 1, 29073, 3 },
  { 2295, 1588, 436, 1, 29073, 3 },
  { 85, 1588, 436, 1, 29073, 3 },
  { 394, 1588, 436, 1, 29073, 3 },
  { 709, 1588, 436, 1, 29073, 3 },
  { 952, 1588, 436, 1, 29073, 3 },
  { 1202, 1588, 436, 1, 29073, 3 },
  { 1442, 1588, 436, 1, 29073, 3 },
  { 1680, 1588, 436, 1, 29073, 3 },
  { 1912, 1588, 436, 1, 29073, 3 },
  { 2168, 1588, 436, 1, 29073, 3 },
  { 2395, 1588, 436, 1, 29073, 3 },
  { 153, 1588, 436, 1, 29073, 3 },
  { 494, 1588, 436, 1, 29073, 3 },
  { 228, 1590, 498, 3, 28289, 3 },
  { 562, 1590, 580, 3, 28289, 3 },
  { 782, 1590, 777, 3, 28289, 3 },
  { 1056, 1590, 434, 3, 28289, 3 },
  { 1274, 1590, 434, 3, 28289, 3 },
  { 1546, 1590, 434, 3, 28289, 3 },
  { 1752, 1590, 434, 3, 28289, 3 },
  { 2016, 1590, 434, 3, 28289, 3 },
  { 2235, 1590, 434, 3, 28289, 3 },
  { 2462, 1590, 434, 3, 28289, 3 },
  { 17, 1590, 434, 3, 28289, 3 },
  { 298, 1590, 434, 3, 28289, 3 },
  { 637, 1590, 434, 3, 28289, 3 },
  { 850, 1590, 434, 3, 28289, 3 },
  { 1128, 1590, 434, 3, 28289, 3 },
  { 1338, 1590, 434, 3, 28289, 3 },
  { 1616, 1590, 434, 3, 28289, 3 },
  { 1816, 1590, 434, 3, 28289, 3 },
  { 2104, 1590, 434, 3, 28289, 3 },
  { 2299, 1590, 434, 3, 28289, 3 },
  { 89, 1590, 434, 3, 28289, 3 },
  { 398, 1590, 434, 3, 28289, 3 },
  { 713, 1590, 434, 3, 28289, 3 },
  { 956, 1590, 434, 3, 28289, 3 },
  { 1206, 1590, 434, 3, 28289, 3 },
  { 1446, 1590, 434, 3, 28289, 3 },
  { 1684, 1590, 434, 3, 28289, 3 },
  { 1916, 1590, 434, 3, 28289, 3 },
  { 2172, 1590, 434, 3, 28289, 3 },
  { 2399, 1590, 434, 3, 28289, 3 },
  { 157, 1590, 434, 3, 28289, 3 },
  { 498, 1590, 434, 3, 28289, 3 },
  { 231, 16, 16, 4, 28289, 0 },
  { 565, 16, 16, 4, 28289, 0 },
  { 785, 16, 16, 4, 28289, 0 },
  { 1059, 16, 16, 4, 28289, 0 },
  { 1277, 16, 16, 4, 28289, 0 },
  { 1549, 16, 16, 4, 28289, 0 },
  { 1755, 16, 16, 4, 28289, 0 },
  { 2019, 16, 16, 4, 28289, 0 },
  { 2238, 16, 16, 4, 28289, 0 },
  { 2465, 16, 16, 4, 28289, 0 },
  { 21, 16, 16, 4, 28289, 0 },
  { 302, 16, 16, 4, 28289, 0 },
  { 641, 16, 16, 4, 28289, 0 },
  { 854, 16, 16, 4, 28289, 0 },
  { 1132, 16, 16, 4, 28289, 0 },
  { 1342, 16, 16, 4, 28289, 0 },
  { 236, 1601, 530, 0, 23425, 3 },
  { 570, 1601, 612, 0, 23425, 3 },
  { 790, 1601, 809, 0, 23425, 3 },
  { 1064, 1601, 466, 0, 23425, 3 },
  { 1282, 1601, 466, 0, 23425, 3 },
  { 1554, 1601, 466, 0, 23425, 3 },
  { 1760, 1601, 466, 0, 23425, 3 },
  { 2024, 1601, 466, 0, 23425, 3 },
  { 2243, 1601, 466, 0, 23425, 3 },
  { 2470, 1601, 466, 0, 23425, 3 },
  { 27, 1601, 466, 0, 23425, 3 },
  { 308, 1601, 466, 0, 23425, 3 },
  { 647, 1601, 466, 0, 23425, 3 },
  { 860, 1601, 466, 0, 23425, 3 },
  { 1138, 1601, 466, 0, 23425, 3 },
  { 1348, 1601, 466, 0, 23425, 3 },
  { 1632, 1601, 466, 0, 23425, 3 },
  { 1832, 1601, 466, 0, 23425, 3 },
  { 2120, 1601, 466, 0, 23425, 3 },
  { 2315, 1601, 466, 0, 23425, 3 },
  { 105, 1601, 466, 0, 23425, 3 },
  { 414, 1601, 466, 0, 23425, 3 },
  { 729, 1601, 466, 0, 23425, 3 },
  { 972, 1601, 466, 0, 23425, 3 },
  { 1222, 1601, 466, 0, 23425, 3 },
  { 1462, 1601, 466, 0, 23425, 3 },
  { 1700, 1601, 466, 0, 23425, 3 },
  { 1932, 1601, 466, 0, 23425, 3 },
  { 2188, 1601, 466, 0, 23425, 3 },
  { 2415, 1601, 466, 0, 23425, 3 },
  { 173, 1601, 466, 0, 23425, 3 },
  { 514, 1601, 466, 0, 23425, 3 },
  { 256, 1589, 499, 2, 23361, 3 },
  { 589, 1589, 581, 2, 23361, 3 },
  { 808, 1589, 778, 2, 23361, 3 },
  { 1081, 1589, 435, 2, 23361, 3 },
  { 1297, 1589, 435, 2, 23361, 3 },
  { 1569, 1589, 435, 2, 23361, 3 },
  { 1775, 1589, 435, 2, 23361, 3 },
  { 2039, 1589, 435, 2, 23361, 3 },
  { 2258, 1589, 435, 2, 23361, 3 },
  { 2485, 1589, 435, 2, 23361, 3 },
  { 44, 1589, 435, 2, 23361, 3 },
  { 326, 1589, 435, 2, 23361, 3 },
  { 666, 1589, 435, 2, 23361, 3 },
  { 880, 1589, 435, 2, 23361, 3 },
  { 1158, 1589, 435, 2, 23361, 3 },
  { 1368, 1589, 435, 2, 23361, 3 },
  { 1636, 1589, 435, 2, 23361, 3 },
  { 1836, 1589, 435, 2, 23361, 3 },
  { 2124, 1589, 435, 2, 23361, 3 },
  { 2319, 1589, 435, 2, 23361, 3 },
  { 109, 1589, 435, 2, 23361, 3 },
  { 418, 1589, 435, 2, 23361, 3 },
  { 733, 1589, 435, 2, 23361, 3 },
  { 976, 1589, 435, 2, 23361, 3 },
  { 1226, 1589, 435, 2, 23361, 3 },
  { 1466, 1589, 435, 2, 23361, 3 },
  { 1704, 1589, 435, 2, 23361, 3 },
  { 1936, 1589, 435, 2, 23361, 3 },
  { 2192, 1589, 435, 2, 23361, 3 },
  { 2419, 1589, 435, 2, 23361, 3 },
  { 177, 1589, 435, 2, 23361, 3 },
  { 518, 1589, 435, 2, 23361, 3 },
  { 259, 16, 1800, 4, 23393, 0 },
  { 595, 16, 1795, 4, 23393, 0 },
  { 811, 16, 676, 4, 23393, 0 },
  { 1087, 16, 670, 4, 23393, 0 },
  { 1300, 16, 263, 4, 23393, 0 },
  { 1575, 16, 256, 4, 23393, 0 },
  { 1778, 16, 137, 4, 23393, 0 },
  { 2045, 16, 129, 4, 23393, 0 },
  { 2261, 16, 129, 4, 23393, 0 },
  { 2491, 16, 121, 4, 23393, 0 },
  { 48, 16, 121, 4, 23393, 0 },
  { 334, 16, 113, 4, 23393, 0 },
  { 670, 16, 113, 4, 23393, 0 },
  { 888, 16, 105, 4, 23393, 0 },
  { 1162, 16, 105, 4, 23393, 0 },
  { 1376, 16, 97, 4, 23393, 0 },
  { 1640, 16, 97, 4, 23393, 0 },
  { 1844, 16, 89, 4, 23393, 0 },
  { 2128, 16, 89, 4, 23393, 0 },
  { 2327, 16, 81, 4, 23393, 0 },
  { 113, 16, 81, 4, 23393, 0 },
  { 426, 16, 73, 4, 23393, 0 },
  { 737, 16, 207, 4, 23393, 0 },
  { 984, 16, 199, 4, 23393, 0 },
  { 1230, 16, 287, 4, 23393, 0 },
  { 1474, 16, 280, 4, 23393, 0 },
  { 1708, 16, 738, 4, 23393, 0 },
  { 1944, 16, 732, 4, 23393, 0 },
  { 2196, 16, 1785, 4, 23393, 0 },
  { 2427, 16, 1790, 4, 23217, 0 },
  { 181, 16, 758, 4, 23217, 0 },
  { 262, 1783, 1811, 5, 23329, 27 },
  { 601, 1783, 1808, 5, 23329, 27 },
  { 814, 1783, 686, 5, 23329, 27 },
  { 1093, 1783, 682, 5, 23329, 27 },
  { 1303, 1783, 275, 5, 23329, 27 },
  { 1581, 1783, 270, 5, 23329, 27 },
  { 1781, 1783, 193, 5, 23329, 27 },
  { 2069, 1783, 187, 5, 23329, 27 },
  { 2264, 1783, 187, 5, 23329, 27 },
  { 2515, 1783, 181, 5, 23329, 27 },
  { 52, 1783, 181, 5, 23329, 27 },
  { 360, 1783, 175, 5, 23329, 27 },
  { 674, 1783, 175, 5, 23329, 27 },
  { 916, 1783, 169, 5, 23329, 27 },
  { 1166, 1783, 169, 5, 23329, 27 },
  { 1406, 1783, 163, 5, 23329, 27 },
  { 1644, 1783, 163, 5, 23329, 27 },
  { 1876, 1783, 157, 5, 23329, 27 },
  { 2132, 1783, 157, 5, 23329, 27 },
  { 2359, 1783, 151, 5, 23329, 27 },
  { 117, 1783, 151, 5, 23329, 27 },
  { 458, 1783, 145, 5, 23329, 27 },
  { 741, 1783, 221, 5, 23329, 27 },
  { 1016, 1783, 215, 5, 23329, 27 },
  { 1234, 1783, 299, 5, 23329, 27 },
  { 1506, 1783, 294, 5, 23329, 27 },
  { 1712, 1783, 748, 5, 23329, 27 },
  { 1976, 1783, 744, 5, 23329, 27 },
  { 2200, 1783, 1805, 5, 23329, 27 },
  { 277, 1208, 551, 13, 14177, 38 },
  { 615, 1208, 633, 13, 14177, 38 },
  { 827, 1208, 830, 13, 14177, 38 },
  { 1105, 1208, 487, 13, 14177, 38 },
  { 1315, 1208, 487, 13, 14177, 38 },
  { 1593, 1208, 487, 13, 14177, 38 },
  { 1793, 1208, 487, 13, 14177, 38 },
  { 2081, 1208, 487, 13, 14177, 38 },
  { 2276, 1208, 487, 13, 14177, 38 },
  { 2527, 1208, 487, 13, 14177, 38 },
  { 65, 1208, 487, 13, 14177, 38 },
  { 374, 1208, 487, 13, 14177, 38 },
  { 689, 1208, 487, 13, 14177, 38 },
  { 932, 1208, 487, 13, 14177, 38 },
  { 1182, 1208, 487, 13, 14177, 38 },
  { 1422, 1208, 487, 13, 14177, 38 },
  { 1660, 1208, 487, 13, 14177, 38 },
  { 1892, 1208, 487, 13, 14177, 38 },
  { 2148, 1208, 487, 13, 14177, 38 },
  { 2375, 1208, 487, 13, 14177, 38 },
  { 133, 1208, 487, 13, 14177, 38 },
  { 474, 1208, 487, 13, 14177, 38 },
  { 757, 1208, 487, 13, 14177, 38 },
  { 1032, 1208, 487, 13, 14177, 38 },
  { 1250, 1208, 487, 13, 14177, 38 },
  { 1522, 1208, 487, 13, 14177, 38 },
  { 1728, 1208, 487, 13, 14177, 38 },
  { 1992, 1208, 487, 13, 14177, 38 },
  { 2216, 1208, 487, 13, 14177, 38 },
  { 2443, 1208, 487, 13, 14177, 38 },
  { 197, 1208, 487, 13, 14177, 38 },
  { 534, 1208, 487, 13, 14177, 38 },
  { 201, 701, 1359, 43, 0, 76 },
  { 206, 729, 1440, 10, 5976, 35 },
  { 541, 729, 1392, 10, 5976, 35 },
  { 764, 729, 1416, 10, 5928, 35 },
  { 1039, 729, 1368, 10, 5928, 35 },
  { 1257, 729, 1428, 10, 5880, 35 },
  { 1529, 729, 1380, 10, 5880, 35 },
  { 1735, 729, 1404, 10, 5832, 35 },
  { 1999, 729, 1356, 10, 5832, 35 },
  { 226, 717, 1406, 27, 280, 67 },
  { 560, 717, 1358, 27, 280, 67 },
  { 234, 16, 1445, 4, 21656, 0 },
  { 568, 16, 1397, 4, 21656, 0 },
  { 788, 16, 1421, 4, 21624, 0 },
  { 1062, 16, 1373, 4, 21624, 0 },
  { 1280, 16, 1433, 4, 21592, 0 },
  { 1552, 16, 1385, 4, 21592, 0 },
  { 1758, 16, 1409, 4, 21560, 0 },
  { 2022, 16, 1361, 4, 21560, 0 },
  { 2241, 16, 1439, 4, 21528, 0 },
  { 2468, 16, 1391, 4, 21528, 0 },
  { 25, 16, 1415, 4, 21496, 0 },
  { 306, 16, 1367, 4, 21496, 0 },
  { 645, 16, 1427, 4, 21464, 0 },
  { 858, 16, 1379, 4, 21464, 0 },
  { 1136, 16, 1403, 4, 21432, 0 },
  { 1346, 16, 1355, 4, 21432, 0 },
  { 254, 725, 1429, 20, 936, 62 },
  { 587, 725, 1381, 20, 936, 62 },
  { 806, 725, 1405, 20, 856, 62 },
  { 1079, 725, 1357, 20, 856, 62 },
  { 2557, 16, 550, 4, 23249, 0 },
  { 2584, 16, 632, 4, 23249, 0 },
  { 2604, 16, 829, 4, 23249, 0 },
  { 2624, 16, 486, 4, 23249, 0 },
  { 2644, 16, 486, 4, 23249, 0 },
  { 2664, 16, 486, 4, 23249, 0 },
  { 2684, 16, 486, 4, 23249, 0 },
  { 2704, 16, 486, 4, 23249, 0 },
  { 2724, 16, 486, 4, 23249, 0 },
  { 2744, 16, 486, 4, 23249, 0 },
  { 2536, 16, 486, 4, 23249, 0 },
  { 2563, 16, 486, 4, 23249, 0 },
  { 2590, 16, 486, 4, 23249, 0 },
  { 2610, 16, 486, 4, 23249, 0 },
  { 2630, 16, 486, 4, 23249, 0 },
  { 2650, 16, 486, 4, 23249, 0 },
  { 2670, 16, 486, 4, 23249, 0 },
  { 2690, 16, 486, 4, 23249, 0 },
  { 2710, 16, 486, 4, 23249, 0 },
  { 2730, 16, 486, 4, 23249, 0 },
  { 2543, 16, 486, 4, 23249, 0 },
  { 2570, 16, 486, 4, 23249, 0 },
  { 2597, 16, 486, 4, 23249, 0 },
  { 2617, 16, 486, 4, 23249, 0 },
  { 2637, 16, 486, 4, 23249, 0 },
  { 2657, 16, 486, 4, 23249, 0 },
  { 2677, 16, 486, 4, 23249, 0 },
  { 2697, 16, 486, 4, 23249, 0 },
  { 2717, 16, 486, 4, 23249, 0 },
  { 2737, 16, 486, 4, 23249, 0 },
  { 2550, 16, 486, 4, 23249, 0 },
  { 2577, 16, 486, 4, 23249, 0 },
  { 554, 1592, 643, 74, 6881, 95 },
  { 776, 1592, 851, 74, 6881, 95 },
  { 1050, 1592, 561, 74, 6881, 95 },
  { 1268, 1592, 561, 74, 6881, 95 },
  { 1540, 1592, 561, 74, 6881, 95 },
  { 1746, 1592, 561, 74, 6881, 95 },
  { 2010, 1592, 561, 74, 6881, 95 },
  { 2229, 1592, 561, 74, 6881, 95 },
  { 2456, 1592, 561, 74, 6881, 95 },
  { 10, 1592, 561, 74, 6881, 95 },
  { 290, 1592, 561, 74, 6881, 95 },
  { 629, 1592, 561, 74, 6881, 95 },
  { 842, 1592, 561, 74, 6881, 95 },
  { 1120, 1592, 561, 74, 6881, 95 },
  { 1330, 1592, 561, 74, 6881, 95 },
  { 1608, 1592, 561, 74, 6881, 95 },
  { 1808, 1592, 561, 74, 6881, 95 },
  { 2096, 1592, 561, 74, 6881, 95 },
  { 2291, 1592, 561, 74, 6881, 95 },
  { 81, 1592, 561, 74, 6881, 95 },
  { 390, 1592, 561, 74, 6881, 95 },
  { 705, 1592, 561, 74, 6881, 95 },
  { 948, 1592, 561, 74, 6881, 95 },
  { 1198, 1592, 561, 74, 6881, 95 },
  { 1438, 1592, 561, 74, 6881, 95 },
  { 1676, 1592, 561, 74, 6881, 95 },
  { 1908, 1592, 561, 74, 6881, 95 },
  { 2164, 1592, 561, 74, 6881, 95 },
  { 2391, 1592, 561, 74, 6881, 95 },
  { 149, 1592, 561, 74, 6881, 95 },
  { 490, 1592, 561, 74, 6881, 95 },
  { 219, 1583, 561, 74, 13904, 2 },
  { 1044, 1724, 1203, 98, 1089, 102 },
  { 1262, 1724, 1203, 98, 1089, 102 },
  { 1534, 1724, 1203, 98, 1089, 102 },
  { 1740, 1724, 1203, 98, 1089, 102 },
  { 2004, 1724, 1203, 98, 1089, 102 },
  { 2223, 1724, 1203, 98, 1089, 102 },
  { 2450, 1724, 1203, 98, 1089, 102 },
  { 4, 1724, 1203, 98, 1089, 102 },
  { 284, 1724, 1203, 98, 1089, 102 },
  { 622, 1724, 1203, 98, 1089, 102 },
  { 834, 1724, 1203, 98, 1089, 102 },
  { 1112, 1724, 1203, 98, 1089, 102 },
  { 1322, 1724, 1203, 98, 1089, 102 },
  { 1600, 1724, 1203, 98, 1089, 102 },
  { 1800, 1724, 1203, 98, 1089, 102 },
  { 2088, 1724, 1203, 98, 1089, 102 },
  { 2283, 1724, 1203, 98, 1089, 102 },
  { 73, 1724, 1203, 98, 1089, 102 },
  { 382, 1724, 1203, 98, 1089, 102 },
  { 697, 1724, 1203, 98, 1089, 102 },
  { 940, 1724, 1203, 98, 1089, 102 },
  { 1190, 1724, 1203, 98, 1089, 102 },
  { 1430, 1724, 1203, 98, 1089, 102 },
  { 1668, 1724, 1203, 98, 1089, 102 },
  { 1900, 1724, 1203, 98, 1089, 102 },
  { 2156, 1724, 1203, 98, 1089, 102 },
  { 2383, 1724, 1203, 98, 1089, 102 },
  { 141, 1724, 1203, 98, 1089, 102 },
  { 482, 1724, 1203, 98, 1089, 102 },
  { 211, 1746, 1203, 98, 3632, 107 },
  { 546, 1561, 1203, 98, 4864, 93 },
  { 769, 1702, 1203, 98, 12192, 5 },
  { 773, 334, 877, 83, 4033, 108 },
  { 1047, 334, 661, 83, 4033, 108 },
  { 1265, 334, 661, 83, 4033, 108 },
  { 1537, 334, 661, 83, 4033, 108 },
  { 1743, 334, 661, 83, 4033, 108 },
  { 2007, 334, 661, 83, 4033, 108 },
  { 2226, 334, 661, 83, 4033, 108 },
  { 2453, 334, 661, 83, 4033, 108 },
  { 7, 334, 661, 83, 4033, 108 },
  { 287, 334, 661, 83, 4033, 108 },
  { 625, 334, 661, 83, 4033, 108 },
  { 838, 334, 661, 83, 4033, 108 },
  { 1116, 334, 661, 83, 4033, 108 },
  { 1326, 334, 661, 83, 4033, 108 },
  { 1604, 334, 661, 83, 4033, 108 },
  { 1804, 334, 661, 83, 4033, 108 },
  { 2092, 334, 661, 83, 4033, 108 },
  { 2287, 334, 661, 83, 4033, 108 },
  { 77, 334, 661, 83, 4033, 108 },
  { 386, 334, 661, 83, 4033, 108 },
  { 701, 334, 661, 83, 4033, 108 },
  { 944, 334, 661, 83, 4033, 108 },
  { 1194, 334, 661, 83, 4033, 108 },
  { 1434, 334, 661, 83, 4033, 108 },
  { 1672, 334, 661, 83, 4033, 108 },
  { 1904, 334, 661, 83, 4033, 108 },
  { 2160, 334, 661, 83, 4033, 108 },
  { 2387, 334, 661, 83, 4033, 108 },
  { 145, 334, 661, 83, 4033, 108 },
  { 486, 334, 661, 83, 4033, 108 },
  { 215, 349, 661, 83, 5088, 98 },
  { 550, 1770, 661, 83, 13440, 10 },
  { 581, 1215, 649, 120, 6113, 114 },
  { 800, 1215, 857, 120, 6113, 114 },
  { 1073, 1215, 567, 120, 6113, 114 },
  { 1291, 1215, 567, 120, 6113, 114 },
  { 1563, 1215, 567, 120, 6113, 114 },
  { 1769, 1215, 567, 120, 6113, 114 },
  { 2033, 1215, 567, 120, 6113, 114 },
  { 2252, 1215, 567, 120, 6113, 114 },
  { 2479, 1215, 567, 120, 6113, 114 },
  { 37, 1215, 567, 120, 6113, 114 },
  { 318, 1215, 567, 120, 6113, 114 },
  { 658, 1215, 567, 120, 6113, 114 },
  { 872, 1215, 567, 120, 6113, 114 },
  { 1150, 1215, 567, 120, 6113, 114 },
  { 1360, 1215, 567, 120, 6113, 114 },
  { 1628, 1215, 567, 120, 6113, 114 },
  { 1828, 1215, 567, 120, 6113, 114 },
  { 2116, 1215, 567, 120, 6113, 114 },
  { 2311, 1215, 567, 120, 6113, 114 },
  { 101, 1215, 567, 120, 6113, 114 },
  { 410, 1215, 567, 120, 6113, 114 },
  { 725, 1215, 567, 120, 6113, 114 },
  { 968, 1215, 567, 120, 6113, 114 },
  { 1218, 1215, 567, 120, 6113, 114 },
  { 1458, 1215, 567, 120, 6113, 114 },
  { 1696, 1215, 567, 120, 6113, 114 },
  { 1928, 1215, 567, 120, 6113, 114 },
  { 2184, 1215, 567, 120, 6113, 114 },
  { 2411, 1215, 567, 120, 6113, 114 },
  { 169, 1215, 567, 120, 6113, 114 },
  { 510, 1215, 567, 120, 6113, 114 },
  { 247, 1317, 567, 120, 13904, 14 },
  { 1067, 1638, 1204, 153, 1009, 121 },
  { 1285, 1638, 1204, 153, 1009, 121 },
  { 1557, 1638, 1204, 153, 1009, 121 },
  { 1763, 1638, 1204, 153, 1009, 121 },
  { 2027, 1638, 1204, 153, 1009, 121 },
  { 2246, 1638, 1204, 153, 1009, 121 },
  { 2473, 1638, 1204, 153, 1009, 121 },
  { 31, 1638, 1204, 153, 1009, 121 },
  { 312, 1638, 1204, 153, 1009, 121 },
  { 651, 1638, 1204, 153, 1009, 121 },
  { 864, 1638, 1204, 153, 1009, 121 },
  { 1142, 1638, 1204, 153, 1009, 121 },
  { 1352, 1638, 1204, 153, 1009, 121 },
  { 1620, 1638, 1204, 153, 1009, 121 },
  { 1820, 1638, 1204, 153, 1009, 121 },
  { 2108, 1638, 1204, 153, 1009, 121 },
  { 2303, 1638, 1204, 153, 1009, 121 },
  { 93, 1638, 1204, 153, 1009, 121 },
  { 402, 1638, 1204, 153, 1009, 121 },
  { 717, 1638, 1204, 153, 1009, 121 },
  { 960, 1638, 1204, 153, 1009, 121 },
  { 1210, 1638, 1204, 153, 1009, 121 },
  { 1450, 1638, 1204, 153, 1009, 121 },
  { 1688, 1638, 1204, 153, 1009, 121 },
  { 1920, 1638, 1204, 153, 1009, 121 },
  { 2176, 1638, 1204, 153, 1009, 121 },
  { 2403, 1638, 1204, 153, 1009, 121 },
  { 161, 1638, 1204, 153, 1009, 121 },
  { 502, 1638, 1204, 153, 1009, 121 },
  { 239, 1670, 1204, 153, 3632, 126 },
  { 573, 1529, 1204, 153, 4864, 112 },
  { 793, 1606, 1204, 153, 12192, 17 },
  { 797, 1466, 880, 132, 3969, 127 },
  { 1070, 1466, 664, 132, 3969, 127 },
  { 1288, 1466, 664, 132, 3969, 127 },
  { 1560, 1466, 664, 132, 3969, 127 },
  { 1766, 1466, 664, 132, 3969, 127 },
  { 2030, 1466, 664, 132, 3969, 127 },
  { 2249, 1466, 664, 132, 3969, 127 },
  { 2476, 1466, 664, 132, 3969, 127 },
  { 34, 1466, 664, 132, 3969, 127 },
  { 315, 1466, 664, 132, 3969, 127 },
  { 654, 1466, 664, 132, 3969, 127 },
  { 868, 1466, 664, 132, 3969, 127 },
  { 1146, 1466, 664, 132, 3969, 127 },
  { 1356, 1466, 664, 132, 3969, 127 },
  { 1624, 1466, 664, 132, 3969, 127 },
  { 1824, 1466, 664, 132, 3969, 127 },
  { 2112, 1466, 664, 132, 3969, 127 },
  { 2307, 1466, 664, 132, 3969, 127 },
  { 97, 1466, 664, 132, 3969, 127 },
  { 406, 1466, 664, 132, 3969, 127 },
  { 721, 1466, 664, 132, 3969, 127 },
  { 964, 1466, 664, 132, 3969, 127 },
  { 1214, 1466, 664, 132, 3969, 127 },
  { 1454, 1466, 664, 132, 3969, 127 },
  { 1692, 1466, 664, 132, 3969, 127 },
  { 1924, 1466, 664, 132, 3969, 127 },
  { 2180, 1466, 664, 132, 3969, 127 },
  { 2407, 1466, 664, 132, 3969, 127 },
  { 165, 1466, 664, 132, 3969, 127 },
  { 506, 1466, 664, 132, 3969, 127 },
  { 243, 1487, 664, 132, 5088, 117 },
  { 577, 1508, 664, 132, 13440, 22 },
  { 2750, 2098, 16, 185, 560, 140 },
  { 2048, 2073, 16, 185, 418, 131 },
  { 2494, 2048, 16, 185, 418, 131 },
  { 338, 2023, 16, 185, 418, 131 },
  { 892, 1998, 16, 185, 418, 131 },
  { 1380, 1973, 16, 185, 418, 131 },
  { 1848, 1948, 16, 185, 418, 131 },
  { 2331, 1923, 16, 185, 418, 131 },
  { 430, 1898, 16, 185, 418, 131 },
  { 988, 1873, 16, 185, 418, 131 },
  { 1478, 1848, 16, 185, 418, 131 },
  { 1948, 1823, 16, 185, 418, 131 },
  { 2792, 1455, 756, 7, 11072, 32 },
  { 592, 385, 1797, 7, 6066, 32 },
  { 1084, 388, 672, 7, 6066, 32 },
  { 1572, 391, 258, 7, 6066, 32 },
  { 2042, 394, 75, 7, 6066, 32 },
  { 2488, 397, 75, 7, 6066, 32 },
  { 330, 400, 75, 7, 6066, 32 },
  { 884, 403, 75, 7, 6066, 32 },
  { 1372, 406, 75, 7, 6066, 32 },
  { 1840, 409, 75, 7, 6066, 32 },
  { 2323, 412, 75, 7, 6066, 32 },
  { 422, 415, 75, 7, 6066, 32 },
  { 980, 418, 201, 7, 6066, 32 },
  { 1470, 421, 282, 7, 6066, 32 },
  { 1940, 424, 734, 7, 6066, 32 },
  { 2423, 427, 1787, 7, 19201, 29 },
  { 2800, 1329, 16, 210, 11121, 149 },
  { 2774, 1227, 1788, 210, 19152, 26 },
  { 598, 1311, 1788, 210, 6018, 149 },
  { 1090, 1305, 673, 210, 6018, 149 },
  { 1578, 1299, 259, 210, 6018, 149 },
  { 2066, 1293, 76, 210, 6018, 149 },
  { 2512, 1287, 76, 210, 6018, 149 },
  { 356, 1281, 76, 210, 6018, 149 },
  { 912, 1275, 76, 210, 6018, 149 },
  { 1402, 1269, 76, 210, 6018, 149 },
  { 1872, 1263, 76, 210, 6018, 149 },
  { 2355, 1257, 76, 210, 6018, 149 },
  { 454, 1251, 76, 210, 6018, 149 },
  { 1012, 1245, 202, 210, 6018, 149 },
  { 1502, 1239, 283, 210, 6018, 149 },
  { 1972, 1233, 735, 210, 6018, 149 },
  { 612, 889, 655, 216, 5265, 152 },
  { 824, 889, 863, 216, 5265, 152 },
  { 1102, 889, 573, 216, 5265, 152 },
  { 1312, 889, 573, 216, 5265, 152 },
  { 1590, 889, 573, 216, 5265, 152 },
  { 1790, 889, 573, 216, 5265, 152 },
  { 2078, 889, 573, 216, 5265, 152 },
  { 2273, 889, 573, 216, 5265, 152 },
  { 2524, 889, 573, 216, 5265, 152 },
  { 62, 889, 573, 216, 5265, 152 },
  { 370, 889, 573, 216, 5265, 152 },
  { 685, 889, 573, 216, 5265, 152 },
  { 928, 889, 573, 216, 5265, 152 },
  { 1178, 889, 573, 216, 5265, 152 },
  { 1418, 889, 573, 216, 5265, 152 },
  { 1656, 889, 573, 216, 5265, 152 },
  { 1888, 889, 573, 216, 5265, 152 },
  { 2144, 889, 573, 216, 5265, 152 },
  { 2371, 889, 573, 216, 5265, 152 },
  { 129, 889, 573, 216, 5265, 152 },
  { 470, 889, 573, 216, 5265, 152 },
  { 753, 889, 573, 216, 5265, 152 },
  { 1028, 889, 573, 216, 5265, 152 },
  { 1246, 889, 573, 216, 5265, 152 },
  { 1518, 889, 573, 216, 5265, 152 },
  { 1724, 889, 573, 216, 5265, 152 },
  { 1988, 889, 573, 216, 5265, 152 },
  { 2212, 889, 573, 216, 5265, 152 },
  { 2439, 889, 573, 216, 5265, 152 },
  { 193, 889, 573, 216, 5265, 152 },
  { 530, 889, 573, 216, 5265, 152 },
  { 273, 906, 573, 216, 13952, 41 },
  { 1096, 1105, 16, 263, 705, 173 },
  { 1306, 1105, 16, 263, 705, 173 },
  { 1584, 1105, 16, 263, 705, 173 },
  { 1784, 1105, 16, 263, 705, 173 },
  { 2072, 1105, 16, 263, 705, 173 },
  { 2267, 1105, 16, 263, 705, 173 },
  { 2518, 1105, 16, 263, 705, 173 },
  { 56, 1105, 16, 263, 705, 173 },
  { 364, 1105, 16, 263, 705, 173 },
  { 678, 1105, 16, 263, 705, 173 },
  { 920, 1105, 16, 263, 705, 173 },
  { 1170, 1105, 16, 263, 705, 173 },
  { 1410, 1105, 16, 263, 705, 173 },
  { 1648, 1105, 16, 263, 705, 173 },
  { 1880, 1105, 16, 263, 705, 173 },
  { 2136, 1105, 16, 263, 705, 173 },
  { 2363, 1105, 16, 263, 705, 173 },
  { 121, 1105, 16, 263, 705, 173 },
  { 462, 1105, 16, 263, 705, 173 },
  { 745, 1105, 16, 263, 705, 173 },
  { 1020, 1105, 16, 263, 705, 173 },
  { 1238, 1105, 16, 263, 705, 173 },
  { 1510, 1105, 16, 263, 705, 173 },
  { 1716, 1105, 16, 263, 705, 173 },
  { 1980, 1105, 16, 263, 705, 173 },
  { 2204, 1105, 16, 263, 705, 173 },
  { 2431, 1105, 16, 263, 705, 173 },
  { 185, 1105, 16, 263, 705, 173 },
  { 522, 1105, 16, 263, 705, 173 },
  { 265, 1151, 16, 263, 3712, 182 },
  { 604, 1013, 16, 263, 4944, 157 },
  { 817, 1059, 16, 263, 12272, 46 },
  { 821, 923, 883, 233, 3857, 191 },
  { 1099, 923, 463, 233, 3857, 191 },
  { 1309, 923, 463, 233, 3857, 191 },
  { 1587, 923, 463, 233, 3857, 191 },
  { 1787, 923, 463, 233, 3857, 191 },
  { 2075, 923, 463, 233, 3857, 191 },
  { 2270, 923, 463, 233, 3857, 191 },
  { 2521, 923, 463, 233, 3857, 191 },
  { 59, 923, 463, 233, 3857, 191 },
  { 367, 923, 463, 233, 3857, 191 },
  { 681, 923, 463, 233, 3857, 191 },
  { 924, 923, 463, 233, 3857, 191 },
  { 1174, 923, 463, 233, 3857, 191 },
  { 1414, 923, 463, 233, 3857, 191 },
  { 1652, 923, 463, 233, 3857, 191 },
  { 1884, 923, 463, 233, 3857, 191 },
  { 2140, 923, 463, 233, 3857, 191 },
  { 2367, 923, 463, 233, 3857, 191 },
  { 125, 923, 463, 233, 3857, 191 },
  { 466, 923, 463, 233, 3857, 191 },
  { 749, 923, 463, 233, 3857, 191 },
  { 1024, 923, 463, 233, 3857, 191 },
  { 1242, 923, 463, 233, 3857, 191 },
  { 1514, 923, 463, 233, 3857, 191 },
  { 1720, 923, 463, 233, 3857, 191 },
  { 1984, 923, 463, 233, 3857, 191 },
  { 2208, 923, 463, 233, 3857, 191 },
  { 2435, 923, 463, 233, 3857, 191 },
  { 189, 923, 463, 233, 3857, 191 },
  { 526, 923, 463, 233, 3857, 191 },
  { 269, 953, 463, 233, 5152, 166 },
  { 608, 983, 463, 233, 13504, 55 },
};

  // FPR8 Register Class...
  static const MCPhysReg FPR8[] = {
    AArch64_B0, AArch64_B1, AArch64_B2, AArch64_B3, AArch64_B4, AArch64_B5, AArch64_B6, AArch64_B7, AArch64_B8, AArch64_B9, AArch64_B10, AArch64_B11, AArch64_B12, AArch64_B13, AArch64_B14, AArch64_B15, AArch64_B16, AArch64_B17, AArch64_B18, AArch64_B19, AArch64_B20, AArch64_B21, AArch64_B22, AArch64_B23, AArch64_B24, AArch64_B25, AArch64_B26, AArch64_B27, AArch64_B28, AArch64_B29, AArch64_B30, AArch64_B31,
  };
  // FPR8 Bit set.
  static const uint8_t FPR8Bits[] = {
    0x00, 0xf8, 0xff, 0xff, 0xff, 0x07,
  };
  // FPR16 Register Class...
  static const MCPhysReg FPR16[] = {
    AArch64_H0, AArch64_H1, AArch64_H2, AArch64_H3, AArch64_H4, AArch64_H5, AArch64_H6, AArch64_H7, AArch64_H8, AArch64_H9, AArch64_H10, AArch64_H11, AArch64_H12, AArch64_H13, AArch64_H14, AArch64_H15, AArch64_H16, AArch64_H17, AArch64_H18, AArch64_H19, AArch64_H20, AArch64_H21, AArch64_H22, AArch64_H23, AArch64_H24, AArch64_H25, AArch64_H26, AArch64_H27, AArch64_H28, AArch64_H29, AArch64_H30, AArch64_H31,
  };
  // FPR16 Bit set.
  static const uint8_t FPR16Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x07,
  };
  // FPR16_lo Register Class...
  static const MCPhysReg FPR16_lo[] = {
    AArch64_H0, AArch64_H1, AArch64_H2, AArch64_H3, AArch64_H4, AArch64_H5, AArch64_H6, AArch64_H7, AArch64_H8, AArch64_H9, AArch64_H10, AArch64_H11, AArch64_H12, AArch64_H13, AArch64_H14, AArch64_H15,
  };
  // FPR16_lo Bit set.
  static const uint8_t FPR16_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x07,
  };
  // PPR Register Class...
  static const MCPhysReg PPR[] = {
    AArch64_P0, AArch64_P1, AArch64_P2, AArch64_P3, AArch64_P4, AArch64_P5, AArch64_P6, AArch64_P7, AArch64_P8, AArch64_P9, AArch64_P10, AArch64_P11, AArch64_P12, AArch64_P13, AArch64_P14, AArch64_P15,
  };
  // PPR Bit set.
  static const uint8_t PPRBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x07,
  };
  // PPR_3b Register Class...
  static const MCPhysReg PPR_3b[] = {
    AArch64_P0, AArch64_P1, AArch64_P2, AArch64_P3, AArch64_P4, AArch64_P5, AArch64_P6, AArch64_P7,
  };
  // PPR_3b Bit set.
  static const uint8_t PPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x07,
  };
  // GPR32all Register Class...
  static const MCPhysReg GPR32all[] = {
    AArch64_W0, AArch64_W1, AArch64_W2, AArch64_W3, AArch64_W4, AArch64_W5, AArch64_W6, AArch64_W7, AArch64_W8, AArch64_W9, AArch64_W10, AArch64_W11, AArch64_W12, AArch64_W13, AArch64_W14, AArch64_W15, AArch64_W16, AArch64_W17, AArch64_W18, AArch64_W19, AArch64_W20, AArch64_W21, AArch64_W22, AArch64_W23, AArch64_W24, AArch64_W25, AArch64_W26, AArch64_W27, AArch64_W28, AArch64_W29, AArch64_W30, AArch64_WZR, AArch64_WSP,
  };
  // GPR32all Bit set.
  static const uint8_t GPR32allBits[] = {
    0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x03,
  };
  // FPR32 Register Class...
  static const MCPhysReg FPR32[] = {
    AArch64_S0, AArch64_S1, AArch64_S2, AArch64_S3, AArch64_S4, AArch64_S5, AArch64_S6, AArch64_S7, AArch64_S8, AArch64_S9, AArch64_S10, AArch64_S11, AArch64_S12, AArch64_S13, AArch64_S14, AArch64_S15, AArch64_S16, AArch64_S17, AArch64_S18, AArch64_S19, AArch64_S20, AArch64_S21, AArch64_S22, AArch64_S23, AArch64_S24, AArch64_S25, AArch64_S26, AArch64_S27, AArch64_S28, AArch64_S29, AArch64_S30, AArch64_S31,
  };
  // FPR32 Bit set.
  static const uint8_t FPR32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x07,
  };
  // GPR32 Register Class...
  static const MCPhysReg GPR32[] = {
    AArch64_W0, AArch64_W1, AArch64_W2, AArch64_W3, AArch64_W4, AArch64_W5, AArch64_W6, AArch64_W7, AArch64_W8, AArch64_W9, AArch64_W10, AArch64_W11, AArch64_W12, AArch64_W13, AArch64_W14, AArch64_W15, AArch64_W16, AArch64_W17, AArch64_W18, AArch64_W19, AArch64_W20, AArch64_W21, AArch64_W22, AArch64_W23, AArch64_W24, AArch64_W25, AArch64_W26, AArch64_W27, AArch64_W28, AArch64_W29, AArch64_W30, AArch64_WZR,
  };
  // GPR32 Bit set.
  static const uint8_t GPR32Bits[] = {
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x03,
  };
  // GPR32sp Register Class...
  static const MCPhysReg GPR32sp[] = {
    AArch64_W0, AArch64_W1, AArch64_W2, AArch64_W3, AArch64_W4, AArch64_W5, AArch64_W6, AArch64_W7, AArch64_W8, AArch64_W9, AArch64_W10, AArch64_W11, AArch64_W12, AArch64_W13, AArch64_W14, AArch64_W15, AArch64_W16, AArch64_W17, AArch64_W18, AArch64_W19, AArch64_W20, AArch64_W21, AArch64_W22, AArch64_W23, AArch64_W24, AArch64_W25, AArch64_W26, AArch64_W27, AArch64_W28, AArch64_W29, AArch64_W30, AArch64_WSP,
  };
  // GPR32sp Bit set.
  static const uint8_t GPR32spBits[] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x03,
  };
  // GPR32common Register Class...
  static const MCPhysReg GPR32common[] = {
    AArch64_W0, AArch64_W1, AArch64_W2, AArch64_W3, AArch64_W4, AArch64_W5, AArch64_W6, AArch64_W7, AArch64_W8, AArch64_W9, AArch64_W10, AArch64_W11, AArch64_W12, AArch64_W13, AArch64_W14, AArch64_W15, AArch64_W16, AArch64_W17, AArch64_W18, AArch64_W19, AArch64_W20, AArch64_W21, AArch64_W22, AArch64_W23, AArch64_W24, AArch64_W25, AArch64_W26, AArch64_W27, AArch64_W28, AArch64_W29, AArch64_W30,
  };
  // GPR32common Bit set.
  static const uint8_t GPR32commonBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x03,
  };
  // FPR32_with_hsub_in_FPR16_lo Register Class...
  static const MCPhysReg FPR32_with_hsub_in_FPR16_lo[] = {
    AArch64_S0, AArch64_S1, AArch64_S2, AArch64_S3, AArch64_S4, AArch64_S5, AArch64_S6, AArch64_S7, AArch64_S8, AArch64_S9, AArch64_S10, AArch64_S11, AArch64_S12, AArch64_S13, AArch64_S14, AArch64_S15,
  };
  // FPR32_with_hsub_in_FPR16_lo Bit set.
  static const uint8_t FPR32_with_hsub_in_FPR16_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x07,
  };
  // GPR32arg Register Class...
  static const MCPhysReg GPR32arg[] = {
    AArch64_W0, AArch64_W1, AArch64_W2, AArch64_W3, AArch64_W4, AArch64_W5, AArch64_W6, AArch64_W7,
  };
  // GPR32arg Bit set.
  static const uint8_t GPR32argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x07,
  };
  // MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg MatrixIndexGPR32_12_15[] = {
    AArch64_W12, AArch64_W13, AArch64_W14, AArch64_W15,
  };
  // MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07,
  };
  // CCR Register Class...
  static const MCPhysReg CCR[] = {
    AArch64_NZCV,
  };
  // CCR Bit set.
  static const uint8_t CCRBits[] = {
    0x10,
  };
  // GPR32sponly Register Class...
  static const MCPhysReg GPR32sponly[] = {
    AArch64_WSP,
  };
  // GPR32sponly Bit set.
  static const uint8_t GPR32sponlyBits[] = {
    0x80,
  };
  // WSeqPairsClass Register Class...
  static const MCPhysReg WSeqPairsClass[] = {
    AArch64_W0_W1, AArch64_W2_W3, AArch64_W4_W5, AArch64_W6_W7, AArch64_W8_W9, AArch64_W10_W11, AArch64_W12_W13, AArch64_W14_W15, AArch64_W16_W17, AArch64_W18_W19, AArch64_W20_W21, AArch64_W22_W23, AArch64_W24_W25, AArch64_W26_W27, AArch64_W28_W29, AArch64_W30_WZR,
  };
  // WSeqPairsClass Bit set.
  static const uint8_t WSeqPairsClassBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };
  // WSeqPairsClass_with_subo32_in_GPR32common Register Class...
  static const MCPhysReg WSeqPairsClass_with_subo32_in_GPR32common[] = {
    AArch64_W0_W1, AArch64_W2_W3, AArch64_W4_W5, AArch64_W6_W7, AArch64_W8_W9, AArch64_W10_W11, AArch64_W12_W13, AArch64_W14_W15, AArch64_W16_W17, AArch64_W18_W19, AArch64_W20_W21, AArch64_W22_W23, AArch64_W24_W25, AArch64_W26_W27, AArch64_W28_W29,
  };
  // WSeqPairsClass_with_subo32_in_GPR32common Bit set.
  static const uint8_t WSeqPairsClass_with_subo32_in_GPR32commonBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x03,
  };
  // WSeqPairsClass_with_sube32_in_GPR32arg Register Class...
  static const MCPhysReg WSeqPairsClass_with_sube32_in_GPR32arg[] = {
    AArch64_W0_W1, AArch64_W2_W3, AArch64_W4_W5, AArch64_W6_W7,
  };
  // WSeqPairsClass_with_sube32_in_GPR32arg Bit set.
  static const uint8_t WSeqPairsClass_with_sube32_in_GPR32argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78,
  };
  // WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_W12_W13, AArch64_W14_W15,
  };
  // WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
  };
  // GPR64all Register Class...
  static const MCPhysReg GPR64all[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X16, AArch64_X17, AArch64_X18, AArch64_X19, AArch64_X20, AArch64_X21, AArch64_X22, AArch64_X23, AArch64_X24, AArch64_X25, AArch64_X26, AArch64_X27, AArch64_X28, AArch64_FP, AArch64_LR, AArch64_XZR, AArch64_SP,
  };
  // GPR64all Bit set.
  static const uint8_t GPR64allBits[] = {
    0x2c, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x7f,
  };
  // FPR64 Register Class...
  static const MCPhysReg FPR64[] = {
    AArch64_D0, AArch64_D1, AArch64_D2, AArch64_D3, AArch64_D4, AArch64_D5, AArch64_D6, AArch64_D7, AArch64_D8, AArch64_D9, AArch64_D10, AArch64_D11, AArch64_D12, AArch64_D13, AArch64_D14, AArch64_D15, AArch64_D16, AArch64_D17, AArch64_D18, AArch64_D19, AArch64_D20, AArch64_D21, AArch64_D22, AArch64_D23, AArch64_D24, AArch64_D25, AArch64_D26, AArch64_D27, AArch64_D28, AArch64_D29, AArch64_D30, AArch64_D31,
  };
  // FPR64 Bit set.
  static const uint8_t FPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x07,
  };
  // GPR64 Register Class...
  static const MCPhysReg GPR64[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X16, AArch64_X17, AArch64_X18, AArch64_X19, AArch64_X20, AArch64_X21, AArch64_X22, AArch64_X23, AArch64_X24, AArch64_X25, AArch64_X26, AArch64_X27, AArch64_X28, AArch64_FP, AArch64_LR, AArch64_XZR,
  };
  // GPR64 Bit set.
  static const uint8_t GPR64Bits[] = {
    0x0c, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x7f,
  };
  // GPR64sp Register Class...
  static const MCPhysReg GPR64sp[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X16, AArch64_X17, AArch64_X18, AArch64_X19, AArch64_X20, AArch64_X21, AArch64_X22, AArch64_X23, AArch64_X24, AArch64_X25, AArch64_X26, AArch64_X27, AArch64_X28, AArch64_FP, AArch64_LR, AArch64_SP,
  };
  // GPR64sp Bit set.
  static const uint8_t GPR64spBits[] = {
    0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x7f,
  };
  // GPR64common Register Class...
  static const MCPhysReg GPR64common[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X16, AArch64_X17, AArch64_X18, AArch64_X19, AArch64_X20, AArch64_X21, AArch64_X22, AArch64_X23, AArch64_X24, AArch64_X25, AArch64_X26, AArch64_X27, AArch64_X28, AArch64_FP, AArch64_LR,
  };
  // GPR64common Bit set.
  static const uint8_t GPR64commonBits[] = {
    0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0x7f,
  };
  // GPR64noip Register Class...
  static const MCPhysReg GPR64noip[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X18, AArch64_X19, AArch64_X20, AArch64_X21, AArch64_X22, AArch64_X23, AArch64_X24, AArch64_X25, AArch64_X26, AArch64_X27, AArch64_X28, AArch64_FP, AArch64_XZR,
  };
  // GPR64noip Bit set.
  static const uint8_t GPR64noipBits[] = {
    0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xf3, 0x7f,
  };
  // GPR64common_and_GPR64noip Register Class...
  static const MCPhysReg GPR64common_and_GPR64noip[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X18, AArch64_X19, AArch64_X20, AArch64_X21, AArch64_X22, AArch64_X23, AArch64_X24, AArch64_X25, AArch64_X26, AArch64_X27, AArch64_X28, AArch64_FP,
  };
  // GPR64common_and_GPR64noip Bit set.
  static const uint8_t GPR64common_and_GPR64noipBits[] = {
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xf3, 0x7f,
  };
  // tcGPR64 Register Class...
  static const MCPhysReg tcGPR64[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X16, AArch64_X17, AArch64_X18,
  };
  // tcGPR64 Bit set.
  static const uint8_t tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x1f,
  };
  // GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64noip_and_tcGPR64[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7, AArch64_X8, AArch64_X9, AArch64_X10, AArch64_X11, AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15, AArch64_X18,
  };
  // GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x13,
  };
  // FPR64_lo Register Class...
  static const MCPhysReg FPR64_lo[] = {
    AArch64_D0, AArch64_D1, AArch64_D2, AArch64_D3, AArch64_D4, AArch64_D5, AArch64_D6, AArch64_D7, AArch64_D8, AArch64_D9, AArch64_D10, AArch64_D11, AArch64_D12, AArch64_D13, AArch64_D14, AArch64_D15,
  };
  // FPR64_lo Bit set.
  static const uint8_t FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x07,
  };
  // GPR64arg Register Class...
  static const MCPhysReg GPR64arg[] = {
    AArch64_X0, AArch64_X1, AArch64_X2, AArch64_X3, AArch64_X4, AArch64_X5, AArch64_X6, AArch64_X7,
  };
  // GPR64arg Bit set.
  static const uint8_t GPR64argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03,
  };
  // GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg GPR64_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X12, AArch64_X13, AArch64_X14, AArch64_X15,
  };
  // GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x03,
  };
  // rtcGPR64 Register Class...
  static const MCPhysReg rtcGPR64[] = {
    AArch64_X16, AArch64_X17,
  };
  // rtcGPR64 Bit set.
  static const uint8_t rtcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
  };
  // GPR64sponly Register Class...
  static const MCPhysReg GPR64sponly[] = {
    AArch64_SP,
  };
  // GPR64sponly Bit set.
  static const uint8_t GPR64sponlyBits[] = {
    0x20,
  };
  // DD Register Class...
  static const MCPhysReg DD[] = {
    AArch64_D0_D1, AArch64_D1_D2, AArch64_D2_D3, AArch64_D3_D4, AArch64_D4_D5, AArch64_D5_D6, AArch64_D6_D7, AArch64_D7_D8, AArch64_D8_D9, AArch64_D9_D10, AArch64_D10_D11, AArch64_D11_D12, AArch64_D12_D13, AArch64_D13_D14, AArch64_D14_D15, AArch64_D15_D16, AArch64_D16_D17, AArch64_D17_D18, AArch64_D18_D19, AArch64_D19_D20, AArch64_D20_D21, AArch64_D21_D22, AArch64_D22_D23, AArch64_D23_D24, AArch64_D24_D25, AArch64_D25_D26, AArch64_D26_D27, AArch64_D27_D28, AArch64_D28_D29, AArch64_D29_D30, AArch64_D30_D31, AArch64_D31_D0,
  };
  // DD Bit set.
  static const uint8_t DDBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x3f,
  };
  // DD_with_dsub0_in_FPR64_lo Register Class...
  static const MCPhysReg DD_with_dsub0_in_FPR64_lo[] = {
    AArch64_D0_D1, AArch64_D1_D2, AArch64_D2_D3, AArch64_D3_D4, AArch64_D4_D5, AArch64_D5_D6, AArch64_D6_D7, AArch64_D7_D8, AArch64_D8_D9, AArch64_D9_D10, AArch64_D10_D11, AArch64_D11_D12, AArch64_D12_D13, AArch64_D13_D14, AArch64_D14_D15, AArch64_D15_D16,
  };
  // DD_with_dsub0_in_FPR64_lo Bit set.
  static const uint8_t DD_with_dsub0_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f,
  };
  // DD_with_dsub1_in_FPR64_lo Register Class...
  static const MCPhysReg DD_with_dsub1_in_FPR64_lo[] = {
    AArch64_D0_D1, AArch64_D1_D2, AArch64_D2_D3, AArch64_D3_D4, AArch64_D4_D5, AArch64_D5_D6, AArch64_D6_D7, AArch64_D7_D8, AArch64_D8_D9, AArch64_D9_D10, AArch64_D10_D11, AArch64_D11_D12, AArch64_D12_D13, AArch64_D13_D14, AArch64_D14_D15, AArch64_D31_D0,
  };
  // DD_with_dsub1_in_FPR64_lo Bit set.
  static const uint8_t DD_with_dsub1_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x20,
  };
  // XSeqPairsClass Register Class...
  static const MCPhysReg XSeqPairsClass[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X16_X17, AArch64_X18_X19, AArch64_X20_X21, AArch64_X22_X23, AArch64_X24_X25, AArch64_X26_X27, AArch64_X28_FP, AArch64_LR_XZR,
  };
  // XSeqPairsClass Bit set.
  static const uint8_t XSeqPairsClassBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };
  // DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_lo Register Class...
  static const MCPhysReg DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_lo[] = {
    AArch64_D0_D1, AArch64_D1_D2, AArch64_D2_D3, AArch64_D3_D4, AArch64_D4_D5, AArch64_D5_D6, AArch64_D6_D7, AArch64_D7_D8, AArch64_D8_D9, AArch64_D9_D10, AArch64_D10_D11, AArch64_D11_D12, AArch64_D12_D13, AArch64_D13_D14, AArch64_D14_D15,
  };
  // DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_lo Bit set.
  static const uint8_t DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f,
  };
  // XSeqPairsClass_with_subo64_in_GPR64common Register Class...
  static const MCPhysReg XSeqPairsClass_with_subo64_in_GPR64common[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X16_X17, AArch64_X18_X19, AArch64_X20_X21, AArch64_X22_X23, AArch64_X24_X25, AArch64_X26_X27, AArch64_X28_FP,
  };
  // XSeqPairsClass_with_subo64_in_GPR64common Bit set.
  static const uint8_t XSeqPairsClass_with_subo64_in_GPR64commonBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x03,
  };
  // XSeqPairsClass_with_subo64_in_GPR64noip Register Class...
  static const MCPhysReg XSeqPairsClass_with_subo64_in_GPR64noip[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X18_X19, AArch64_X20_X21, AArch64_X22_X23, AArch64_X24_X25, AArch64_X26_X27, AArch64_X28_FP, AArch64_LR_XZR,
  };
  // XSeqPairsClass_with_subo64_in_GPR64noip Bit set.
  static const uint8_t XSeqPairsClass_with_subo64_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xef, 0x03,
  };
  // XSeqPairsClass_with_sube64_in_GPR64noip Register Class...
  static const MCPhysReg XSeqPairsClass_with_sube64_in_GPR64noip[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X18_X19, AArch64_X20_X21, AArch64_X22_X23, AArch64_X24_X25, AArch64_X26_X27, AArch64_X28_FP,
  };
  // XSeqPairsClass_with_sube64_in_GPR64noip Bit set.
  static const uint8_t XSeqPairsClass_with_sube64_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xef, 0x03,
  };
  // XSeqPairsClass_with_sube64_in_tcGPR64 Register Class...
  static const MCPhysReg XSeqPairsClass_with_sube64_in_tcGPR64[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X16_X17, AArch64_X18_X19,
  };
  // XSeqPairsClass_with_sube64_in_tcGPR64 Bit set.
  static const uint8_t XSeqPairsClass_with_sube64_in_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,
  };
  // XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X18_X19,
  };
  // XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x2f,
  };
  // XSeqPairsClass_with_subo64_in_tcGPR64 Register Class...
  static const MCPhysReg XSeqPairsClass_with_subo64_in_tcGPR64[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15, AArch64_X16_X17,
  };
  // XSeqPairsClass_with_subo64_in_tcGPR64 Bit set.
  static const uint8_t XSeqPairsClass_with_subo64_in_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x1f,
  };
  // XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7, AArch64_X8_X9, AArch64_X10_X11, AArch64_X12_X13, AArch64_X14_X15,
  };
  // XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f,
  };
  // XSeqPairsClass_with_sub_32_in_GPR32arg Register Class...
  static const MCPhysReg XSeqPairsClass_with_sub_32_in_GPR32arg[] = {
    AArch64_X0_X1, AArch64_X2_X3, AArch64_X4_X5, AArch64_X6_X7,
  };
  // XSeqPairsClass_with_sub_32_in_GPR32arg Bit set.
  static const uint8_t XSeqPairsClass_with_sub_32_in_GPR32argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0,
  };
  // XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X12_X13, AArch64_X14_X15,
  };
  // XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
  };
  // XSeqPairsClass_with_sube64_in_rtcGPR64 Register Class...
  static const MCPhysReg XSeqPairsClass_with_sube64_in_rtcGPR64[] = {
    AArch64_X16_X17,
  };
  // XSeqPairsClass_with_sube64_in_rtcGPR64 Bit set.
  static const uint8_t XSeqPairsClass_with_sube64_in_rtcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
  };
  // FPR128 Register Class...
  static const MCPhysReg FPR128[] = {
    AArch64_Q0, AArch64_Q1, AArch64_Q2, AArch64_Q3, AArch64_Q4, AArch64_Q5, AArch64_Q6, AArch64_Q7, AArch64_Q8, AArch64_Q9, AArch64_Q10, AArch64_Q11, AArch64_Q12, AArch64_Q13, AArch64_Q14, AArch64_Q15, AArch64_Q16, AArch64_Q17, AArch64_Q18, AArch64_Q19, AArch64_Q20, AArch64_Q21, AArch64_Q22, AArch64_Q23, AArch64_Q24, AArch64_Q25, AArch64_Q26, AArch64_Q27, AArch64_Q28, AArch64_Q29, AArch64_Q30, AArch64_Q31,
  };
  // FPR128 Bit set.
  static const uint8_t FPR128Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0xff, 0xff, 0x07,
  };
  // ZPR Register Class...
  static const MCPhysReg ZPR[] = {
    AArch64_Z0, AArch64_Z1, AArch64_Z2, AArch64_Z3, AArch64_Z4, AArch64_Z5, AArch64_Z6, AArch64_Z7, AArch64_Z8, AArch64_Z9, AArch64_Z10, AArch64_Z11, AArch64_Z12, AArch64_Z13, AArch64_Z14, AArch64_Z15, AArch64_Z16, AArch64_Z17, AArch64_Z18, AArch64_Z19, AArch64_Z20, AArch64_Z21, AArch64_Z22, AArch64_Z23, AArch64_Z24, AArch64_Z25, AArch64_Z26, AArch64_Z27, AArch64_Z28, AArch64_Z29, AArch64_Z30, AArch64_Z31,
  };
  // ZPR Bit set.
  static const uint8_t ZPRBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0xff, 0x7f,
  };
  // FPR128_lo Register Class...
  static const MCPhysReg FPR128_lo[] = {
    AArch64_Q0, AArch64_Q1, AArch64_Q2, AArch64_Q3, AArch64_Q4, AArch64_Q5, AArch64_Q6, AArch64_Q7, AArch64_Q8, AArch64_Q9, AArch64_Q10, AArch64_Q11, AArch64_Q12, AArch64_Q13, AArch64_Q14, AArch64_Q15,
  };
  // FPR128_lo Bit set.
  static const uint8_t FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff, 0x07,
  };
  // MPR128 Register Class...
  static const MCPhysReg MPR128[] = {
    AArch64_ZAQ0, AArch64_ZAQ1, AArch64_ZAQ2, AArch64_ZAQ3, AArch64_ZAQ4, AArch64_ZAQ5, AArch64_ZAQ6, AArch64_ZAQ7, AArch64_ZAQ8, AArch64_ZAQ9, AArch64_ZAQ10, AArch64_ZAQ11, AArch64_ZAQ12, AArch64_ZAQ13, AArch64_ZAQ14, AArch64_ZAQ15,
  };
  // MPR128 Bit set.
  static const uint8_t MPR128Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };
  // ZPR_4b Register Class...
  static const MCPhysReg ZPR_4b[] = {
    AArch64_Z0, AArch64_Z1, AArch64_Z2, AArch64_Z3, AArch64_Z4, AArch64_Z5, AArch64_Z6, AArch64_Z7, AArch64_Z8, AArch64_Z9, AArch64_Z10, AArch64_Z11, AArch64_Z12, AArch64_Z13, AArch64_Z14, AArch64_Z15,
  };
  // ZPR_4b Bit set.
  static const uint8_t ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x7f,
  };
  // ZPR_3b Register Class...
  static const MCPhysReg ZPR_3b[] = {
    AArch64_Z0, AArch64_Z1, AArch64_Z2, AArch64_Z3, AArch64_Z4, AArch64_Z5, AArch64_Z6, AArch64_Z7,
  };
  // ZPR_3b Bit set.
  static const uint8_t ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7f,
  };
  // DDD Register Class...
  static const MCPhysReg DDD[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15, AArch64_D14_D15_D16, AArch64_D15_D16_D17, AArch64_D16_D17_D18, AArch64_D17_D18_D19, AArch64_D18_D19_D20, AArch64_D19_D20_D21, AArch64_D20_D21_D22, AArch64_D21_D22_D23, AArch64_D22_D23_D24, AArch64_D23_D24_D25, AArch64_D24_D25_D26, AArch64_D25_D26_D27, AArch64_D26_D27_D28, AArch64_D27_D28_D29, AArch64_D28_D29_D30, AArch64_D29_D30_D31, AArch64_D30_D31_D0, AArch64_D31_D0_D1,
  };
  // DDD Bit set.
  static const uint8_t DDDBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x3f,
  };
  // DDD_with_dsub0_in_FPR64_lo Register Class...
  static const MCPhysReg DDD_with_dsub0_in_FPR64_lo[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15, AArch64_D14_D15_D16, AArch64_D15_D16_D17,
  };
  // DDD_with_dsub0_in_FPR64_lo Bit set.
  static const uint8_t DDD_with_dsub0_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f,
  };
  // DDD_with_dsub1_in_FPR64_lo Register Class...
  static const MCPhysReg DDD_with_dsub1_in_FPR64_lo[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15, AArch64_D14_D15_D16, AArch64_D31_D0_D1,
  };
  // DDD_with_dsub1_in_FPR64_lo Bit set.
  static const uint8_t DDD_with_dsub1_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x20,
  };
  // DDD_with_dsub2_in_FPR64_lo Register Class...
  static const MCPhysReg DDD_with_dsub2_in_FPR64_lo[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15, AArch64_D30_D31_D0, AArch64_D31_D0_D1,
  };
  // DDD_with_dsub2_in_FPR64_lo Bit set.
  static const uint8_t DDD_with_dsub2_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x30,
  };
  // DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_lo Register Class...
  static const MCPhysReg DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_lo[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15, AArch64_D14_D15_D16,
  };
  // DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_lo Bit set.
  static const uint8_t DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f,
  };
  // DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo Register Class...
  static const MCPhysReg DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15, AArch64_D31_D0_D1,
  };
  // DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo Bit set.
  static const uint8_t DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x20,
  };
  // DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo Register Class...
  static const MCPhysReg DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo[] = {
    AArch64_D0_D1_D2, AArch64_D1_D2_D3, AArch64_D2_D3_D4, AArch64_D3_D4_D5, AArch64_D4_D5_D6, AArch64_D5_D6_D7, AArch64_D6_D7_D8, AArch64_D7_D8_D9, AArch64_D8_D9_D10, AArch64_D9_D10_D11, AArch64_D10_D11_D12, AArch64_D11_D12_D13, AArch64_D12_D13_D14, AArch64_D13_D14_D15,
  };
  // DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo Bit set.
  static const uint8_t DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f,
  };
  // DDDD Register Class...
  static const MCPhysReg DDDD[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16, AArch64_D14_D15_D16_D17, AArch64_D15_D16_D17_D18, AArch64_D16_D17_D18_D19, AArch64_D17_D18_D19_D20, AArch64_D18_D19_D20_D21, AArch64_D19_D20_D21_D22, AArch64_D20_D21_D22_D23, AArch64_D21_D22_D23_D24, AArch64_D22_D23_D24_D25, AArch64_D23_D24_D25_D26, AArch64_D24_D25_D26_D27, AArch64_D25_D26_D27_D28, AArch64_D26_D27_D28_D29, AArch64_D27_D28_D29_D30, AArch64_D28_D29_D30_D31, AArch64_D29_D30_D31_D0, AArch64_D30_D31_D0_D1, AArch64_D31_D0_D1_D2,
  };
  // DDDD Bit set.
  static const uint8_t DDDDBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x3f,
  };
  // DDDD_with_dsub0_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub0_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16, AArch64_D14_D15_D16_D17, AArch64_D15_D16_D17_D18,
  };
  // DDDD_with_dsub0_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub0_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f,
  };
  // DDDD_with_dsub1_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub1_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16, AArch64_D14_D15_D16_D17, AArch64_D31_D0_D1_D2,
  };
  // DDDD_with_dsub1_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub1_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x20,
  };
  // DDDD_with_dsub2_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub2_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16, AArch64_D30_D31_D0_D1, AArch64_D31_D0_D1_D2,
  };
  // DDDD_with_dsub2_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub2_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x30,
  };
  // DDDD_with_dsub3_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub3_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D29_D30_D31_D0, AArch64_D30_D31_D0_D1, AArch64_D31_D0_D1_D2,
  };
  // DDDD_with_dsub3_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub3_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x38,
  };
  // DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16, AArch64_D14_D15_D16_D17,
  };
  // DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f,
  };
  // DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16, AArch64_D31_D0_D1_D2,
  };
  // DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x20,
  };
  // DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D30_D31_D0_D1, AArch64_D31_D0_D1_D2,
  };
  // DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x30,
  };
  // DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D13_D14_D15_D16,
  };
  // DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f,
  };
  // DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15, AArch64_D31_D0_D1_D2,
  };
  // DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x20,
  };
  // DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo Register Class...
  static const MCPhysReg DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo[] = {
    AArch64_D0_D1_D2_D3, AArch64_D1_D2_D3_D4, AArch64_D2_D3_D4_D5, AArch64_D3_D4_D5_D6, AArch64_D4_D5_D6_D7, AArch64_D5_D6_D7_D8, AArch64_D6_D7_D8_D9, AArch64_D7_D8_D9_D10, AArch64_D8_D9_D10_D11, AArch64_D9_D10_D11_D12, AArch64_D10_D11_D12_D13, AArch64_D11_D12_D13_D14, AArch64_D12_D13_D14_D15,
  };
  // DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo Bit set.
  static const uint8_t DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07,
  };
  // QQ Register Class...
  static const MCPhysReg QQ[] = {
    AArch64_Q0_Q1, AArch64_Q1_Q2, AArch64_Q2_Q3, AArch64_Q3_Q4, AArch64_Q4_Q5, AArch64_Q5_Q6, AArch64_Q6_Q7, AArch64_Q7_Q8, AArch64_Q8_Q9, AArch64_Q9_Q10, AArch64_Q10_Q11, AArch64_Q11_Q12, AArch64_Q12_Q13, AArch64_Q13_Q14, AArch64_Q14_Q15, AArch64_Q15_Q16, AArch64_Q16_Q17, AArch64_Q17_Q18, AArch64_Q18_Q19, AArch64_Q19_Q20, AArch64_Q20_Q21, AArch64_Q21_Q22, AArch64_Q22_Q23, AArch64_Q23_Q24, AArch64_Q24_Q25, AArch64_Q25_Q26, AArch64_Q26_Q27, AArch64_Q27_Q28, AArch64_Q28_Q29, AArch64_Q29_Q30, AArch64_Q30_Q31, AArch64_Q31_Q0,
  };
  // QQ Bit set.
  static const uint8_t QQBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x3f,
  };
  // ZPR2 Register Class...
  static const MCPhysReg ZPR2[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7, AArch64_Z7_Z8, AArch64_Z8_Z9, AArch64_Z9_Z10, AArch64_Z10_Z11, AArch64_Z11_Z12, AArch64_Z12_Z13, AArch64_Z13_Z14, AArch64_Z14_Z15, AArch64_Z15_Z16, AArch64_Z16_Z17, AArch64_Z17_Z18, AArch64_Z18_Z19, AArch64_Z19_Z20, AArch64_Z20_Z21, AArch64_Z21_Z22, AArch64_Z22_Z23, AArch64_Z23_Z24, AArch64_Z24_Z25, AArch64_Z25_Z26, AArch64_Z26_Z27, AArch64_Z27_Z28, AArch64_Z28_Z29, AArch64_Z29_Z30, AArch64_Z30_Z31, AArch64_Z31_Z0,
  };
  // ZPR2 Bit set.
  static const uint8_t ZPR2Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };
  // QQ_with_dsub_in_FPR64_lo Register Class...
  static const MCPhysReg QQ_with_dsub_in_FPR64_lo[] = {
    AArch64_Q0_Q1, AArch64_Q1_Q2, AArch64_Q2_Q3, AArch64_Q3_Q4, AArch64_Q4_Q5, AArch64_Q5_Q6, AArch64_Q6_Q7, AArch64_Q7_Q8, AArch64_Q8_Q9, AArch64_Q9_Q10, AArch64_Q10_Q11, AArch64_Q11_Q12, AArch64_Q12_Q13, AArch64_Q13_Q14, AArch64_Q14_Q15, AArch64_Q15_Q16,
  };
  // QQ_with_dsub_in_FPR64_lo Bit set.
  static const uint8_t QQ_with_dsub_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f,
  };
  // QQ_with_qsub1_in_FPR128_lo Register Class...
  static const MCPhysReg QQ_with_qsub1_in_FPR128_lo[] = {
    AArch64_Q0_Q1, AArch64_Q1_Q2, AArch64_Q2_Q3, AArch64_Q3_Q4, AArch64_Q4_Q5, AArch64_Q5_Q6, AArch64_Q6_Q7, AArch64_Q7_Q8, AArch64_Q8_Q9, AArch64_Q9_Q10, AArch64_Q10_Q11, AArch64_Q11_Q12, AArch64_Q12_Q13, AArch64_Q13_Q14, AArch64_Q14_Q15, AArch64_Q31_Q0,
  };
  // QQ_with_qsub1_in_FPR128_lo Bit set.
  static const uint8_t QQ_with_qsub1_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x20,
  };
  // ZPR2_with_dsub_in_FPR64_lo Register Class...
  static const MCPhysReg ZPR2_with_dsub_in_FPR64_lo[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7, AArch64_Z7_Z8, AArch64_Z8_Z9, AArch64_Z9_Z10, AArch64_Z10_Z11, AArch64_Z11_Z12, AArch64_Z12_Z13, AArch64_Z13_Z14, AArch64_Z14_Z15, AArch64_Z15_Z16,
  };
  // ZPR2_with_dsub_in_FPR64_lo Bit set.
  static const uint8_t ZPR2_with_dsub_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };
  // ZPR2_with_zsub1_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR2_with_zsub1_in_ZPR_4b[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7, AArch64_Z7_Z8, AArch64_Z8_Z9, AArch64_Z9_Z10, AArch64_Z10_Z11, AArch64_Z11_Z12, AArch64_Z12_Z13, AArch64_Z13_Z14, AArch64_Z14_Z15, AArch64_Z31_Z0,
  };
  // ZPR2_with_zsub1_in_ZPR_4b Bit set.
  static const uint8_t ZPR2_with_zsub1_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01, 0x00, 0x02,
  };
  // QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_lo Register Class...
  static const MCPhysReg QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_lo[] = {
    AArch64_Q0_Q1, AArch64_Q1_Q2, AArch64_Q2_Q3, AArch64_Q3_Q4, AArch64_Q4_Q5, AArch64_Q5_Q6, AArch64_Q6_Q7, AArch64_Q7_Q8, AArch64_Q8_Q9, AArch64_Q9_Q10, AArch64_Q10_Q11, AArch64_Q11_Q12, AArch64_Q12_Q13, AArch64_Q13_Q14, AArch64_Q14_Q15,
  };
  // QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_lo Bit set.
  static const uint8_t QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f,
  };
  // ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4b[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7, AArch64_Z7_Z8, AArch64_Z8_Z9, AArch64_Z9_Z10, AArch64_Z10_Z11, AArch64_Z11_Z12, AArch64_Z12_Z13, AArch64_Z13_Z14, AArch64_Z14_Z15,
  };
  // ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4b Bit set.
  static const uint8_t ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01,
  };
  // ZPR2_with_zsub0_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR2_with_zsub0_in_ZPR_3b[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7, AArch64_Z7_Z8,
  };
  // ZPR2_with_zsub0_in_ZPR_3b Bit set.
  static const uint8_t ZPR2_with_zsub0_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03,
  };
  // ZPR2_with_zsub1_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR2_with_zsub1_in_ZPR_3b[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7, AArch64_Z31_Z0,
  };
  // ZPR2_with_zsub1_in_ZPR_3b Bit set.
  static const uint8_t ZPR2_with_zsub1_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01, 0x00, 0x00, 0x02,
  };
  // ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3b[] = {
    AArch64_Z0_Z1, AArch64_Z1_Z2, AArch64_Z2_Z3, AArch64_Z3_Z4, AArch64_Z4_Z5, AArch64_Z5_Z6, AArch64_Z6_Z7,
  };
  // ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3b Bit set.
  static const uint8_t ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01,
  };
  // MPR64 Register Class...
  static const MCPhysReg MPR64[] = {
    AArch64_ZAD0, AArch64_ZAD1, AArch64_ZAD2, AArch64_ZAD3, AArch64_ZAD4, AArch64_ZAD5, AArch64_ZAD6, AArch64_ZAD7,
  };
  // MPR64 Bit set.
  static const uint8_t MPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  };
  // QQQ Register Class...
  static const MCPhysReg QQQ[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15, AArch64_Q14_Q15_Q16, AArch64_Q15_Q16_Q17, AArch64_Q16_Q17_Q18, AArch64_Q17_Q18_Q19, AArch64_Q18_Q19_Q20, AArch64_Q19_Q20_Q21, AArch64_Q20_Q21_Q22, AArch64_Q21_Q22_Q23, AArch64_Q22_Q23_Q24, AArch64_Q23_Q24_Q25, AArch64_Q24_Q25_Q26, AArch64_Q25_Q26_Q27, AArch64_Q26_Q27_Q28, AArch64_Q27_Q28_Q29, AArch64_Q28_Q29_Q30, AArch64_Q29_Q30_Q31, AArch64_Q30_Q31_Q0, AArch64_Q31_Q0_Q1,
  };
  // QQQ Bit set.
  static const uint8_t QQQBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x3f,
  };
  // ZPR3 Register Class...
  static const MCPhysReg ZPR3[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15, AArch64_Z14_Z15_Z16, AArch64_Z15_Z16_Z17, AArch64_Z16_Z17_Z18, AArch64_Z17_Z18_Z19, AArch64_Z18_Z19_Z20, AArch64_Z19_Z20_Z21, AArch64_Z20_Z21_Z22, AArch64_Z21_Z22_Z23, AArch64_Z22_Z23_Z24, AArch64_Z23_Z24_Z25, AArch64_Z24_Z25_Z26, AArch64_Z25_Z26_Z27, AArch64_Z26_Z27_Z28, AArch64_Z27_Z28_Z29, AArch64_Z28_Z29_Z30, AArch64_Z29_Z30_Z31, AArch64_Z30_Z31_Z0, AArch64_Z31_Z0_Z1,
  };
  // ZPR3 Bit set.
  static const uint8_t ZPR3Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };
  // QQQ_with_dsub_in_FPR64_lo Register Class...
  static const MCPhysReg QQQ_with_dsub_in_FPR64_lo[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15, AArch64_Q14_Q15_Q16, AArch64_Q15_Q16_Q17,
  };
  // QQQ_with_dsub_in_FPR64_lo Bit set.
  static const uint8_t QQQ_with_dsub_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f,
  };
  // QQQ_with_qsub1_in_FPR128_lo Register Class...
  static const MCPhysReg QQQ_with_qsub1_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15, AArch64_Q14_Q15_Q16, AArch64_Q31_Q0_Q1,
  };
  // QQQ_with_qsub1_in_FPR128_lo Bit set.
  static const uint8_t QQQ_with_qsub1_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x20,
  };
  // QQQ_with_qsub2_in_FPR128_lo Register Class...
  static const MCPhysReg QQQ_with_qsub2_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15, AArch64_Q30_Q31_Q0, AArch64_Q31_Q0_Q1,
  };
  // QQQ_with_qsub2_in_FPR128_lo Bit set.
  static const uint8_t QQQ_with_qsub2_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x30,
  };
  // ZPR3_with_dsub_in_FPR64_lo Register Class...
  static const MCPhysReg ZPR3_with_dsub_in_FPR64_lo[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15, AArch64_Z14_Z15_Z16, AArch64_Z15_Z16_Z17,
  };
  // ZPR3_with_dsub_in_FPR64_lo Bit set.
  static const uint8_t ZPR3_with_dsub_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };
  // ZPR3_with_zsub1_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR3_with_zsub1_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15, AArch64_Z14_Z15_Z16, AArch64_Z31_Z0_Z1,
  };
  // ZPR3_with_zsub1_in_ZPR_4b Bit set.
  static const uint8_t ZPR3_with_zsub1_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01, 0x00, 0x02,
  };
  // ZPR3_with_zsub2_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR3_with_zsub2_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15, AArch64_Z30_Z31_Z0, AArch64_Z31_Z0_Z1,
  };
  // ZPR3_with_zsub2_in_ZPR_4b Bit set.
  static const uint8_t ZPR3_with_zsub2_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x03,
  };
  // QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_lo Register Class...
  static const MCPhysReg QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15, AArch64_Q14_Q15_Q16,
  };
  // QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_lo Bit set.
  static const uint8_t QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f,
  };
  // QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_lo Register Class...
  static const MCPhysReg QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15, AArch64_Q31_Q0_Q1,
  };
  // QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_lo Bit set.
  static const uint8_t QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x20,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15, AArch64_Z14_Z15_Z16,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4b Bit set.
  static const uint8_t ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01,
  };
  // ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15, AArch64_Z31_Z0_Z1,
  };
  // ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4b Bit set.
  static const uint8_t ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x02,
  };
  // QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_lo Register Class...
  static const MCPhysReg QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2, AArch64_Q1_Q2_Q3, AArch64_Q2_Q3_Q4, AArch64_Q3_Q4_Q5, AArch64_Q4_Q5_Q6, AArch64_Q5_Q6_Q7, AArch64_Q6_Q7_Q8, AArch64_Q7_Q8_Q9, AArch64_Q8_Q9_Q10, AArch64_Q9_Q10_Q11, AArch64_Q10_Q11_Q12, AArch64_Q11_Q12_Q13, AArch64_Q12_Q13_Q14, AArch64_Q13_Q14_Q15,
  };
  // QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_lo Bit set.
  static const uint8_t QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9, AArch64_Z8_Z9_Z10, AArch64_Z9_Z10_Z11, AArch64_Z10_Z11_Z12, AArch64_Z11_Z12_Z13, AArch64_Z12_Z13_Z14, AArch64_Z13_Z14_Z15,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4b Bit set.
  static const uint8_t ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff,
  };
  // ZPR3_with_zsub0_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR3_with_zsub0_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z7_Z8_Z9,
  };
  // ZPR3_with_zsub0_in_ZPR_3b Bit set.
  static const uint8_t ZPR3_with_zsub0_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03,
  };
  // ZPR3_with_zsub1_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR3_with_zsub1_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8, AArch64_Z31_Z0_Z1,
  };
  // ZPR3_with_zsub1_in_ZPR_3b Bit set.
  static const uint8_t ZPR3_with_zsub1_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01, 0x00, 0x00, 0x02,
  };
  // ZPR3_with_zsub2_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR3_with_zsub2_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z30_Z31_Z0, AArch64_Z31_Z0_Z1,
  };
  // ZPR3_with_zsub2_in_ZPR_3b Bit set.
  static const uint8_t ZPR3_with_zsub2_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x03,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z6_Z7_Z8,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3b Bit set.
  static const uint8_t ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01,
  };
  // ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7, AArch64_Z31_Z0_Z1,
  };
  // ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3b Bit set.
  static const uint8_t ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x02,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2, AArch64_Z1_Z2_Z3, AArch64_Z2_Z3_Z4, AArch64_Z3_Z4_Z5, AArch64_Z4_Z5_Z6, AArch64_Z5_Z6_Z7,
  };
  // ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3b Bit set.
  static const uint8_t ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
  };
  // QQQQ Register Class...
  static const MCPhysReg QQQQ[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16, AArch64_Q14_Q15_Q16_Q17, AArch64_Q15_Q16_Q17_Q18, AArch64_Q16_Q17_Q18_Q19, AArch64_Q17_Q18_Q19_Q20, AArch64_Q18_Q19_Q20_Q21, AArch64_Q19_Q20_Q21_Q22, AArch64_Q20_Q21_Q22_Q23, AArch64_Q21_Q22_Q23_Q24, AArch64_Q22_Q23_Q24_Q25, AArch64_Q23_Q24_Q25_Q26, AArch64_Q24_Q25_Q26_Q27, AArch64_Q25_Q26_Q27_Q28, AArch64_Q26_Q27_Q28_Q29, AArch64_Q27_Q28_Q29_Q30, AArch64_Q28_Q29_Q30_Q31, AArch64_Q29_Q30_Q31_Q0, AArch64_Q30_Q31_Q0_Q1, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ Bit set.
  static const uint8_t QQQQBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0xff, 0xff, 0x3f,
  };
  // ZPR4 Register Class...
  static const MCPhysReg ZPR4[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16, AArch64_Z14_Z15_Z16_Z17, AArch64_Z15_Z16_Z17_Z18, AArch64_Z16_Z17_Z18_Z19, AArch64_Z17_Z18_Z19_Z20, AArch64_Z18_Z19_Z20_Z21, AArch64_Z19_Z20_Z21_Z22, AArch64_Z20_Z21_Z22_Z23, AArch64_Z21_Z22_Z23_Z24, AArch64_Z22_Z23_Z24_Z25, AArch64_Z23_Z24_Z25_Z26, AArch64_Z24_Z25_Z26_Z27, AArch64_Z25_Z26_Z27_Z28, AArch64_Z26_Z27_Z28_Z29, AArch64_Z27_Z28_Z29_Z30, AArch64_Z28_Z29_Z30_Z31, AArch64_Z29_Z30_Z31_Z0, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4 Bit set.
  static const uint8_t ZPR4Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0xff, 0xff, 0x03,
  };
  // QQQQ_with_dsub_in_FPR64_lo Register Class...
  static const MCPhysReg QQQQ_with_dsub_in_FPR64_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16, AArch64_Q14_Q15_Q16_Q17, AArch64_Q15_Q16_Q17_Q18,
  };
  // QQQQ_with_dsub_in_FPR64_lo Bit set.
  static const uint8_t QQQQ_with_dsub_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x3f,
  };
  // QQQQ_with_qsub1_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_qsub1_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16, AArch64_Q14_Q15_Q16_Q17, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ_with_qsub1_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_qsub1_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f, 0x00, 0x20,
  };
  // QQQQ_with_qsub2_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_qsub2_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16, AArch64_Q30_Q31_Q0_Q1, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ_with_qsub2_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_qsub2_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x30,
  };
  // QQQQ_with_qsub3_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_qsub3_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q29_Q30_Q31_Q0, AArch64_Q30_Q31_Q0_Q1, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ_with_qsub3_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_qsub3_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x38,
  };
  // ZPR4_with_dsub_in_FPR64_lo Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16, AArch64_Z14_Z15_Z16_Z17, AArch64_Z15_Z16_Z17_Z18,
  };
  // ZPR4_with_dsub_in_FPR64_lo Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x03,
  };
  // ZPR4_with_zsub1_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_zsub1_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16, AArch64_Z14_Z15_Z16_Z17, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub1_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_zsub1_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01, 0x00, 0x02,
  };
  // ZPR4_with_zsub2_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_zsub2_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub2_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_zsub2_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x03,
  };
  // ZPR4_with_zsub3_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_zsub3_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z29_Z30_Z31_Z0, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub3_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_zsub3_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, 0x00, 0x80, 0x03,
  };
  // QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16, AArch64_Q14_Q15_Q16_Q17,
  };
  // QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x1f,
  };
  // QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f, 0x00, 0x20,
  };
  // QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q30_Q31_Q0_Q1, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x30,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16, AArch64_Z14_Z15_Z16_Z17,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x01,
  };
  // ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x02,
  };
  // ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, 0x00, 0x00, 0x03,
  };
  // QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q13_Q14_Q15_Q16,
  };
  // QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x0f,
  };
  // QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15, AArch64_Q31_Q0_Q1_Q2,
  };
  // QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07, 0x00, 0x20,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z13_Z14_Z15_Z16,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0xff,
  };
  // ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f, 0x00, 0x00, 0x02,
  };
  // QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_lo Register Class...
  static const MCPhysReg QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_lo[] = {
    AArch64_Q0_Q1_Q2_Q3, AArch64_Q1_Q2_Q3_Q4, AArch64_Q2_Q3_Q4_Q5, AArch64_Q3_Q4_Q5_Q6, AArch64_Q4_Q5_Q6_Q7, AArch64_Q5_Q6_Q7_Q8, AArch64_Q6_Q7_Q8_Q9, AArch64_Q7_Q8_Q9_Q10, AArch64_Q8_Q9_Q10_Q11, AArch64_Q9_Q10_Q11_Q12, AArch64_Q10_Q11_Q12_Q13, AArch64_Q11_Q12_Q13_Q14, AArch64_Q12_Q13_Q14_Q15,
  };
  // QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_lo Bit set.
  static const uint8_t QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_loBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x07,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4b Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10, AArch64_Z8_Z9_Z10_Z11, AArch64_Z9_Z10_Z11_Z12, AArch64_Z10_Z11_Z12_Z13, AArch64_Z11_Z12_Z13_Z14, AArch64_Z12_Z13_Z14_Z15,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4b Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x7f,
  };
  // ZPR4_with_zsub0_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub0_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z7_Z8_Z9_Z10,
  };
  // ZPR4_with_zsub0_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub0_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03,
  };
  // ZPR4_with_zsub1_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub1_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub1_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub1_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01, 0x00, 0x00, 0x02,
  };
  // ZPR4_with_zsub2_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub2_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub2_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub2_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x03,
  };
  // ZPR4_with_zsub3_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub3_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z29_Z30_Z31_Z0, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub3_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub3_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x80, 0x03,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z6_Z7_Z8_Z9,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x01,
  };
  // ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x02,
  };
  // ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z30_Z31_Z0_Z1, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x03,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z5_Z6_Z7_Z8,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
  };
  // ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7, AArch64_Z31_Z0_Z1_Z2,
  };
  // ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x02,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3b Register Class...
  static const MCPhysReg ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3b[] = {
    AArch64_Z0_Z1_Z2_Z3, AArch64_Z1_Z2_Z3_Z4, AArch64_Z2_Z3_Z4_Z5, AArch64_Z3_Z4_Z5_Z6, AArch64_Z4_Z5_Z6_Z7,
  };
  // ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3b Bit set.
  static const uint8_t ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3bBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c,
  };
  // GPR64x8Class Register Class...
  static const MCPhysReg GPR64x8Class[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class Bit set.
  static const uint8_t GPR64x8ClassBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x7f, 0x03,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xbf, 0x03,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xdf, 0x03,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xef, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x3f, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x5f, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x6f, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff, 0x01,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x9f, 0x03,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xaf, 0x03,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xcf, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7f, 0x01,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf, 0x01,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xdf, 0x01,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xef, 0x01,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_1_in_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_1_in_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xff,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x1f, 0x03,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x2f, 0x03,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x4f, 0x03,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x8f, 0x03,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x01,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x5f, 0x01,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x6f, 0x01,
  };
  // GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21,
  };
  // GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7f,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xdf,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xef,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f, 0x01,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xaf, 0x01,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xbf,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xcf, 0x01,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X18_X19_X20_X21_X22_X23_X24_X25, AArch64_X20_X21_X22_X23_X24_X25_X26_X27, AArch64_X22_X23_X24_X25_X26_X27_X28_FP,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x0f, 0x03,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21,
  };
  // GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x6f,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x1f, 0x01,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x2f, 0x01,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x9f,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xaf,
  };
  // GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19,
  };
  // GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x4f, 0x01,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X14_X15_X16_X17_X18_X19_X20_X21, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xcf,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X16_X17_X18_X19_X20_X21_X22_X23, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x8f, 0x01,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X14_X15_X16_X17_X18_X19_X20_X21,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x5f,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X14_X15_X16_X17_X18_X19_X20_X21,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x4f,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X18_X19_X20_X21_X22_X23_X24_X25,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0f, 0x01,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x8f,
  };
  // GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17,
  };
  // GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x1f,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X12_X13_X14_X15_X16_X17_X18_X19,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x2f,
  };
  // GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15,
  };
  // GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0f,
  };
  // GPR64x8Class_with_sub_32_in_GPR32arg Register Class...
  static const MCPhysReg GPR64x8Class_with_sub_32_in_GPR32arg[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11, AArch64_X6_X7_X8_X9_X10_X11_X12_X13,
  };
  // GPR64x8Class_with_sub_32_in_GPR32arg Bit set.
  static const uint8_t GPR64x8Class_with_sub_32_in_GPR32argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07,
  };
  // MPR32 Register Class...
  static const MCPhysReg MPR32[] = {
    AArch64_ZAS0, AArch64_ZAS1, AArch64_ZAS2, AArch64_ZAS3,
  };
  // MPR32 Bit set.
  static const uint8_t MPR32Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64arg Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64arg[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9, AArch64_X4_X5_X6_X7_X8_X9_X10_X11,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64arg Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x03,
  };
  // GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X12_X13_X14_X15_X16_X17_X18_X19, AArch64_X14_X15_X16_X17_X18_X19_X20_X21,
  };
  // GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X10_X11_X12_X13_X14_X15_X16_X17, AArch64_X12_X13_X14_X15_X16_X17_X18_X19,
  };
  // GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X8_X9_X10_X11_X12_X13_X14_X15, AArch64_X10_X11_X12_X13_X14_X15_X16_X17,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64arg Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64arg[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7, AArch64_X2_X3_X4_X5_X6_X7_X8_X9,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64arg Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X6_X7_X8_X9_X10_X11_X12_X13, AArch64_X8_X9_X10_X11_X12_X13_X14_X15,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
  };
  // GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Register Class...
  static const MCPhysReg GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15[] = {
    AArch64_X6_X7_X8_X9_X10_X11_X12_X13,
  };
  // GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15 Bit set.
  static const uint8_t GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
  };
  // GPR64x8Class_with_x8sub_0_in_rtcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_0_in_rtcGPR64[] = {
    AArch64_X16_X17_X18_X19_X20_X21_X22_X23,
  };
  // GPR64x8Class_with_x8sub_0_in_rtcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_0_in_rtcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  };
  // GPR64x8Class_with_x8sub_2_in_rtcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_2_in_rtcGPR64[] = {
    AArch64_X14_X15_X16_X17_X18_X19_X20_X21,
  };
  // GPR64x8Class_with_x8sub_2_in_rtcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_2_in_rtcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noip[] = {
    AArch64_X8_X9_X10_X11_X12_X13_X14_X15,
  };
  // GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noip Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
  };
  // GPR64x8Class_with_x8sub_4_in_rtcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_4_in_rtcGPR64[] = {
    AArch64_X12_X13_X14_X15_X16_X17_X18_X19,
  };
  // GPR64x8Class_with_x8sub_4_in_rtcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_4_in_rtcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64arg Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_6_in_GPR64arg[] = {
    AArch64_X0_X1_X2_X3_X4_X5_X6_X7,
  };
  // GPR64x8Class_with_x8sub_6_in_GPR64arg Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_6_in_GPR64argBits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  };
  // GPR64x8Class_with_x8sub_6_in_rtcGPR64 Register Class...
  static const MCPhysReg GPR64x8Class_with_x8sub_6_in_rtcGPR64[] = {
    AArch64_X10_X11_X12_X13_X14_X15_X16_X17,
  };
  // GPR64x8Class_with_x8sub_6_in_rtcGPR64 Bit set.
  static const uint8_t GPR64x8Class_with_x8sub_6_in_rtcGPR64Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
  };
  // MPR16 Register Class...
  static const MCPhysReg MPR16[] = {
    AArch64_ZAH0, AArch64_ZAH1,
  };
  // MPR16 Bit set.
  static const uint8_t MPR16Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  };
  // MPR Register Class...
  static const MCPhysReg MPR[] = {
    AArch64_ZA,
  };
  // MPR Bit set.
  static const uint8_t MPRBits[] = {
    0x00, 0x04,
  };
  // MPR8 Register Class...
  static const MCPhysReg MPR8[] = {
    AArch64_ZAB0,
  };
  // MPR8 Bit set.
  static const uint8_t MPR8Bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  };


static const MCRegisterClass AArch64MCRegisterClasses[] = {
  { FPR8, FPR8Bits, sizeof(FPR8Bits) },
  { FPR16, FPR16Bits, sizeof(FPR16Bits) },
  { FPR16_lo, FPR16_loBits, sizeof(FPR16_loBits) },
  { PPR, PPRBits, sizeof(PPRBits) },
  { PPR_3b, PPR_3bBits, sizeof(PPR_3bBits) },
  { GPR32all, GPR32allBits, sizeof(GPR32allBits) },
  { FPR32, FPR32Bits, sizeof(FPR32Bits) },
  { GPR32, GPR32Bits, sizeof(GPR32Bits) },
  { GPR32sp, GPR32spBits, sizeof(GPR32spBits) },
  { GPR32common, GPR32commonBits, sizeof(GPR32commonBits) },
  { FPR32_with_hsub_in_FPR16_lo, FPR32_with_hsub_in_FPR16_loBits, sizeof(FPR32_with_hsub_in_FPR16_loBits) },
  { GPR32arg, GPR32argBits, sizeof(GPR32argBits) },
  { MatrixIndexGPR32_12_15, MatrixIndexGPR32_12_15Bits, sizeof(MatrixIndexGPR32_12_15Bits) },
  { CCR, CCRBits, sizeof(CCRBits) },
  { GPR32sponly, GPR32sponlyBits, sizeof(GPR32sponlyBits) },
  { WSeqPairsClass, WSeqPairsClassBits, sizeof(WSeqPairsClassBits) },
  { WSeqPairsClass_with_subo32_in_GPR32common, WSeqPairsClass_with_subo32_in_GPR32commonBits, sizeof(WSeqPairsClass_with_subo32_in_GPR32commonBits) },
  { WSeqPairsClass_with_sube32_in_GPR32arg, WSeqPairsClass_with_sube32_in_GPR32argBits, sizeof(WSeqPairsClass_with_sube32_in_GPR32argBits) },
  { WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15, WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15Bits, sizeof(WSeqPairsClass_with_sube32_in_MatrixIndexGPR32_12_15Bits) },
  { GPR64all, GPR64allBits, sizeof(GPR64allBits) },
  { FPR64, FPR64Bits, sizeof(FPR64Bits) },
  { GPR64, GPR64Bits, sizeof(GPR64Bits) },
  { GPR64sp, GPR64spBits, sizeof(GPR64spBits) },
  { GPR64common, GPR64commonBits, sizeof(GPR64commonBits) },
  { GPR64noip, GPR64noipBits, sizeof(GPR64noipBits) },
  { GPR64common_and_GPR64noip, GPR64common_and_GPR64noipBits, sizeof(GPR64common_and_GPR64noipBits) },
  { tcGPR64, tcGPR64Bits, sizeof(tcGPR64Bits) },
  { GPR64noip_and_tcGPR64, GPR64noip_and_tcGPR64Bits, sizeof(GPR64noip_and_tcGPR64Bits) },
  { FPR64_lo, FPR64_loBits, sizeof(FPR64_loBits) },
  { GPR64arg, GPR64argBits, sizeof(GPR64argBits) },
  { GPR64_with_sub_32_in_MatrixIndexGPR32_12_15, GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { rtcGPR64, rtcGPR64Bits, sizeof(rtcGPR64Bits) },
  { GPR64sponly, GPR64sponlyBits, sizeof(GPR64sponlyBits) },
  { DD, DDBits, sizeof(DDBits) },
  { DD_with_dsub0_in_FPR64_lo, DD_with_dsub0_in_FPR64_loBits, sizeof(DD_with_dsub0_in_FPR64_loBits) },
  { DD_with_dsub1_in_FPR64_lo, DD_with_dsub1_in_FPR64_loBits, sizeof(DD_with_dsub1_in_FPR64_loBits) },
  { XSeqPairsClass, XSeqPairsClassBits, sizeof(XSeqPairsClassBits) },
  { DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_lo, DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_loBits, sizeof(DD_with_dsub0_in_FPR64_lo_and_DD_with_dsub1_in_FPR64_loBits) },
  { XSeqPairsClass_with_subo64_in_GPR64common, XSeqPairsClass_with_subo64_in_GPR64commonBits, sizeof(XSeqPairsClass_with_subo64_in_GPR64commonBits) },
  { XSeqPairsClass_with_subo64_in_GPR64noip, XSeqPairsClass_with_subo64_in_GPR64noipBits, sizeof(XSeqPairsClass_with_subo64_in_GPR64noipBits) },
  { XSeqPairsClass_with_sube64_in_GPR64noip, XSeqPairsClass_with_sube64_in_GPR64noipBits, sizeof(XSeqPairsClass_with_sube64_in_GPR64noipBits) },
  { XSeqPairsClass_with_sube64_in_tcGPR64, XSeqPairsClass_with_sube64_in_tcGPR64Bits, sizeof(XSeqPairsClass_with_sube64_in_tcGPR64Bits) },
  { XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64, XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64Bits, sizeof(XSeqPairsClass_with_sube64_in_GPR64noip_and_tcGPR64Bits) },
  { XSeqPairsClass_with_subo64_in_tcGPR64, XSeqPairsClass_with_subo64_in_tcGPR64Bits, sizeof(XSeqPairsClass_with_subo64_in_tcGPR64Bits) },
  { XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64, XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64Bits, sizeof(XSeqPairsClass_with_subo64_in_GPR64noip_and_tcGPR64Bits) },
  { XSeqPairsClass_with_sub_32_in_GPR32arg, XSeqPairsClass_with_sub_32_in_GPR32argBits, sizeof(XSeqPairsClass_with_sub_32_in_GPR32argBits) },
  { XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15, XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(XSeqPairsClass_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { XSeqPairsClass_with_sube64_in_rtcGPR64, XSeqPairsClass_with_sube64_in_rtcGPR64Bits, sizeof(XSeqPairsClass_with_sube64_in_rtcGPR64Bits) },
  { FPR128, FPR128Bits, sizeof(FPR128Bits) },
  { ZPR, ZPRBits, sizeof(ZPRBits) },
  { FPR128_lo, FPR128_loBits, sizeof(FPR128_loBits) },
  { MPR128, MPR128Bits, sizeof(MPR128Bits) },
  { ZPR_4b, ZPR_4bBits, sizeof(ZPR_4bBits) },
  { ZPR_3b, ZPR_3bBits, sizeof(ZPR_3bBits) },
  { DDD, DDDBits, sizeof(DDDBits) },
  { DDD_with_dsub0_in_FPR64_lo, DDD_with_dsub0_in_FPR64_loBits, sizeof(DDD_with_dsub0_in_FPR64_loBits) },
  { DDD_with_dsub1_in_FPR64_lo, DDD_with_dsub1_in_FPR64_loBits, sizeof(DDD_with_dsub1_in_FPR64_loBits) },
  { DDD_with_dsub2_in_FPR64_lo, DDD_with_dsub2_in_FPR64_loBits, sizeof(DDD_with_dsub2_in_FPR64_loBits) },
  { DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_lo, DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_loBits, sizeof(DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub1_in_FPR64_loBits) },
  { DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo, DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loBits, sizeof(DDD_with_dsub1_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loBits) },
  { DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_lo, DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loBits, sizeof(DDD_with_dsub0_in_FPR64_lo_and_DDD_with_dsub2_in_FPR64_loBits) },
  { DDDD, DDDDBits, sizeof(DDDDBits) },
  { DDDD_with_dsub0_in_FPR64_lo, DDDD_with_dsub0_in_FPR64_loBits, sizeof(DDDD_with_dsub0_in_FPR64_loBits) },
  { DDDD_with_dsub1_in_FPR64_lo, DDDD_with_dsub1_in_FPR64_loBits, sizeof(DDDD_with_dsub1_in_FPR64_loBits) },
  { DDDD_with_dsub2_in_FPR64_lo, DDDD_with_dsub2_in_FPR64_loBits, sizeof(DDDD_with_dsub2_in_FPR64_loBits) },
  { DDDD_with_dsub3_in_FPR64_lo, DDDD_with_dsub3_in_FPR64_loBits, sizeof(DDDD_with_dsub3_in_FPR64_loBits) },
  { DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_lo, DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_loBits, sizeof(DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub1_in_FPR64_loBits) },
  { DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo, DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loBits, sizeof(DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loBits) },
  { DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo, DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits, sizeof(DDDD_with_dsub2_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits) },
  { DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_lo, DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loBits, sizeof(DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub2_in_FPR64_loBits) },
  { DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo, DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits, sizeof(DDDD_with_dsub1_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits) },
  { DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_lo, DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits, sizeof(DDDD_with_dsub0_in_FPR64_lo_and_DDDD_with_dsub3_in_FPR64_loBits) },
  { QQ, QQBits, sizeof(QQBits) },
  { ZPR2, ZPR2Bits, sizeof(ZPR2Bits) },
  { QQ_with_dsub_in_FPR64_lo, QQ_with_dsub_in_FPR64_loBits, sizeof(QQ_with_dsub_in_FPR64_loBits) },
  { QQ_with_qsub1_in_FPR128_lo, QQ_with_qsub1_in_FPR128_loBits, sizeof(QQ_with_qsub1_in_FPR128_loBits) },
  { ZPR2_with_dsub_in_FPR64_lo, ZPR2_with_dsub_in_FPR64_loBits, sizeof(ZPR2_with_dsub_in_FPR64_loBits) },
  { ZPR2_with_zsub1_in_ZPR_4b, ZPR2_with_zsub1_in_ZPR_4bBits, sizeof(ZPR2_with_zsub1_in_ZPR_4bBits) },
  { QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_lo, QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_loBits, sizeof(QQ_with_dsub_in_FPR64_lo_and_QQ_with_qsub1_in_FPR128_loBits) },
  { ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4b, ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4bBits, sizeof(ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_4bBits) },
  { ZPR2_with_zsub0_in_ZPR_3b, ZPR2_with_zsub0_in_ZPR_3bBits, sizeof(ZPR2_with_zsub0_in_ZPR_3bBits) },
  { ZPR2_with_zsub1_in_ZPR_3b, ZPR2_with_zsub1_in_ZPR_3bBits, sizeof(ZPR2_with_zsub1_in_ZPR_3bBits) },
  { ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3b, ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3bBits, sizeof(ZPR2_with_dsub_in_FPR64_lo_and_ZPR2_with_zsub1_in_ZPR_3bBits) },
  { MPR64, MPR64Bits, sizeof(MPR64Bits) },
  { QQQ, QQQBits, sizeof(QQQBits) },
  { ZPR3, ZPR3Bits, sizeof(ZPR3Bits) },
  { QQQ_with_dsub_in_FPR64_lo, QQQ_with_dsub_in_FPR64_loBits, sizeof(QQQ_with_dsub_in_FPR64_loBits) },
  { QQQ_with_qsub1_in_FPR128_lo, QQQ_with_qsub1_in_FPR128_loBits, sizeof(QQQ_with_qsub1_in_FPR128_loBits) },
  { QQQ_with_qsub2_in_FPR128_lo, QQQ_with_qsub2_in_FPR128_loBits, sizeof(QQQ_with_qsub2_in_FPR128_loBits) },
  { ZPR3_with_dsub_in_FPR64_lo, ZPR3_with_dsub_in_FPR64_loBits, sizeof(ZPR3_with_dsub_in_FPR64_loBits) },
  { ZPR3_with_zsub1_in_ZPR_4b, ZPR3_with_zsub1_in_ZPR_4bBits, sizeof(ZPR3_with_zsub1_in_ZPR_4bBits) },
  { ZPR3_with_zsub2_in_ZPR_4b, ZPR3_with_zsub2_in_ZPR_4bBits, sizeof(ZPR3_with_zsub2_in_ZPR_4bBits) },
  { QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_lo, QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_loBits, sizeof(QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub1_in_FPR128_loBits) },
  { QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_lo, QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_loBits, sizeof(QQQ_with_qsub1_in_FPR128_lo_and_QQQ_with_qsub2_in_FPR128_loBits) },
  { ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4b, ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4bBits, sizeof(ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_4bBits) },
  { ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4b, ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4bBits, sizeof(ZPR3_with_zsub1_in_ZPR_4b_and_ZPR3_with_zsub2_in_ZPR_4bBits) },
  { QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_lo, QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_loBits, sizeof(QQQ_with_dsub_in_FPR64_lo_and_QQQ_with_qsub2_in_FPR128_loBits) },
  { ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4b, ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4bBits, sizeof(ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_4bBits) },
  { ZPR3_with_zsub0_in_ZPR_3b, ZPR3_with_zsub0_in_ZPR_3bBits, sizeof(ZPR3_with_zsub0_in_ZPR_3bBits) },
  { ZPR3_with_zsub1_in_ZPR_3b, ZPR3_with_zsub1_in_ZPR_3bBits, sizeof(ZPR3_with_zsub1_in_ZPR_3bBits) },
  { ZPR3_with_zsub2_in_ZPR_3b, ZPR3_with_zsub2_in_ZPR_3bBits, sizeof(ZPR3_with_zsub2_in_ZPR_3bBits) },
  { ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3b, ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3bBits, sizeof(ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub1_in_ZPR_3bBits) },
  { ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3b, ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3bBits, sizeof(ZPR3_with_zsub1_in_ZPR_3b_and_ZPR3_with_zsub2_in_ZPR_3bBits) },
  { ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3b, ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3bBits, sizeof(ZPR3_with_dsub_in_FPR64_lo_and_ZPR3_with_zsub2_in_ZPR_3bBits) },
  { QQQQ, QQQQBits, sizeof(QQQQBits) },
  { ZPR4, ZPR4Bits, sizeof(ZPR4Bits) },
  { QQQQ_with_dsub_in_FPR64_lo, QQQQ_with_dsub_in_FPR64_loBits, sizeof(QQQQ_with_dsub_in_FPR64_loBits) },
  { QQQQ_with_qsub1_in_FPR128_lo, QQQQ_with_qsub1_in_FPR128_loBits, sizeof(QQQQ_with_qsub1_in_FPR128_loBits) },
  { QQQQ_with_qsub2_in_FPR128_lo, QQQQ_with_qsub2_in_FPR128_loBits, sizeof(QQQQ_with_qsub2_in_FPR128_loBits) },
  { QQQQ_with_qsub3_in_FPR128_lo, QQQQ_with_qsub3_in_FPR128_loBits, sizeof(QQQQ_with_qsub3_in_FPR128_loBits) },
  { ZPR4_with_dsub_in_FPR64_lo, ZPR4_with_dsub_in_FPR64_loBits, sizeof(ZPR4_with_dsub_in_FPR64_loBits) },
  { ZPR4_with_zsub1_in_ZPR_4b, ZPR4_with_zsub1_in_ZPR_4bBits, sizeof(ZPR4_with_zsub1_in_ZPR_4bBits) },
  { ZPR4_with_zsub2_in_ZPR_4b, ZPR4_with_zsub2_in_ZPR_4bBits, sizeof(ZPR4_with_zsub2_in_ZPR_4bBits) },
  { ZPR4_with_zsub3_in_ZPR_4b, ZPR4_with_zsub3_in_ZPR_4bBits, sizeof(ZPR4_with_zsub3_in_ZPR_4bBits) },
  { QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_lo, QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_loBits, sizeof(QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub1_in_FPR128_loBits) },
  { QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_lo, QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_loBits, sizeof(QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub2_in_FPR128_loBits) },
  { QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo, QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loBits, sizeof(QQQQ_with_qsub2_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loBits) },
  { ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4b, ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4bBits, sizeof(ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_4bBits) },
  { ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4b, ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4bBits, sizeof(ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub2_in_ZPR_4bBits) },
  { ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b, ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bBits, sizeof(ZPR4_with_zsub2_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bBits) },
  { QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_lo, QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_loBits, sizeof(QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub2_in_FPR128_loBits) },
  { QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_lo, QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loBits, sizeof(QQQQ_with_qsub1_in_FPR128_lo_and_QQQQ_with_qsub3_in_FPR128_loBits) },
  { ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4b, ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4bBits, sizeof(ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_4bBits) },
  { ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4b, ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bBits, sizeof(ZPR4_with_zsub1_in_ZPR_4b_and_ZPR4_with_zsub3_in_ZPR_4bBits) },
  { QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_lo, QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_loBits, sizeof(QQQQ_with_dsub_in_FPR64_lo_and_QQQQ_with_qsub3_in_FPR128_loBits) },
  { ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4b, ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4bBits, sizeof(ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_4bBits) },
  { ZPR4_with_zsub0_in_ZPR_3b, ZPR4_with_zsub0_in_ZPR_3bBits, sizeof(ZPR4_with_zsub0_in_ZPR_3bBits) },
  { ZPR4_with_zsub1_in_ZPR_3b, ZPR4_with_zsub1_in_ZPR_3bBits, sizeof(ZPR4_with_zsub1_in_ZPR_3bBits) },
  { ZPR4_with_zsub2_in_ZPR_3b, ZPR4_with_zsub2_in_ZPR_3bBits, sizeof(ZPR4_with_zsub2_in_ZPR_3bBits) },
  { ZPR4_with_zsub3_in_ZPR_3b, ZPR4_with_zsub3_in_ZPR_3bBits, sizeof(ZPR4_with_zsub3_in_ZPR_3bBits) },
  { ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3b, ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3bBits, sizeof(ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub1_in_ZPR_3bBits) },
  { ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3b, ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3bBits, sizeof(ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub2_in_ZPR_3bBits) },
  { ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b, ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bBits, sizeof(ZPR4_with_zsub2_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bBits) },
  { ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3b, ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3bBits, sizeof(ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub2_in_ZPR_3bBits) },
  { ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3b, ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bBits, sizeof(ZPR4_with_zsub1_in_ZPR_3b_and_ZPR4_with_zsub3_in_ZPR_3bBits) },
  { ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3b, ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3bBits, sizeof(ZPR4_with_dsub_in_FPR64_lo_and_ZPR4_with_zsub3_in_ZPR_3bBits) },
  { GPR64x8Class, GPR64x8ClassBits, sizeof(GPR64x8ClassBits) },
  { GPR64x8Class_with_x8sub_0_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64, GPR64x8Class_with_x8sub_0_in_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_0_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_1_in_tcGPR64, GPR64x8Class_with_x8sub_1_in_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_1_in_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_1_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_4_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_3_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_0_in_tcGPR64_and_GPR64x8Class_with_x8sub_0_in_GPR64noip_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64noip_and_GPR64x8Class_with_x8sub_2_in_GPR64noip_and_GPR64x8Class_with_x8sub_1_in_tcGPR64_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_5_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_6_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64, GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_7_in_GPR64noip_and_tcGPR64Bits) },
  { GPR64x8Class_with_sub_32_in_GPR32arg, GPR64x8Class_with_sub_32_in_GPR32argBits, sizeof(GPR64x8Class_with_sub_32_in_GPR32argBits) },
  { MPR32, MPR32Bits, sizeof(MPR32Bits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64arg, GPR64x8Class_with_x8sub_2_in_GPR64argBits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64argBits) },
  { GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15, GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(GPR64x8Class_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15, GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(GPR64x8Class_with_x8sub_2_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15, GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64arg, GPR64x8Class_with_x8sub_4_in_GPR64argBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64argBits) },
  { GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15, GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15, GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits, sizeof(GPR64x8Class_with_sub_32_in_GPR32arg_and_GPR64x8Class_with_x8sub_6_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15Bits) },
  { GPR64x8Class_with_x8sub_0_in_rtcGPR64, GPR64x8Class_with_x8sub_0_in_rtcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_0_in_rtcGPR64Bits) },
  { GPR64x8Class_with_x8sub_2_in_rtcGPR64, GPR64x8Class_with_x8sub_2_in_rtcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_2_in_rtcGPR64Bits) },
  { GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noip, GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits, sizeof(GPR64x8Class_with_x8sub_4_in_GPR64_with_sub_32_in_MatrixIndexGPR32_12_15_and_GPR64x8Class_with_x8sub_6_in_GPR64noipBits) },
  { GPR64x8Class_with_x8sub_4_in_rtcGPR64, GPR64x8Class_with_x8sub_4_in_rtcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_4_in_rtcGPR64Bits) },
  { GPR64x8Class_with_x8sub_6_in_GPR64arg, GPR64x8Class_with_x8sub_6_in_GPR64argBits, sizeof(GPR64x8Class_with_x8sub_6_in_GPR64argBits) },
  { GPR64x8Class_with_x8sub_6_in_rtcGPR64, GPR64x8Class_with_x8sub_6_in_rtcGPR64Bits, sizeof(GPR64x8Class_with_x8sub_6_in_rtcGPR64Bits) },
  { MPR16, MPR16Bits, sizeof(MPR16Bits) },
  { MPR, MPRBits, sizeof(MPRBits) },
  { MPR8, MPR8Bits, sizeof(MPR8Bits) },
};

#endif // GET_REGINFO_MC_DESC

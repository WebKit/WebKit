
/* Capstone Disassembly Engine, http://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */

/*===- TableGen'erated file -------------------------------------*- C++ -*-===*|*                                                                            *|
|* GenSystemRegister Source Fragment                                          *|
|*                                                                            *|
|* Automatically generated file, do not edit!                                 *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/


enum BankedRegValues {
	elr_hyp = 0,
	lr_abt = 1,
	lr_fiq = 2,
	lr_irq = 3,
	lr_mon = 4,
	lr_svc = 5,
	lr_und = 6,
	lr_usr = 7,
	r10_fiq = 8,
	r10_usr = 9,
	r11_fiq = 10,
	r11_usr = 11,
	r12_fiq = 12,
	r12_usr = 13,
	r8_fiq = 14,
	r8_usr = 15,
	r9_fiq = 16,
	r9_usr = 17,
	sp_abt = 18,
	sp_fiq = 19,
	sp_hyp = 20,
	sp_irq = 21,
	sp_mon = 22,
	sp_svc = 23,
	sp_und = 24,
	sp_usr = 25,
	spsr_abt = 26,
	spsr_fiq = 27,
	spsr_hyp = 28,
	spsr_irq = 29,
	spsr_mon = 30,
	spsr_svc = 31,
	spsr_und = 32,
};

static const MClassSysReg MClassSysRegsList[] = {
  { "apsr_g", ARM_SYSREG_APSR_G, 0x400, 0x0, 0x400,  {ARM_FeatureDSP}  }, // 0
  { "apsr_nzcvqg", ARM_SYSREG_APSR_NZCVQG, 0xC00, 0x300, 0xC00,  {ARM_FeatureDSP}  }, // 1
  { "iapsr_g", ARM_SYSREG_IAPSR_G, 0x401, 0x1, 0x401,  {ARM_FeatureDSP}  }, // 2
  { "iapsr_nzcvqg", ARM_SYSREG_IAPSR_NZCVQG, 0xC01, 0x301, 0xC01,  {ARM_FeatureDSP}  }, // 3
  { "eapsr_g", ARM_SYSREG_EAPSR_G, 0x402, 0x2, 0x402,  {ARM_FeatureDSP}  }, // 4
  { "eapsr_nzcvqg", ARM_SYSREG_EAPSR_NZCVQG, 0xC02, 0x302, 0xC02,  {ARM_FeatureDSP}  }, // 5
  { "xpsr_g", ARM_SYSREG_XPSR_G, 0x403, 0x3, 0x403,  {ARM_FeatureDSP}  }, // 6
  { "xpsr_nzcvqg", ARM_SYSREG_XPSR_NZCVQG, 0xC03, 0x303, 0xC03,  {ARM_FeatureDSP}  }, // 7
  { "apsr", ARM_SYSREG_APSR, 0x800, 0x100, 0x800,  { 0 }  }, // 8
  { "apsr_nzcvq", ARM_SYSREG_APSR_NZCVQ, 0x1800, 0x200, 0x800,  { 0 }  }, // 9
  { "iapsr", ARM_SYSREG_IAPSR, 0x801, 0x101, 0x801,  { 0 }  }, // 10
  { "iapsr_nzcvq", ARM_SYSREG_IAPSR_NZCVQ, 0x1801, 0x201, 0x801,  { 0 }  }, // 11
  { "eapsr", ARM_SYSREG_EAPSR, 0x802, 0x102, 0x802,  { 0 }  }, // 12
  { "eapsr_nzcvq", ARM_SYSREG_EAPSR_NZCVQ, 0x1802, 0x202, 0x802,  { 0 }  }, // 13
  { "xpsr", ARM_SYSREG_XPSR, 0x803, 0x103, 0x803,  { 0 }  }, // 14
  { "xpsr_nzcvq", ARM_SYSREG_XPSR_NZCVQ, 0x1803, 0x203, 0x803,  { 0 }  }, // 15
  { "ipsr", ARM_SYSREG_IPSR, 0x805, 0x105, 0x805,  { 0 }  }, // 16
  { "epsr", ARM_SYSREG_EPSR, 0x806, 0x106, 0x806,  { 0 }  }, // 17
  { "iepsr", ARM_SYSREG_IEPSR, 0x807, 0x107, 0x807,  { 0 }  }, // 18
  { "msp", ARM_SYSREG_MSP, 0x808, 0x108, 0x808,  { 0 }  }, // 19
  { "psp", ARM_SYSREG_PSP, 0x809, 0x109, 0x809,  { 0 }  }, // 20
  { "msplim", ARM_SYSREG_MSPLIM, 0x80A, 0x10A, 0x80A,  {ARM_HasV8MBaselineOps}  }, // 21
  { "psplim", ARM_SYSREG_PSPLIM, 0x80B, 0x10B, 0x80B,  {ARM_HasV8MBaselineOps}  }, // 22
  { "primask", ARM_SYSREG_PRIMASK, 0x810, 0x110, 0x810,  { 0 }  }, // 23
  { "basepri", ARM_SYSREG_BASEPRI, 0x811, 0x111, 0x811,  {ARM_HasV7Ops}  }, // 24
  { "basepri_max", ARM_SYSREG_BASEPRI_MAX, 0x812, 0x112, 0x812,  {ARM_HasV7Ops}  }, // 25
  { "faultmask", ARM_SYSREG_FAULTMASK, 0x813, 0x113, 0x813,  {ARM_HasV7Ops}  }, // 26
  { "control", ARM_SYSREG_CONTROL, 0x814, 0x114, 0x814,  { 0 }  }, // 27
  { "msp_ns", ARM_SYSREG_MSP_NS, 0x888, 0x188, 0x888,  {ARM_Feature8MSecExt}  }, // 28
  { "psp_ns", ARM_SYSREG_PSP_NS, 0x889, 0x189, 0x889,  {ARM_Feature8MSecExt}  }, // 29
  { "msplim_ns", ARM_SYSREG_MSPLIM_NS, 0x88A, 0x18A, 0x88A,  {ARM_Feature8MSecExt, ARM_HasV8MBaselineOps}  }, // 30
  { "psplim_ns", ARM_SYSREG_PSPLIM_NS, 0x88B, 0x18B, 0x88B,  {ARM_Feature8MSecExt, ARM_HasV8MBaselineOps}  }, // 31
  { "primask_ns", ARM_SYSREG_PRIMASK_NS, 0x890, 0x190, 0x890,  { 0 }  }, // 32
  { "basepri_ns", ARM_SYSREG_BASEPRI_NS, 0x891, 0x191, 0x891,  {ARM_Feature8MSecExt, ARM_HasV7Ops}  }, // 33
  { "faultmask_ns", ARM_SYSREG_FAULTMASK_NS, 0x893, 0x193, 0x893,  {ARM_Feature8MSecExt, ARM_HasV7Ops}  }, // 34
  { "control_ns", ARM_SYSREG_CONTROL_NS, 0x894, 0x194, 0x894,  {ARM_Feature8MSecExt}  }, // 35
  { "sp_ns", ARM_SYSREG_SP_NS, 0x898, 0x198, 0x898,  {ARM_Feature8MSecExt}  }, // 36
};

static const BankedReg BankedRegsList[] = {
  { "r8_usr", ARM_SYSREG_R8_USR, 0x0 }, // 0
  { "r9_usr", ARM_SYSREG_R9_USR, 0x1 }, // 1
  { "r10_usr", ARM_SYSREG_R10_USR, 0x2 }, // 2
  { "r11_usr", ARM_SYSREG_R11_USR, 0x3 }, // 3
  { "r12_usr", ARM_SYSREG_R12_USR, 0x4 }, // 4
  { "sp_usr", ARM_SYSREG_SP_USR, 0x5 }, // 5
  { "lr_usr", ARM_SYSREG_LR_USR, 0x6 }, // 6
  { "r8_fiq", ARM_SYSREG_R8_FIQ, 0x8 }, // 7
  { "r9_fiq", ARM_SYSREG_R9_FIQ, 0x9 }, // 8
  { "r10_fiq", ARM_SYSREG_R10_FIQ, 0xA }, // 9
  { "r11_fiq", ARM_SYSREG_R11_FIQ, 0xB }, // 10
  { "r12_fiq", ARM_SYSREG_R12_FIQ, 0xC }, // 11
  { "sp_fiq", ARM_SYSREG_SP_FIQ, 0xD }, // 12
  { "lr_fiq", ARM_SYSREG_LR_FIQ, 0xE }, // 13
  { "lr_irq", ARM_SYSREG_LR_IRQ, 0x10 }, // 14
  { "sp_irq", ARM_SYSREG_SP_IRQ, 0x11 }, // 15
  { "lr_svc", ARM_SYSREG_LR_SVC, 0x12 }, // 16
  { "sp_svc", ARM_SYSREG_SP_SVC, 0x13 }, // 17
  { "lr_abt", ARM_SYSREG_LR_ABT, 0x14 }, // 18
  { "sp_abt", ARM_SYSREG_SP_ABT, 0x15 }, // 19
  { "lr_und", ARM_SYSREG_LR_UND, 0x16 }, // 20
  { "sp_und", ARM_SYSREG_SP_UND, 0x17 }, // 21
  { "lr_mon", ARM_SYSREG_LR_MON, 0x1C }, // 22
  { "sp_mon", ARM_SYSREG_SP_MON, 0x1D }, // 23
  { "elr_hyp", ARM_SYSREG_ELR_HYP, 0x1E }, // 24
  { "sp_hyp", ARM_SYSREG_SP_HYP, 0x1F }, // 25
  { "spsr_fiq", ARM_SYSREG_SPSR_FIQ, 0x2E }, // 26
  { "spsr_irq", ARM_SYSREG_SPSR_IRQ, 0x30 }, // 27
  { "spsr_svc", ARM_SYSREG_SPSR_SVC, 0x32 }, // 28
  { "spsr_abt", ARM_SYSREG_SPSR_ABT, 0x34 }, // 29
  { "spsr_und", ARM_SYSREG_SPSR_UND, 0x36 }, // 30
  { "spsr_mon", ARM_SYSREG_SPSR_MON, 0x3C }, // 31
  { "spsr_hyp", ARM_SYSREG_SPSR_HYP, 0x3E }, // 32
};

const MClassSysReg *lookupMClassSysRegByM2M3Encoding8(uint16_t encoding)
{
  unsigned int i;
  static const struct IndexType Index[] = {
    { 0x0, 0 },
    { 0x1, 2 },
    { 0x2, 4 },
    { 0x3, 6 },
    { 0x100, 8 },
    { 0x101, 10 },
    { 0x102, 12 },
    { 0x103, 14 },
    { 0x105, 16 },
    { 0x106, 17 },
    { 0x107, 18 },
    { 0x108, 19 },
    { 0x109, 20 },
    { 0x10A, 21 },
    { 0x10B, 22 },
    { 0x110, 23 },
    { 0x111, 24 },
    { 0x112, 25 },
    { 0x113, 26 },
    { 0x114, 27 },
    { 0x188, 28 },
    { 0x189, 29 },
    { 0x18A, 30 },
    { 0x18B, 31 },
    { 0x190, 32 },
    { 0x191, 33 },
    { 0x193, 34 },
    { 0x194, 35 },
    { 0x198, 36 },
    { 0x200, 9 },
    { 0x201, 11 },
    { 0x202, 13 },
    { 0x203, 15 },
    { 0x300, 1 },
    { 0x301, 3 },
    { 0x302, 5 },
    { 0x303, 7 },
  };

  i = binsearch_IndexTypeEncoding(Index, ARR_SIZE(Index), encoding);
  if (i == -1)
    return NULL;
  else
    return &MClassSysRegsList[Index[i].index];
}

const MClassSysReg *lookupMClassSysRegByM1Encoding12(uint16_t encoding)
{
  unsigned int i;
  static const struct IndexType Index[] = {
    { 0x400, 0 },
    { 0x401, 2 },
    { 0x402, 4 },
    { 0x403, 6 },
    { 0x800, 8 },
    { 0x801, 10 },
    { 0x802, 12 },
    { 0x803, 14 },
    { 0x805, 16 },
    { 0x806, 17 },
    { 0x807, 18 },
    { 0x808, 19 },
    { 0x809, 20 },
    { 0x80A, 21 },
    { 0x80B, 22 },
    { 0x810, 23 },
    { 0x811, 24 },
    { 0x812, 25 },
    { 0x813, 26 },
    { 0x814, 27 },
    { 0x888, 28 },
    { 0x889, 29 },
    { 0x88A, 30 },
    { 0x88B, 31 },
    { 0x890, 32 },
    { 0x891, 33 },
    { 0x893, 34 },
    { 0x894, 35 },
    { 0x898, 36 },
    { 0xC00, 1 },
    { 0xC01, 3 },
    { 0xC02, 5 },
    { 0xC03, 7 },
    { 0x1800, 9 },
    { 0x1801, 11 },
    { 0x1802, 13 },
    { 0x1803, 15 },
  };

  i = binsearch_IndexTypeEncoding(Index, ARR_SIZE(Index), encoding);
  if (i == -1)
    return NULL;
  else
    return &MClassSysRegsList[Index[i].index];
}

const BankedReg *lookupBankedRegByEncoding(uint8_t encoding)
{
  unsigned int i;
  static const struct IndexType Index[] = {
    { 0x0, 0 },
    { 0x1, 1 },
    { 0x2, 2 },
    { 0x3, 3 },
    { 0x4, 4 },
    { 0x5, 5 },
    { 0x6, 6 },
    { 0x8, 7 },
    { 0x9, 8 },
    { 0xA, 9 },
    { 0xB, 10 },
    { 0xC, 11 },
    { 0xD, 12 },
    { 0xE, 13 },
    { 0x10, 14 },
    { 0x11, 15 },
    { 0x12, 16 },
    { 0x13, 17 },
    { 0x14, 18 },
    { 0x15, 19 },
    { 0x16, 20 },
    { 0x17, 21 },
    { 0x1C, 22 },
    { 0x1D, 23 },
    { 0x1E, 24 },
    { 0x1F, 25 },
    { 0x2E, 26 },
    { 0x30, 27 },
    { 0x32, 28 },
    { 0x34, 29 },
    { 0x36, 30 },
    { 0x3C, 31 },
    { 0x3E, 32 },
  };

  i = binsearch_IndexTypeEncoding(Index, ARR_SIZE(Index), encoding);
  if (i == -1)
    return NULL;
  else
    return &BankedRegsList[Index[i].index];
}


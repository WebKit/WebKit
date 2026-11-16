{ MOS65XX_INS_BRK    , MOS65XX_AM_INT          , 1 }, // 0x00
{ MOS65XX_INS_ORA    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0x01
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x02
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x03
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x04
{ MOS65XX_INS_ORA    , MOS65XX_AM_ZP           , 1 }, // 0x05
{ MOS65XX_INS_ASL    , MOS65XX_AM_ZP           , 1 }, // 0x06
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x07
{ MOS65XX_INS_PHP    , MOS65XX_AM_IMP          , 0 }, // 0x08
{ MOS65XX_INS_ORA    , MOS65XX_AM_IMM          , 1 }, // 0x09
{ MOS65XX_INS_ASL    , MOS65XX_AM_ACC          , 0 }, // 0x0a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x0b
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x0c
{ MOS65XX_INS_ORA    , MOS65XX_AM_ABS          , 2 }, // 0x0d
{ MOS65XX_INS_ASL    , MOS65XX_AM_ABS          , 2 }, // 0x0e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x0f
{ MOS65XX_INS_BPL    , MOS65XX_AM_REL          , 1 }, // 0x10
{ MOS65XX_INS_ORA    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0x11
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x12
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x13
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x14
{ MOS65XX_INS_ORA    , MOS65XX_AM_ZP_X         , 1 }, // 0x15
{ MOS65XX_INS_ASL    , MOS65XX_AM_ZP_X         , 1 }, // 0x16
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x17
{ MOS65XX_INS_CLC    , MOS65XX_AM_IMP          , 0 }, // 0x18
{ MOS65XX_INS_ORA    , MOS65XX_AM_ABS_Y        , 2 }, // 0x19
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x1a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x1b
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x1c
{ MOS65XX_INS_ORA    , MOS65XX_AM_ABS_X        , 2 }, // 0x1d
{ MOS65XX_INS_ASL    , MOS65XX_AM_ABS_X        , 2 }, // 0x1e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x1f
{ MOS65XX_INS_JSR    , MOS65XX_AM_ABS          , 2 }, // 0x20
{ MOS65XX_INS_AND    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0x21
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x22
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x23
{ MOS65XX_INS_BIT    , MOS65XX_AM_ZP           , 1 }, // 0x24
{ MOS65XX_INS_AND    , MOS65XX_AM_ZP           , 1 }, // 0x25
{ MOS65XX_INS_ROL    , MOS65XX_AM_ZP           , 1 }, // 0x26
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x27
{ MOS65XX_INS_PLP    , MOS65XX_AM_IMP          , 0 }, // 0x28
{ MOS65XX_INS_AND    , MOS65XX_AM_IMM          , 1 }, // 0x29
{ MOS65XX_INS_ROL    , MOS65XX_AM_ACC          , 0 }, // 0x2a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x2b
{ MOS65XX_INS_BIT    , MOS65XX_AM_ABS          , 2 }, // 0x2c
{ MOS65XX_INS_AND    , MOS65XX_AM_ABS          , 2 }, // 0x2d
{ MOS65XX_INS_ROL    , MOS65XX_AM_ABS          , 2 }, // 0x2e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x2f
{ MOS65XX_INS_BMI    , MOS65XX_AM_REL          , 1 }, // 0x30
{ MOS65XX_INS_AND    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0x31
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x32
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x33
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x34
{ MOS65XX_INS_AND    , MOS65XX_AM_ZP_X         , 1 }, // 0x35
{ MOS65XX_INS_ROL    , MOS65XX_AM_ZP_X         , 1 }, // 0x36
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x37
{ MOS65XX_INS_SEC    , MOS65XX_AM_IMP          , 0 }, // 0x38
{ MOS65XX_INS_AND    , MOS65XX_AM_ABS_Y        , 2 }, // 0x39
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x3a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x3b
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x3c
{ MOS65XX_INS_AND    , MOS65XX_AM_ABS_X        , 2 }, // 0x3d
{ MOS65XX_INS_ROL    , MOS65XX_AM_ABS_X        , 2 }, // 0x3e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x3f
{ MOS65XX_INS_RTI    , MOS65XX_AM_IMP          , 0 }, // 0x40
{ MOS65XX_INS_EOR    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0x41
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x42
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x43
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x44
{ MOS65XX_INS_EOR    , MOS65XX_AM_ZP           , 1 }, // 0x45
{ MOS65XX_INS_LSR    , MOS65XX_AM_ZP           , 1 }, // 0x46
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x47
{ MOS65XX_INS_PHA    , MOS65XX_AM_IMP          , 0 }, // 0x48
{ MOS65XX_INS_EOR    , MOS65XX_AM_IMM          , 1 }, // 0x49
{ MOS65XX_INS_LSR    , MOS65XX_AM_ACC          , 0 }, // 0x4a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x4b
{ MOS65XX_INS_JMP    , MOS65XX_AM_ABS          , 2 }, // 0x4c
{ MOS65XX_INS_EOR    , MOS65XX_AM_ABS          , 2 }, // 0x4d
{ MOS65XX_INS_LSR    , MOS65XX_AM_ABS          , 2 }, // 0x4e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x4f
{ MOS65XX_INS_BVC    , MOS65XX_AM_REL          , 1 }, // 0x50
{ MOS65XX_INS_EOR    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0x51
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x52
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x53
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x54
{ MOS65XX_INS_EOR    , MOS65XX_AM_ZP_X         , 1 }, // 0x55
{ MOS65XX_INS_LSR    , MOS65XX_AM_ZP_X         , 1 }, // 0x56
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x57
{ MOS65XX_INS_CLI    , MOS65XX_AM_IMP          , 0 }, // 0x58
{ MOS65XX_INS_EOR    , MOS65XX_AM_ABS_Y        , 2 }, // 0x59
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x5a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x5b
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x5c
{ MOS65XX_INS_EOR    , MOS65XX_AM_ABS_X        , 2 }, // 0x5d
{ MOS65XX_INS_LSR    , MOS65XX_AM_ABS_X        , 2 }, // 0x5e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x5f
{ MOS65XX_INS_RTS    , MOS65XX_AM_IMP          , 0 }, // 0x60
{ MOS65XX_INS_ADC    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0x61
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x62
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x63
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x64
{ MOS65XX_INS_ADC    , MOS65XX_AM_ZP           , 1 }, // 0x65
{ MOS65XX_INS_ROR    , MOS65XX_AM_ZP           , 1 }, // 0x66
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x67
{ MOS65XX_INS_PLA    , MOS65XX_AM_IMP          , 0 }, // 0x68
{ MOS65XX_INS_ADC    , MOS65XX_AM_IMM          , 1 }, // 0x69
{ MOS65XX_INS_ROR    , MOS65XX_AM_ACC          , 0 }, // 0x6a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x6b
{ MOS65XX_INS_JMP    , MOS65XX_AM_ABS_IND      , 2 }, // 0x6c
{ MOS65XX_INS_ADC    , MOS65XX_AM_ABS          , 2 }, // 0x6d
{ MOS65XX_INS_ROR    , MOS65XX_AM_ABS          , 2 }, // 0x6e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x6f
{ MOS65XX_INS_BVS    , MOS65XX_AM_REL          , 1 }, // 0x70
{ MOS65XX_INS_ADC    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0x71
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x72
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x73
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x74
{ MOS65XX_INS_ADC    , MOS65XX_AM_ZP_X         , 1 }, // 0x75
{ MOS65XX_INS_ROR    , MOS65XX_AM_ZP_X         , 1 }, // 0x76
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x77
{ MOS65XX_INS_SEI    , MOS65XX_AM_IMP          , 0 }, // 0x78
{ MOS65XX_INS_ADC    , MOS65XX_AM_ABS_Y        , 2 }, // 0x79
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x7a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x7b
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x7c
{ MOS65XX_INS_ADC    , MOS65XX_AM_ABS_X        , 2 }, // 0x7d
{ MOS65XX_INS_ROR    , MOS65XX_AM_ABS_X        , 2 }, // 0x7e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x7f
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x80
{ MOS65XX_INS_STA    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0x81
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x82
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x83
{ MOS65XX_INS_STY    , MOS65XX_AM_ZP           , 1 }, // 0x84
{ MOS65XX_INS_STA    , MOS65XX_AM_ZP           , 1 }, // 0x85
{ MOS65XX_INS_STX    , MOS65XX_AM_ZP           , 1 }, // 0x86
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x87
{ MOS65XX_INS_DEY    , MOS65XX_AM_IMP          , 0 }, // 0x88
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x89
{ MOS65XX_INS_TXA    , MOS65XX_AM_IMP          , 0 }, // 0x8a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x8b
{ MOS65XX_INS_STY    , MOS65XX_AM_ABS          , 2 }, // 0x8c
{ MOS65XX_INS_STA    , MOS65XX_AM_ABS          , 2 }, // 0x8d
{ MOS65XX_INS_STX    , MOS65XX_AM_ABS          , 2 }, // 0x8e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x8f
{ MOS65XX_INS_BCC    , MOS65XX_AM_REL          , 1 }, // 0x90
{ MOS65XX_INS_STA    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0x91
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x92
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x93
{ MOS65XX_INS_STY    , MOS65XX_AM_ZP_X         , 1 }, // 0x94
{ MOS65XX_INS_STA    , MOS65XX_AM_ZP_X         , 1 }, // 0x95
{ MOS65XX_INS_STX    , MOS65XX_AM_ZP_Y         , 1 }, // 0x96
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x97
{ MOS65XX_INS_TYA    , MOS65XX_AM_IMP          , 0 }, // 0x98
{ MOS65XX_INS_STA    , MOS65XX_AM_ABS_Y        , 2 }, // 0x99
{ MOS65XX_INS_TXS    , MOS65XX_AM_IMP          , 0 }, // 0x9a
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x9b
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x9c
{ MOS65XX_INS_STA    , MOS65XX_AM_ABS_X        , 2 }, // 0x9d
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x9e
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0x9f
{ MOS65XX_INS_LDY    , MOS65XX_AM_IMM          , 1 }, // 0xa0
{ MOS65XX_INS_LDA    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0xa1
{ MOS65XX_INS_LDX    , MOS65XX_AM_IMM          , 1 }, // 0xa2
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xa3
{ MOS65XX_INS_LDY    , MOS65XX_AM_ZP           , 1 }, // 0xa4
{ MOS65XX_INS_LDA    , MOS65XX_AM_ZP           , 1 }, // 0xa5
{ MOS65XX_INS_LDX    , MOS65XX_AM_ZP           , 1 }, // 0xa6
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xa7
{ MOS65XX_INS_TAY    , MOS65XX_AM_IMP          , 0 }, // 0xa8
{ MOS65XX_INS_LDA    , MOS65XX_AM_IMM          , 1 }, // 0xa9
{ MOS65XX_INS_TAX    , MOS65XX_AM_IMP          , 0 }, // 0xaa
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xab
{ MOS65XX_INS_LDY    , MOS65XX_AM_ABS          , 2 }, // 0xac
{ MOS65XX_INS_LDA    , MOS65XX_AM_ABS          , 2 }, // 0xad
{ MOS65XX_INS_LDX    , MOS65XX_AM_ABS          , 2 }, // 0xae
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xaf
{ MOS65XX_INS_BCS    , MOS65XX_AM_REL          , 1 }, // 0xb0
{ MOS65XX_INS_LDA    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0xb1
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xb2
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xb3
{ MOS65XX_INS_LDY    , MOS65XX_AM_ZP_X         , 1 }, // 0xb4
{ MOS65XX_INS_LDA    , MOS65XX_AM_ZP_X         , 1 }, // 0xb5
{ MOS65XX_INS_LDX    , MOS65XX_AM_ZP_Y         , 1 }, // 0xb6
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xb7
{ MOS65XX_INS_CLV    , MOS65XX_AM_IMP          , 0 }, // 0xb8
{ MOS65XX_INS_LDA    , MOS65XX_AM_ABS_Y        , 2 }, // 0xb9
{ MOS65XX_INS_TSX    , MOS65XX_AM_IMP          , 0 }, // 0xba
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xbb
{ MOS65XX_INS_LDY    , MOS65XX_AM_ABS_X        , 2 }, // 0xbc
{ MOS65XX_INS_LDA    , MOS65XX_AM_ABS_X        , 2 }, // 0xbd
{ MOS65XX_INS_LDX    , MOS65XX_AM_ABS_Y        , 2 }, // 0xbe
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xbf
{ MOS65XX_INS_CPY    , MOS65XX_AM_IMM          , 1 }, // 0xc0
{ MOS65XX_INS_CMP    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0xc1
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xc2
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xc3
{ MOS65XX_INS_CPY    , MOS65XX_AM_ZP           , 1 }, // 0xc4
{ MOS65XX_INS_CMP    , MOS65XX_AM_ZP           , 1 }, // 0xc5
{ MOS65XX_INS_DEC    , MOS65XX_AM_ZP           , 1 }, // 0xc6
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xc7
{ MOS65XX_INS_INY    , MOS65XX_AM_IMP          , 0 }, // 0xc8
{ MOS65XX_INS_CMP    , MOS65XX_AM_IMM          , 1 }, // 0xc9
{ MOS65XX_INS_DEX    , MOS65XX_AM_IMP          , 0 }, // 0xca
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xcb
{ MOS65XX_INS_CPY    , MOS65XX_AM_ABS          , 2 }, // 0xcc
{ MOS65XX_INS_CMP    , MOS65XX_AM_ABS          , 2 }, // 0xcd
{ MOS65XX_INS_DEC    , MOS65XX_AM_ABS          , 2 }, // 0xce
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xcf
{ MOS65XX_INS_BNE    , MOS65XX_AM_REL          , 1 }, // 0xd0
{ MOS65XX_INS_CMP    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0xd1
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xd2
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xd3
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xd4
{ MOS65XX_INS_CMP    , MOS65XX_AM_ZP_X         , 1 }, // 0xd5
{ MOS65XX_INS_DEC    , MOS65XX_AM_ZP_X         , 1 }, // 0xd6
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xd7
{ MOS65XX_INS_CLD    , MOS65XX_AM_IMP          , 0 }, // 0xd8
{ MOS65XX_INS_CMP    , MOS65XX_AM_ABS_Y        , 2 }, // 0xd9
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xda
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xdb
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xdc
{ MOS65XX_INS_CMP    , MOS65XX_AM_ABS_X        , 2 }, // 0xdd
{ MOS65XX_INS_DEC    , MOS65XX_AM_ABS_X        , 2 }, // 0xde
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xdf
{ MOS65XX_INS_CPX    , MOS65XX_AM_IMM          , 1 }, // 0xe0
{ MOS65XX_INS_SBC    , MOS65XX_AM_ZP_X_IND     , 1 }, // 0xe1
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xe2
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xe3
{ MOS65XX_INS_CPX    , MOS65XX_AM_ZP           , 1 }, // 0xe4
{ MOS65XX_INS_SBC    , MOS65XX_AM_ZP           , 1 }, // 0xe5
{ MOS65XX_INS_INC    , MOS65XX_AM_ZP           , 1 }, // 0xe6
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xe7
{ MOS65XX_INS_INX    , MOS65XX_AM_IMP          , 0 }, // 0xe8
{ MOS65XX_INS_SBC    , MOS65XX_AM_IMM          , 1 }, // 0xe9
{ MOS65XX_INS_NOP    , MOS65XX_AM_IMP          , 0 }, // 0xea
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xeb
{ MOS65XX_INS_CPX    , MOS65XX_AM_ABS          , 2 }, // 0xec
{ MOS65XX_INS_SBC    , MOS65XX_AM_ABS          , 2 }, // 0xed
{ MOS65XX_INS_INC    , MOS65XX_AM_ABS          , 2 }, // 0xee
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xef
{ MOS65XX_INS_BEQ    , MOS65XX_AM_REL          , 1 }, // 0xf0
{ MOS65XX_INS_SBC    , MOS65XX_AM_ZP_IND_Y     , 1 }, // 0xf1
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xf2
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xf3
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xf4
{ MOS65XX_INS_SBC    , MOS65XX_AM_ZP_X         , 1 }, // 0xf5
{ MOS65XX_INS_INC    , MOS65XX_AM_ZP_X         , 1 }, // 0xf6
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xf7
{ MOS65XX_INS_SED    , MOS65XX_AM_IMP          , 0 }, // 0xf8
{ MOS65XX_INS_SBC    , MOS65XX_AM_ABS_Y        , 2 }, // 0xf9
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xfa
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xfb
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xfc
{ MOS65XX_INS_SBC    , MOS65XX_AM_ABS_X        , 2 }, // 0xfd
{ MOS65XX_INS_INC    , MOS65XX_AM_ABS_X        , 2 }, // 0xfe
{ MOS65XX_INS_INVALID, MOS65XX_AM_NONE         , 0 }, // 0xff

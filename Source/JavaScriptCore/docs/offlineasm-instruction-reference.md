# Offlineasm Instruction Reference

> **NOTE: This document was largely AI-generated based on offlineasm/\*.rb sources and offlineasm code in llint/\*.asm files. Exercise healthy skepticism.**

This document provides a comprehensive reference for the offlineasm assembly language used in JavaScriptCore's Low Level Interpreter (LLInt).

## Introduction

Offlineasm is a portable assembly language that is translated to native assembly for different architectures (ARM64, x86-64, RISC-V, etc.). Instructions are categorized by data size suffixes:
- `i` - 32-bit integer (word)
- `p` - pointer-sized integer (32 or 64-bit depending on platform)
- `q` - 64-bit integer (quad word)
- `f` - 32-bit floating-point (float)
- `d` - 64-bit floating-point (double)
- `b` - 8-bit (byte)
- `h` - 16-bit (half-word)
- `v` - vector/SIMD

## Naming Conventions

**Branching instructions** always begin with "b", and no non-branching instructions begin with "b".

**Terminal instructions** are `jmp` and `ret`.

## Operand Types

- **GPR** - General Purpose Register (e.g., `t0`, `t1`, `cfr`, `sp`, `lr`)
- **FPR** - Floating Point Register (e.g., `ft0`, `ft1`)
- **VecReg** - Vector Register (e.g., `v0`, `v1`)
- **Immediate** - Constant value
- **Address** - Memory address `[base, offset]`
- **BaseIndex** - Indexed memory address `[base, index, scale]`
- **Label** - Code label for branches/jumps

---

## Instruction Categories

### Arithmetic & Logical Operations
[abs{f,d}](#absf-absd) | [add{d,f,i,p,q}](#addd-addf-addi-addp-addq) | [and{d,f,i,p,q}](#andd-andf-andi-andp-andq) | [div{d,f}](#divd-divf) | [lshift{i,p,q}](#lshifti-lshiftp-lshiftq) | [lrotate{i,q}](#lrotatei-lrotateq) | [mul{d,f,i,p,q}](#muld-mulf-muli-mulp-mulq) | [neg{d,f,i,p,q}](#negd-negf-negi-negp-negq) | [noti](#noti) | [or{d,f,h,i,p,q}](#ord-orf-orh-ori-orp-orq) | [rrotate{i,q}](#rrotatei-rrotateq) | [rshift{i,p,q}](#rshifti-rshiftp-rshiftq) | [sqrt{d,f}](#sqrtd-sqrtf) | [sub{d,f,i,p,q}](#subd-subf-subi-subp-subq) | [urshift{i,p,q}](#urshifti-urshiftp-urshiftq) | [xor{i,p,q}](#xori-xorp-xorq)


### Branches (Conditional)
[baddio](#baddio) | [baddis](#baddis) | [baddiz](#baddiz) | [baddinz](#baddinz) | [baddpo](#baddpo) | [baddps](#baddps) | [baddpz](#baddpz) | [baddpnz](#baddpnz) | [baddqo](#baddqo) | [baddqs](#baddqs) | [baddqz](#baddqz) | [baddqnz](#baddqnz) | [bba](#bba) | [bbaeq](#bbaeq) | [bbb](#bbb) | [bbbeq](#bbbeq) | [bbeq](#bbeq) | [bbgt](#bbgt) | [bbgteq](#bbgteq) | [bblt](#bblt) | [bblteq](#bblteq) | [bbneq](#bbneq) | [bdeq](#bdeq) | [bdequn](#bdequn) | [bdgt](#bdgt) | [bdgteq](#bdgteq) | [bdgtequn](#bdgtequn) | [bdgtun](#bdgtun) | [bdlt](#bdlt) | [bdlteq](#bdlteq) | [bdltequn](#bdltequn) | [bdltun](#bdltun) | [bdneq](#bdneq) | [bdnequn](#bdnequn) | [bfeq](#bfeq) | [bfgt](#bfgt) | [bfgtequn](#bfgtequn) | [bfgtun](#bfgtun) | [bflt](#bflt) | [bfltequn](#bfltequn) | [bfltun](#bfltun) | [bia](#bia) | [biaeq](#biaeq) | [bib](#bib) | [bibeq](#bibeq) | [bieq](#bieq) | [bigt](#bigt) | [bigteq](#bigteq) | [bilt](#bilt) | [bilteq](#bilteq) | [bineq](#bineq) | [bmulio](#bmulio) | [bmulis](#bmulis) | [bmuliz](#bmuliz) | [bmulinz](#bmulinz) | [bnz](#bnz) | [bo](#bo) | [borio](#borio) | [borinz](#borinz) | [boris](#boris) | [boriz](#boriz) | [bpa](#bpa) | [bpaeq](#bpaeq) | [bpb](#bpb) | [bpbeq](#bpbeq) | [bpeq](#bpeq) | [bpgt](#bpgt) | [bpgteq](#bpgteq) | [bplt](#bplt) | [bplteq](#bplteq) | [bpneq](#bpneq) | [bqa](#bqa) | [bqaeq](#bqaeq) | [bqb](#bqb) | [bqbeq](#bqbeq) | [bqeq](#bqeq) | [bqgt](#bqgt) | [bqgteq](#bqgteq) | [bqlt](#bqlt) | [bqlteq](#bqlteq) | [bqneq](#bqneq) | [bs](#bs) | [bsubinz](#bsubinz) | [bsubio](#bsubio) | [bsubis](#bsubis) | [bsubiz](#bsubiz) | [btbnz](#btbnz) | [btbs](#btbs) | [btbz](#btbz) | [btd2i](#btd2i) | [btinz](#btinz) | [btis](#btis) | [btiz](#btiz) | [btpnz](#btpnz) | [btps](#btps) | [btpz](#btpz) | [btqnz](#btqnz) | [btqs](#btqs) | [btqz](#btqz) | [bz](#bz)

### Comparisons (Non-branching)
[cba](#cba) | [cbaeq](#cbaeq) | [cbb](#cbb) | [cbbeq](#cbbeq) | [cbeq](#cbeq) | [cbgt](#cbgt) | [cbgteq](#cbgteq) | [cblt](#cblt) | [cblteq](#cblteq) | [cbneq](#cbneq) | [cdeq](#cdeq) | [cdgt](#cdgt) | [cdgteq](#cdgteq) | [cdlt](#cdlt) | [cdlteq](#cdlteq) | [cdneq](#cdneq) | [cdnequn](#cdnequn) | [cfeq](#cfeq) | [cfgt](#cfgt) | [cfgteq](#cfgteq) | [cflt](#cflt) | [cflteq](#cflteq) | [cfneq](#cfneq) | [cfnequn](#cfnequn) | [cia](#cia) | [ciaeq](#ciaeq) | [cib](#cib) | [cibeq](#cibeq) | [cieq](#cieq) | [cigt](#cigt) | [cigteq](#cigteq) | [cilt](#cilt) | [cilteq](#cilteq) | [cineq](#cineq) | [cpa](#cpa) | [cpaeq](#cpaeq) | [cpb](#cpb) | [cpbeq](#cpbeq) | [cpeq](#cpeq) | [cpgt](#cpgt) | [cpgteq](#cpgteq) | [cplt](#cplt) | [cplteq](#cplteq) | [cpneq](#cpneq) | [cqa](#cqa) | [cqaeq](#cqaeq) | [cqb](#cqb) | [cqbeq](#cqbeq) | [cqeq](#cqeq) | [cqgt](#cqgt) | [cqgteq](#cqgteq) | [cqlt](#cqlt) | [cqlteq](#cqlteq) | [cqneq](#cqneq)

### Control Flow
[break](#break) | [call](#call) | [jmp](#jmp) | [ret](#ret)

### Conversions & Type Operations
[cd2f](#cd2f) | [cf2d](#cf2d) | [ci2{d,ds,f,fs}](#ci2d-ci2ds-ci2f-ci2fs) | [cq2{d,ds,f,fs}](#cq2d-cq2ds-cq2f-cq2fs) | [fd2ii](#fd2ii) | [fd2q](#fd2q) | [ff2i](#ff2i) | [fi2f](#fi2f) | [fii2d](#fii2d) | [fq2d](#fq2d) | [sx{b,h}2{i,p,q}](#sxb2i-sxb2p-sxb2q) | [sxi2q](#sxi2q) | [td2i](#td2i) | [transfer{i,p,q}](#transferi-transferp-transferq) | [truncate{d,f}](#truncated-truncatef) | [truncate{d,f}2{i,is,q,qs}](#truncated2i-truncated2is-truncated2q-truncated2qs) | [zxi2q](#zxi2q)


### Floating-Point Operations
[ceil{d,f}](#ceild-ceilf) | [floor{d,f}](#floord-floorf) | [round{d,f}](#roundd-roundf)

### Memory Operations
[leai](#leai) | [leap](#leap) | [load2ia](#load2ia) | [load{b,bsi,bsq}](#loadb-loadbsi-loadbsq) | [load{d,f,i,is,p,q,v}](#loadd-loadf-loadi-loadis-loadp-loadq-loadv) | [load{h,hsi,hsq}](#loadh-loadhsi-loadhsq) | [store2ia](#store2ia) | [store{b,d,f,h,i,p,q,v}](#storeb-stored-storef-storeh-storei-storep-storeq-storev)


### Misc Operations
[emit](#emit) | [lzcnt{i,q}](#lzcnti-lzcntq) | [memfence](#memfence) | [move](#move) | [moved](#moved) | [movdz](#movdz) | [nop](#nop) | [tbnz](#tbnz) | [tbs](#tbs) | [tbz](#tbz) | [tinz](#tinz) | [tis](#tis) | [tiz](#tiz) | [tpnz](#tpnz) | [tps](#tps) | [tpz](#tpz) | [tqnz](#tqnz) | [tqs](#tqs) | [tqz](#tqz) | [tzcnt{i,q}](#tzcnti-tzcntq)


### Pointer Authentication (ARM64e)
[removeArrayPtrTag](#removearrayptrtag) | [removeCodePtrTag](#removecodeptrtag) | [tagCodePtr](#tagcodeptr) | [tagReturnAddress](#tagreturnaddress) | [untagArrayPtr](#untagarrayptr) | [untagReturnAddress](#untagreturnaddress)

### Stack Operations
[peek](#peek) | [poke](#poke) | [pop](#pop) | [popv](#popv) | [push](#push) | [pushv](#pushv)

---

## Instruction Reference (Alphabetically)

### absf, absd

**Syntax:** `abs{f|d} <fpr>, <fpr>`

**Description:** Absolute value of floating-point number.

**Variants:**
- `absf` - single-precision float
- `absd` - double-precision float

**Operands:**
- Source: Floating-point register
- Destination: Floating-point register

**Effect:** `dest = abs(source)`

---

### addd, addf, addi, addp, addq

**Syntax:** `add{d|f|i|p|q} <src>, <dest>` or `add{d|f|i|p|q} <src1>, <src2>, <dest>`

**Description:** Add two values.

**Variants:**
- `addd` - double-precision floats (FPR operands)
- `addf` - single-precision floats (FPR operands)
- `addi` - 32-bit integers (GPR operands, immediate or GPR source)
- `addp` - pointer-sized integers (GPR operands, immediate or GPR source)
- `addq` - 64-bit integers (GPR operands, immediate or GPR source)

**Operands:**
- 2 operands: Source, Destination (in-place add)
- 3 operands: Source1, Source2, Destination

**Effect:** `dest = src1 + src2`

**Example Usage:**
```asm
addi 1, t0          # t0 = t0 + 1
addi t1, t0, t2     # t2 = t0 + t1
```

**ARM64 Translation:** `add/fadd` with appropriate size specifier

---

### andd, andf, andi, andp, andq

**Syntax:** `and{d|f|i|p|q} <src>, <dest>` or `and{d|f|i|p|q} <src1>, <src2>, <dest>`

**Description:** Bitwise AND of values.

**Variants:**
- `andd` - double-precision float bit patterns (FPR operands)
- `andf` - single-precision float bit patterns (FPR operands)
- `andi` - 32-bit integers (GPR operands, immediate or GPR source)
- `andp` - pointer-sized integers (GPR operands, immediate or GPR source)
- `andq` - 64-bit integers (GPR operands, immediate or GPR source)

**Operands:**
- 2 operands: Source, Destination (in-place AND)
- 3 operands: Source1, Source2, Destination

**Effect:** `dest = src1 & src2`

**Example Usage:**
```asm
andi 0xff, t0       # t0 = t0 & 0xff (mask low byte)
```

**ARM64 Translation:** `and` with appropriate size specifier

---

### baddio

**Syntax:** `baddio <gpr>, <gpr>, <label>`

**Description:** Branch if add of 32-bit integers overflows.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch to label if signed overflow occurs

**ARM64 Translation:** `adds` followed by `bvs`

---

### baddis

**Syntax:** `baddis <gpr>, <gpr>, <label>`

**Description:** Branch if add of 32-bit integers sets sign flag (result is negative).

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result < 0

---

### baddiz

**Syntax:** `baddiz <gpr>, <gpr>, <label>`

**Description:** Branch if add of 32-bit integers results in zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result == 0

---

### baddinz

**Syntax:** `baddinz <gpr>, <gpr>, <label>`

**Description:** Branch if add of 32-bit integers results in non-zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result != 0

---

### baddpo

**Syntax:** `baddpo <gpr>, <gpr>, <label>`

**Description:** Branch if add of pointer-sized integers overflows.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if signed overflow occurs

---

### baddps

**Syntax:** `baddps <gpr>, <gpr>, <label>`

**Description:** Branch if add of pointer-sized integers sets sign flag.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result < 0

---

### baddpz

**Syntax:** `baddpz <gpr>, <gpr>, <label>`

**Description:** Branch if add of pointer-sized integers results in zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result == 0

---

### baddpnz

**Syntax:** `baddpnz <gpr>, <gpr>, <label>`

**Description:** Branch if add of pointer-sized integers results in non-zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result != 0

---

### baddqo

**Syntax:** `baddqo <gpr>, <gpr>, <label>`

**Description:** Branch if add of 64-bit integers overflows.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if signed overflow occurs

---

### baddqs

**Syntax:** `baddqs <gpr>, <gpr>, <label>`

**Description:** Branch if add of 64-bit integers sets sign flag.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result < 0

---

### baddqz

**Syntax:** `baddqz <gpr>, <gpr>, <label>`

**Description:** Branch if add of 64-bit integers results in zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result == 0

---

### baddqnz

**Syntax:** `baddqnz <gpr>, <gpr>, <label>`

**Description:** Branch if add of 64-bit integers results in non-zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 + src2`; branch if result != 0

---

### bbeq

**Syntax:** `bbeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if bytes are equal (unsigned 8-bit comparison).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 == src2`

---

### bbneq

**Syntax:** `bbneq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if bytes are not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 != src2`

---

### bba

**Syntax:** `bba <gpr>, <gpr|imm>, <label>`

**Description:** Branch if byte is above (unsigned >).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (unsigned)

---

### bbaeq

**Syntax:** `bbaeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if byte is above or equal (unsigned >=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (unsigned)

---

### bbb

**Syntax:** `bbb <gpr>, <gpr|imm>, <label>`

**Description:** Branch if byte is below (unsigned <).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (unsigned)

---

### bbbeq

**Syntax:** `bbbeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if byte is below or equal (unsigned <=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (unsigned)

---

### bbgt

**Syntax:** `bbgt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed byte is greater than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (signed)

---

### bbgteq

**Syntax:** `bbgteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed byte is greater than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (signed)

---

### bblt

**Syntax:** `bblt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed byte is less than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (signed)

---

### bblteq

**Syntax:** `bblteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed byte is less than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (signed)

---

### bdeq

**Syntax:** `bdeq <fpr>, <fpr>, <label>`

**Description:** Branch if double-precision floats are equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 == src2`

---

### bdneq

**Syntax:** `bdneq <fpr>, <fpr>, <label>`

**Description:** Branch if double-precision floats are not equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 != src2`

---

### bdgt

**Syntax:** `bdgt <fpr>, <fpr>, <label>`

**Description:** Branch if double > (ordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 > src2` and neither is NaN

---

### bdgteq

**Syntax:** `bdgteq <fpr>, <fpr>, <label>`

**Description:** Branch if double >= (ordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 >= src2` and neither is NaN

---

### bdlt

**Syntax:** `bdlt <fpr>, <fpr>, <label>`

**Description:** Branch if double < (ordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 < src2` and neither is NaN

---

### bdlteq

**Syntax:** `bdlteq <fpr>, <fpr>, <label>`

**Description:** Branch if double <= (ordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 <= src2` and neither is NaN

---

### bdequn

**Syntax:** `bdequn <fpr>, <fpr>, <label>`

**Description:** Branch if double == (unordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 == src2` or either is NaN

---

### bdnequn

**Syntax:** `bdnequn <fpr>, <fpr>, <label>`

**Description:** Branch if double != (unordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 != src2` or either is NaN

---

### bdgtun

**Syntax:** `bdgtun <fpr>, <fpr>, <label>`

**Description:** Branch if double > (unordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 > src2` or either is NaN

---

### bdgtequn

**Syntax:** `bdgtequn <fpr>, <fpr>, <label>`

**Description:** Branch if double >= (unordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 >= src2` or either is NaN

---

### bdltun

**Syntax:** `bdltun <fpr>, <fpr>, <label>`

**Description:** Branch if double < (unordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 < src2` or either is NaN

---

### bdltequn

**Syntax:** `bdltequn <fpr>, <fpr>, <label>`

**Description:** Branch if double <= (unordered comparison).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 <= src2` or either is NaN

---

### bfeq

**Syntax:** `bfeq <fpr>, <fpr>, <label>`

**Description:** Branch if single-precision floats are equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 == src2`

---

### bfgt

**Syntax:** `bfgt <fpr>, <fpr>, <label>`

**Description:** Branch if float > (ordered).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 > src2` and neither is NaN

---

### bflt

**Syntax:** `bflt <fpr>, <fpr>, <label>`

**Description:** Branch if float < (ordered).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch to label if `src1 < src2` and neither is NaN

---

### bfgtun

**Syntax:** `bfgtun <fpr>, <fpr>, <label>`

**Description:** Branch if float > (unordered).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 > src2` or either is NaN

---

### bfgtequn

**Syntax:** `bfgtequn <fpr>, <fpr>, <label>`

**Description:** Branch if float >= (unordered).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 >= src2` or either is NaN

---

### bfltun

**Syntax:** `bfltun <fpr>, <fpr>, <label>`

**Description:** Branch if float < (unordered).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 < src2` or either is NaN

---

### bfltequn

**Syntax:** `bfltequn <fpr>, <fpr>, <label>`

**Description:** Branch if float <= (unordered).

**Operands:**
- Source1: FPR
- Source2: FPR
- Target: Label

**Effect:** Branch if `src1 <= src2` or either is NaN

---

### bieq

**Syntax:** `bieq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 32-bit integers are equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 == src2`

**Example Usage:**
```asm
bieq t0, 0, .isZero
bieq t0, t1, .areEqual
```

---

### bineq

**Syntax:** `bineq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 32-bit integers are not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 != src2`

---

### bia

**Syntax:** `bia <gpr>, <gpr|imm>, <label>`

**Description:** Branch if integer above (unsigned >).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (unsigned 32-bit)

---

### biaeq

**Syntax:** `biaeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if integer above or equal (unsigned >=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (unsigned 32-bit)

---

### bib

**Syntax:** `bib <gpr>, <gpr|imm>, <label>`

**Description:** Branch if integer below (unsigned <).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (unsigned 32-bit)

---

### bibeq

**Syntax:** `bibeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if integer below or equal (unsigned <=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (unsigned 32-bit)

---

### bigt

**Syntax:** `bigt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed integer is greater than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (signed 32-bit)

---

### bigteq

**Syntax:** `bigteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed integer is greater than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (signed 32-bit)

---

### bilt

**Syntax:** `bilt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed integer is less than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (signed 32-bit)

---

### bilteq

**Syntax:** `bilteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed integer is less than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (signed 32-bit)

---

### bmulio

**Syntax:** `bmulio <gpr>, <gpr>, <label>`

**Description:** Branch if multiply of 32-bit integers overflows.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 * src2`; branch if signed overflow

---

### bmulis

**Syntax:** `bmulis <gpr>, <gpr>, <label>`

**Description:** Branch if multiply of 32-bit integers sets sign flag.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 * src2`; branch if result < 0

---

### bmuliz

**Syntax:** `bmuliz <gpr>, <gpr>, <label>`

**Description:** Branch if multiply of 32-bit integers results in zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 * src2`; branch if result == 0

---

### bmulinz

**Syntax:** `bmulinz <gpr>, <gpr>, <label>`

**Description:** Branch if multiply of 32-bit integers results in non-zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 * src2`; branch if result != 0

---

### bo

**Syntax:** `bo <label>`

**Description:** Branch if overflow flag is set.

**Operands:**
- Target: Label

**Effect:** Branch based on previous arithmetic operation's overflow status

---

### borio

**Syntax:** `borio <gpr>, <gpr>, <label>`

**Description:** Branch if OR of 32-bit integers would overflow (always false, but modifies destination).

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 | src2`; branch if overflow (never branches, OR cannot overflow)

---

### boris

**Syntax:** `boris <gpr>, <gpr>, <label>`

**Description:** Branch if OR of 32-bit integers sets sign flag.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 | src2`; branch if result < 0

---

### boriz

**Syntax:** `boriz <gpr>, <gpr>, <label>`

**Description:** Branch if OR of 32-bit integers results in zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 | src2`; branch if result == 0

---

### borinz

**Syntax:** `borinz <gpr>, <gpr>, <label>`

**Description:** Branch if OR of 32-bit integers results in non-zero.

**Operands:**
- Source1: GPR
- Source2: GPR (also destination)
- Target: Label

**Effect:** `src2 = src1 | src2`; branch if result != 0

---

### bpeq

**Syntax:** `bpeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if pointers are equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 == src2`

---

### bpneq

**Syntax:** `bpneq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if pointers are not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 != src2`

---

### bpa

**Syntax:** `bpa <gpr>, <gpr|imm>, <label>`

**Description:** Branch if pointer above (unsigned >).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (unsigned)

---

### bpaeq

**Syntax:** `bpaeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if pointer above or equal (unsigned >=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (unsigned)

---

### bpb

**Syntax:** `bpb <gpr>, <gpr|imm>, <label>`

**Description:** Branch if pointer below (unsigned <).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (unsigned)

---

### bpbeq

**Syntax:** `bpbeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if pointer below or equal (unsigned <=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (unsigned)

---

### bpgt

**Syntax:** `bpgt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed pointer greater than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (signed)

---

### bpgteq

**Syntax:** `bpgteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed pointer greater than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (signed)

---

### bplt

**Syntax:** `bplt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed pointer less than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (signed)

---

### bplteq

**Syntax:** `bplteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed pointer less than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (signed)

---

### bqeq

**Syntax:** `bqeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 64-bit integers are equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 == src2`

---

### bqneq

**Syntax:** `bqneq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 64-bit integers are not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 != src2`

---

### bqa

**Syntax:** `bqa <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 64-bit integer above (unsigned >).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (unsigned)

---

### bqaeq

**Syntax:** `bqaeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 64-bit integer above or equal (unsigned >=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (unsigned)

---

### bqb

**Syntax:** `bqb <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 64-bit integer below (unsigned <).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (unsigned)

---

### bqbeq

**Syntax:** `bqbeq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if 64-bit integer below or equal (unsigned <=).

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (unsigned)

---

### bqgt

**Syntax:** `bqgt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed 64-bit integer greater than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 > src2` (signed)

---

### bqgteq

**Syntax:** `bqgteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed 64-bit integer greater than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 >= src2` (signed)

---

### bqlt

**Syntax:** `bqlt <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed 64-bit integer less than.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 < src2` (signed)

---

### bqlteq

**Syntax:** `bqlteq <gpr>, <gpr|imm>, <label>`

**Description:** Branch if signed 64-bit integer less than or equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Target: Label

**Effect:** Branch to label if `src1 <= src2` (signed)

---

### break

**Syntax:** `break`

**Description:** Breakpoint/trap instruction for debugging.

**Effect:** Causes a debug trap/breakpoint

**ARM64 Translation:** `brk #0`

---

### bs

**Syntax:** `bs <label>`

**Description:** Branch if sign flag is set.

**Operands:**
- Target: Label

**Effect:** Branch based on previous operation's sign flag

---

### bsubio

**Syntax:** `bsubio <gpr>, <gpr>, <label>`

**Description:** Branch if subtract of 32-bit integers overflows.

**Operands:**
- Source1: GPR (subtrahend)
- Source2: GPR (minuend and destination)
- Target: Label

**Effect:** `src2 = src2 - src1`; branch if signed overflow

---

### bsubis

**Syntax:** `bsubis <gpr>, <gpr>, <label>`

**Description:** Branch if subtract of 32-bit integers sets sign flag.

**Operands:**
- Source1: GPR (subtrahend)
- Source2: GPR (minuend and destination)
- Target: Label

**Effect:** `src2 = src2 - src1`; branch if result < 0

---

### bsubiz

**Syntax:** `bsubiz <gpr>, <gpr>, <label>`

**Description:** Branch if subtract of 32-bit integers results in zero.

**Operands:**
- Source1: GPR (subtrahend)
- Source2: GPR (minuend and destination)
- Target: Label

**Effect:** `src2 = src2 - src1`; branch if result == 0

---

### bsubinz

**Syntax:** `bsubinz <gpr>, <gpr>, <label>`

**Description:** Branch if subtract of 32-bit integers results in non-zero.

**Operands:**
- Source1: GPR (subtrahend)
- Source2: GPR (minuend and destination)
- Target: Label

**Effect:** `src2 = src2 - src1`; branch if result != 0

---

### btbs

**Syntax:** `btbs <gpr>, <imm>, <label>`

**Description:** Branch and test byte if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) < 0` (sign bit of masked result is set)

---

### btbz

**Syntax:** `btbz <gpr>, <imm>, <label>`

**Description:** Branch and test byte if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) == 0`

---

### btbnz

**Syntax:** `btbnz <gpr>, <imm>, <label>`

**Description:** Branch and test byte if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) != 0`

---

### btd2i

**Syntax:** `btd2i <fpr>, <gpr>, <label>`

**Description:** Branch if truncate double to int fails, otherwise convert.

**Operands:**
- Source: FPR (double)
- Destination: GPR (32-bit int)
- Target: Label

**Effect:** Convert double to 32-bit int; branch if out of range

---

### btis

**Syntax:** `btis <gpr>, <imm>, <label>`

**Description:** Branch and test 32-bit integer if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) < 0`

---

### btiz

**Syntax:** `btiz <gpr>, <imm>, <label>`

**Description:** Branch and test 32-bit integer if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) == 0`

---

### btinz

**Syntax:** `btinz <gpr>, <imm>, <label>`

**Description:** Branch and test 32-bit integer if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) != 0`

**Example Usage:**
```asm
btinz t0, 0x1, .isOdd    # Branch if low bit is set
```

---

### btps

**Syntax:** `btps <gpr>, <imm>, <label>`

**Description:** Branch and test pointer if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) < 0`

---

### btpz

**Syntax:** `btpz <gpr>, <imm>, <label>`

**Description:** Branch and test pointer if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) == 0`

---

### btpnz

**Syntax:** `btpnz <gpr>, <imm>, <label>`

**Description:** Branch and test pointer if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) != 0`

---

### btqs

**Syntax:** `btqs <gpr>, <imm>, <label>`

**Description:** Branch and test 64-bit integer if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) < 0`

---

### btqz

**Syntax:** `btqz <gpr>, <imm>, <label>`

**Description:** Branch and test 64-bit integer if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) == 0`

---

### btqnz

**Syntax:** `btqnz <gpr>, <imm>, <label>`

**Description:** Branch and test 64-bit integer if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Target: Label

**Effect:** Branch if `(src & mask) != 0`

---

### bz

**Syntax:** `bz <label>`

**Description:** Branch if zero flag is set.

**Operands:**
- Target: Label

**Effect:** Branch based on previous operation's zero flag

---

### bnz

**Syntax:** `bnz <label>`

**Description:** Branch if zero flag is not set.

**Operands:**
- Target: Label

**Effect:** Branch based on previous operation's zero flag

---

### call

**Syntax:** `call <label|gpr>`

**Description:** Call a function.

**Operands:**
- Target: Label or GPR containing function address

**Effect:** Push return address, jump to target

**ARM64 Translation:** `bl <label>` or `blr <register>`

---

### cbeq

**Syntax:** `cbeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare bytes and set destination to 1 if equal, 0 otherwise.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 == src2) ? 1 : 0`

---

### cbneq

**Syntax:** `cbneq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare bytes and set destination to 1 if not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 != src2) ? 1 : 0`

---

### cba

**Syntax:** `cba <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare bytes (unsigned >) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (unsigned)

---

### cbaeq

**Syntax:** `cbaeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare bytes (unsigned >=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (unsigned)

---

### cbb

**Syntax:** `cbb <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare bytes (unsigned <) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (unsigned)

---

### cbbeq

**Syntax:** `cbbeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare bytes (unsigned <=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (unsigned)

---

### cbgt

**Syntax:** `cbgt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed bytes (>) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (signed)

---

### cbgteq

**Syntax:** `cbgteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed bytes (>=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (signed)

---

### cblt

**Syntax:** `cblt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed bytes (<) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (signed)

---

### cblteq

**Syntax:** `cblteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed bytes (<=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (signed)

---

### cd2f

**Syntax:** `cd2f <fpr>, <fpr>`

**Description:** Convert double to float.

**Operands:**
- Source: FPR (double)
- Destination: FPR (float)

**Effect:** `dest_float = (float)src_double`

---

### cdeq

**Syntax:** `cdeq <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles and set destination to 1 if equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 == src2) ? 1 : 0`

---

### cdlt

**Syntax:** `cdlt <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles (<) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0`

---

### cdlteq

**Syntax:** `cdlteq <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles (<=) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0`

---

### cdgt

**Syntax:** `cdgt <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles (>) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0`

---

### cdgteq

**Syntax:** `cdgteq <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles (>=) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0`

---

### cdneq

**Syntax:** `cdneq <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles and set destination to 1 if not equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 != src2) ? 1 : 0`

---

### cdnequn

**Syntax:** `cdnequn <fpr>, <fpr>, <gpr>`

**Description:** Compare doubles (unordered !=) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 != src2 || isNaN(src1) || isNaN(src2)) ? 1 : 0`

---

### ceild, ceilf

**Syntax:** `ceil{d|f} <src>, <dest>`

**Description:** Ceiling function (round toward +infinity).

**Variants:**
- `ceild` - double-precision float
- `ceilf` - single-precision float

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = ceil(src)`

**ARM64 Translation:** `frintp` with appropriate size specifier

---

### cf2d

**Syntax:** `cf2d <fpr>, <fpr>`

**Description:** Convert float to double.

**Operands:**
- Source: FPR (float)
- Destination: FPR (double)

**Effect:** `dest_double = (double)src_float`

---

### cfeq

**Syntax:** `cfeq <fpr>, <fpr>, <gpr>`

**Description:** Compare floats and set destination to 1 if equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 == src2) ? 1 : 0`

---

### cflt

**Syntax:** `cflt <fpr>, <fpr>, <gpr>`

**Description:** Compare floats (<) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0`

---

### cflteq

**Syntax:** `cflteq <fpr>, <fpr>, <gpr>`

**Description:** Compare floats (<=) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0`

---

### cfgt

**Syntax:** `cfgt <fpr>, <fpr>, <gpr>`

**Description:** Compare floats (>) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0`

---

### cfgteq

**Syntax:** `cfgteq <fpr>, <fpr>, <gpr>`

**Description:** Compare floats (>=) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0`

---

### cfneq

**Syntax:** `cfneq <fpr>, <fpr>, <gpr>`

**Description:** Compare floats and set destination to 1 if not equal.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 != src2) ? 1 : 0`

---

### cfnequn

**Syntax:** `cfnequn <fpr>, <fpr>, <gpr>`

**Description:** Compare floats (unordered !=) and set boolean result.

**Operands:**
- Source1: FPR
- Source2: FPR
- Destination: GPR

**Effect:** `dest = (src1 != src2 || isNaN(src1) || isNaN(src2)) ? 1 : 0`

---

### ci2d, ci2ds, ci2f, ci2fs

**Syntax:** `ci2{d|ds|f|fs} <src>, <dest>`

**Description:** Convert signed 32-bit integer to floating-point.

**Variants:**
- `ci2d` - to double-precision
- `ci2ds` - to double-precision with saturation
- `ci2f` - to single-precision
- `ci2fs` - to single-precision with saturation

**Operands:**
- Source: GPR (32-bit int)
- Destination: FPR

**Effect:** `dest_float = (float_type)src_int`

**ARM64 Translation:** `scvtf` with appropriate size specifier

---

### cieq

**Syntax:** `cieq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 32-bit integers and set destination to 1 if equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 == src2) ? 1 : 0`

---

### cineq

**Syntax:** `cineq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 32-bit integers and set destination to 1 if not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 != src2) ? 1 : 0`

---

### cia

**Syntax:** `cia <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare integers (unsigned >) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (unsigned)

---

### ciaeq

**Syntax:** `ciaeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare integers (unsigned >=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (unsigned)

---

### cib

**Syntax:** `cib <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare integers (unsigned <) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (unsigned)

---

### cibeq

**Syntax:** `cibeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare integers (unsigned <=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (unsigned)

---

### cigt

**Syntax:** `cigt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed integers (>) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (signed)

---

### cigteq

**Syntax:** `cigteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed integers (>=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (signed)

---

### cilt

**Syntax:** `cilt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed integers (<) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (signed)

---

### cilteq

**Syntax:** `cilteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed integers (<=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (signed)

---

### cpeq

**Syntax:** `cpeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare pointers and set destination to 1 if equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 == src2) ? 1 : 0`

---

### cpneq

**Syntax:** `cpneq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare pointers and set destination to 1 if not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 != src2) ? 1 : 0`

---

### cpa

**Syntax:** `cpa <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare pointers (unsigned >) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (unsigned)

---

### cpaeq

**Syntax:** `cpaeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare pointers (unsigned >=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (unsigned)

---

### cpb

**Syntax:** `cpb <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare pointers (unsigned <) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (unsigned)

---

### cpbeq

**Syntax:** `cpbeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare pointers (unsigned <=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (unsigned)

---

### cpgt

**Syntax:** `cpgt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed pointers (>) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (signed)

---

### cpgteq

**Syntax:** `cpgteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed pointers (>=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (signed)

---

### cplt

**Syntax:** `cplt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed pointers (<) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (signed)

---

### cplteq

**Syntax:** `cplteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed pointers (<=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (signed)

---

### cq2d, cq2ds, cq2f, cq2fs

**Syntax:** `cq2{d|ds|f|fs} <src>, <dest>`

**Description:** Convert signed 64-bit integer to floating-point.

**Variants:**
- `cq2d` - to double-precision
- `cq2ds` - to double-precision with saturation
- `cq2f` - to single-precision
- `cq2fs` - to single-precision with saturation

**Operands:**
- Source: GPR (64-bit int)
- Destination: FPR

**Effect:** `dest_float = (float_type)src_int64`

---

### cqeq

**Syntax:** `cqeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 64-bit integers and set destination to 1 if equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 == src2) ? 1 : 0`

---

### cqneq

**Syntax:** `cqneq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 64-bit integers and set destination to 1 if not equal.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 != src2) ? 1 : 0`

---

### cqa

**Syntax:** `cqa <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 64-bit integers (unsigned >) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (unsigned)

---

### cqaeq

**Syntax:** `cqaeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 64-bit integers (unsigned >=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (unsigned)

---

### cqb

**Syntax:** `cqb <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 64-bit integers (unsigned <) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (unsigned)

---

### cqbeq

**Syntax:** `cqbeq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare 64-bit integers (unsigned <=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (unsigned)

---

### cqgt

**Syntax:** `cqgt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed 64-bit integers (>) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 > src2) ? 1 : 0` (signed)

---

### cqgteq

**Syntax:** `cqgteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed 64-bit integers (>=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 >= src2) ? 1 : 0` (signed)

---

### cqlt

**Syntax:** `cqlt <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed 64-bit integers (<) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 < src2) ? 1 : 0` (signed)

---

### cqlteq

**Syntax:** `cqlteq <gpr>, <gpr|imm>, <gpr>`

**Description:** Compare signed 64-bit integers (<=) and set boolean result.

**Operands:**
- Source1: GPR
- Source2: GPR or Immediate
- Destination: GPR

**Effect:** `dest = (src1 <= src2) ? 1 : 0` (signed)

---

### divd, divf

**Syntax:** `div{d|f} <src>, <dest>` or `div{d|f} <src1>, <src2>, <dest>`

**Description:** Divide floating-point numbers.

**Variants:**
- `divd` - double-precision floats
- `divf` - single-precision floats

**Operands:**
- 2 operands: Source FPR, Destination FPR (in-place divide)
- 3 operands: Source1 FPR, Source2 FPR, Destination FPR

**Effect:** `dest = src1 / src2`

**ARM64 Translation:** `fdiv` with appropriate size specifier

---

### emit

**Syntax:** `emit <string>`

**Description:** Emit raw assembly code.

**Operands:**
- Code: String containing raw assembly

**Effect:** Emits the string directly to the output assembly

**Example Usage:**
```asm
emit "nop"
```

---

### fd2ii

**Syntax:** `fd2ii <fpr>, <gpr>, <gpr>`

**Description:** Convert double to two 32-bit integers (LSB and MSB).

**Operands:**
- Source: FPR
- Dest LSB: GPR (least significant 32 bits)
- Dest MSB: GPR (most significant 32 bits)

**Effect:** Extracts the 64-bit double bit pattern into two 32-bit registers

---

### fd2q

**Syntax:** `fd2q <fpr>, <gpr>`

**Description:** Move double-precision float bit pattern to 64-bit integer register.

**Operands:**
- Source: FPR (double)
- Destination: GPR (64-bit)

**Effect:** `dest_int64 = bitcast<int64>(src_double)`

**ARM64 Translation:** `fmov x<dest>, d<src>`

---

### fi2f

**Syntax:** `fi2f <gpr>, <fpr>`

**Description:** Move 32-bit integer bit pattern to float register.

**Operands:**
- Source: GPR (32-bit int)
- Destination: FPR (float)

**Effect:** `dest_float = bitcast<float>(src_int32)`

---

### fii2d

**Syntax:** `fii2d <gpr>, <gpr>, <fpr>`

**Description:** Combine two 32-bit integers into a double.

**Operands:**
- Source LSB: GPR (least significant 32 bits)
- Source MSB: GPR (most significant 32 bits)
- Dest: FPR

**Effect:** Combines two 32-bit integers into a 64-bit double bit pattern

---

### ff2i

**Syntax:** `ff2i <fpr>, <gpr>`

**Description:** Move float bit pattern to 32-bit integer register.

**Operands:**
- Source: FPR (float)
- Destination: GPR (32-bit int)

**Effect:** `dest_int32 = bitcast<int32>(src_float)`

---

### floord, floorf

**Syntax:** `floor{d|f} <src>, <dest>`

**Description:** Floor function (round toward -infinity).

**Variants:**
- `floord` - double-precision float
- `floorf` - single-precision float

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = floor(src)`

**ARM64 Translation:** `frintm` with appropriate size specifier

---

### fq2d

**Syntax:** `fq2d <gpr>, <fpr>`

**Description:** Move 64-bit integer bit pattern to double-precision float register.

**Operands:**
- Source: GPR (64-bit int)
- Destination: FPR (double)

**Effect:** `dest_double = bitcast<double>(src_int64)`

**ARM64 Translation:** `fmov d<dest>, x<src>`

---

### jmp

**Syntax:** `jmp <label|gpr>`

**Description:** Unconditional jump.

**Operands:**
- Target: Label or GPR containing address

**Effect:** Jump to target address

**Example Usage:**
```asm
jmp .done
jmp t0
```

**ARM64 Translation:** `b <label>` or `br <register>`

---

### leai

**Syntax:** `leai <address>, <gpr>`

**Description:** Load effective address (32-bit).

**Operands:**
- Source: Address or BaseIndex
- Destination: GPR

**Effect:** `dest = address_of(src)` (calculates address without loading)

**Example Usage:**
```asm
leai [t0, 16], t1    # t1 = t0 + 16
```

---

### leap

**Syntax:** `leap <address>, <gpr>`

**Description:** Load effective address (pointer-sized).

**Operands:**
- Source: Address or BaseIndex
- Destination: GPR

**Effect:** `dest = address_of(src)`

**Example Usage:**
```asm
leap [cfr, 8], t0    # t0 = cfr + 8
```

**ARM64 Translation:** `add x<dest>, x<base>, #<offset>`

---

### load2ia

**Syntax:** `load2ia <address>, <gpr>, <gpr>`

**Description:** Load two adjacent 32-bit integers.

**Operands:**
- Source: Address
- Dest1: GPR
- Dest2: GPR

**Effect:** Loads two consecutive 32-bit values

---

### loadb, loadbsi, loadbsq

**Syntax:** `loadb <address>, <gpr>` or `loadbs{i|q} <address>, <gpr>`

**Description:** Load 8-bit byte from memory.

**Variants:**
- `loadb` - unsigned byte, zero-extend
- `loadbsi` - signed byte, sign-extend to 32-bit
- `loadbsq` - signed byte, sign-extend to 64-bit

**Operands:**
- Source: Address
- Destination: GPR

**Effect:** `dest = load_byte(address)` with appropriate extension

**ARM64 Translation:** `ldrb/ldrsb` with appropriate size specifier

---

### loadd, loadf, loadi, loadis, loadp, loadq, loadv

**Syntax:** `load{d|f|i|is|p|q|v} <address>, <dest>`

**Description:** Load value from memory.

**Variants:**
- `loadd` - double-precision float (FPR destination)
- `loadf` - single-precision float (FPR destination)
- `loadi` - 32-bit integer (GPR destination)
- `loadis` - 32-bit integer, sign-extend to pointer/64-bit (GPR destination)
- `loadp` - pointer-sized value (GPR destination)
- `loadq` - 64-bit integer (GPR destination)
- `loadv` - 128-bit vector/SIMD (vector register destination)

**Operands:**
- Source: Address
- Destination: FPR, GPR, or VecReg

**Effect:** `dest = value[address]`

**Example Usage:**
```asm
loadi [t0], t1               # Load from [t0]
loadi [cfr, 16], t2          # Load from [cfr + 16]
loadi [t0, t1, 4], t2        # Load from [t0 + t1*16] (scale=4 means shift by 4)
```

**ARM64 Translation:** `ldr` with appropriate size specifier

---

### loadh, loadhsi, loadhsq

**Syntax:** `loadh <address>, <gpr>` or `loadhs{i|q} <address>, <gpr>`

**Description:** Load 16-bit half-word from memory.

**Variants:**
- `loadh` - unsigned half-word, zero-extend
- `loadhsi` - signed half-word, sign-extend to 32-bit
- `loadhsq` - signed half-word, sign-extend to 64-bit

**Operands:**
- Source: Address
- Destination: GPR

**Effect:** `dest = load_halfword(address)` with appropriate extension

**ARM64 Translation:** `ldrh/ldrsh` with appropriate size specifier

---

### lrotatei, lrotateq

**Syntax:** `lrotate{i|q} <count>, <dest>`

**Description:** Rotate integer left.

**Variants:**
- `lrotatei` - 32-bit integers
- `lrotateq` - 64-bit integers

**Operands:**
- Count: Immediate or GPR (rotation amount)
- Destination: GPR (value to rotate, in-place)

**Effect:** `dest = rotate_left(dest, count)`

---

### lshifti, lshiftp, lshiftq

**Syntax:** `lshift{i|p|q} <count>, <dest>` or `lshift{i|p|q} <count>, <src>, <dest>`

**Description:** Logical shift left integer.

**Variants:**
- `lshifti` - 32-bit integers
- `lshiftp` - pointer-sized integers
- `lshiftq` - 64-bit integers

**Operands:**
- 2 operands: Count (immediate or GPR), Destination GPR (in-place shift)
- 3 operands: Count (immediate or GPR), Source GPR, Destination GPR

**Effect:** `dest = src << count`

**Example Usage:**
```asm
lshifti 2, t0        # t0 = t0 << 2
lshifti t1, t0, t2   # t2 = t0 << t1
```

**ARM64 Translation:** `lsl` with appropriate size specifier

---

### lzcnti, lzcntq

**Syntax:** `lzcnt{i|q} <src>, <dest>`

**Description:** Count leading zeros in integer.

**Variants:**
- `lzcnti` - 32-bit integers
- `lzcntq` - 64-bit integers

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = count_leading_zeros(src)`

**ARM64 Translation:** `clz` with appropriate size specifier

---

### memfence

**Syntax:** `memfence`

**Description:** Memory fence/barrier.

**Effect:** Ensures memory operations before the fence complete before operations after

**ARM64 Translation:** `dmb ish` or similar

---

### move

**Syntax:** `move <src>, <dest>`

**Description:** Move value between registers or load immediate.

**Operands:**
- Source: GPR, FPR, or Immediate
- Destination: GPR or FPR

**Effect:** `dest = src`

**Example Usage:**
```asm
move 42, t0          # t0 = 42
move t1, t0          # t0 = t1
```

**ARM64 Translation:** `mov x<dest>, x<src>` or `mov x<dest>, #<imm>`

---

### moved

**Syntax:** `moved <fpr>, <fpr>`

**Description:** Move double-precision float between FP registers.

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = src`

**ARM64 Translation:** `fmov d<dest>, d<src>`

---

### movdz

**Syntax:** `movdz <fpr>, <fpr>`

**Description:** Move double or set to zero if source is -0.0.

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = (src == -0.0) ? 0.0 : src`

---

### muld, mulf, muli, mulp, mulq

**Syntax:** `mul{d|f|i|p|q} <src>, <dest>` or `mul{d|f|i|p|q} <src1>, <src2>, <dest>`

**Description:** Multiply two values.

**Variants:**
- `muld` - double-precision floats (FPR operands)
- `mulf` - single-precision floats (FPR operands)
- `muli` - 32-bit integers (GPR operands, immediate or GPR source)
- `mulp` - pointer-sized integers (GPR operands, immediate or GPR source)
- `mulq` - 64-bit integers (GPR operands, immediate or GPR source)

**Operands:**
- 2 operands: Source, Destination (in-place multiply)
- 3 operands: Source1, Source2, Destination

**Effect:** `dest = src1 * src2`

**ARM64 Translation:** `mul/fmul` with appropriate size specifier

---

### negd, negf, negi, negp, negq

**Syntax:** `neg{d|f} <src>, <dest>` or `neg{i|p|q} <dest>`

**Description:** Negate a value.

**Variants:**
- `negd` - double-precision floats (2 operands: source FPR, dest FPR)
- `negf` - single-precision floats (2 operands: source FPR, dest FPR)
- `negi` - 32-bit integers (1 operand: dest GPR, in-place)
- `negp` - pointer-sized integers (1 operand: dest GPR, in-place)
- `negq` - 64-bit integers (1 operand: dest GPR, in-place)

**Effect:** `dest = -src` (floats) or `dest = -dest` (integers)

**ARM64 Translation:** `neg/fneg` with appropriate size specifier

---

### nop

**Syntax:** `nop`

**Description:** No operation.

**Effect:** Does nothing, used for padding or alignment

**ARM64 Translation:** `nop`

---

### noti

**Syntax:** `noti <gpr>`

**Description:** Bitwise NOT of 32-bit integer.

**Operands:**
- Destination: GPR (in-place NOT)

**Effect:** `dest = ~dest`

**Note:** See also `notq` in architecture-specific instructions for 64-bit variant.

**ARM64 Translation:** `mvn w<dest>, w<src>`

---

### ord, orf, orh, ori, orp, orq

**Syntax:** `or{d|f|h|i|p|q} <src>, <dest>` or `or{i|p|q} <src1>, <src2>, <dest>`

**Description:** Bitwise OR of values.

**Variants:**
- `ord` - double-precision float bit patterns (FPR operands)
- `orf` - single-precision float bit patterns (FPR operands)
- `orh` - 16-bit half-words (GPR operands, immediate or GPR source)
- `ori` - 32-bit integers (GPR operands, immediate or GPR source)
- `orp` - pointer-sized integers (GPR operands, immediate or GPR source)
- `orq` - 64-bit integers (GPR operands, immediate or GPR source)

**Operands:**
- 2 operands: Source, Destination (in-place OR)
- 3 operands: Source1, Source2, Destination (integer variants only)

**Effect:** `dest = src1 | src2`

**ARM64 Translation:** `orr` with appropriate size specifier

---

### peek

**Syntax:** `peek <imm>, <gpr|fpr>`

**Description:** Read value from stack at offset.

**Operands:**
- Offset: Immediate (in pointer-sized units from SP)
- Destination: GPR or FPR

**Effect:** `dest = stack[sp + offset * sizeof(ptr)]`

**Example Usage:**
```asm
peek 2, t0           # Load from [sp + 16] (on 64-bit)
```

---

### poke

**Syntax:** `poke <gpr|fpr|imm>, <imm>`

**Description:** Write value to stack at offset.

**Operands:**
- Source: GPR, FPR, or Immediate
- Offset: Immediate (in pointer-sized units from SP)

**Effect:** `stack[sp + offset * sizeof(ptr)] = src`

**Example Usage:**
```asm
poke t0, 2           # Store to [sp + 16]
poke 0, 1            # Store 0 to [sp + 8]
```

---

### pop

**Syntax:** `pop <gpr>`

**Description:** Pop value from stack.

**Operands:**
- Destination: GPR

**Effect:** `dest = [sp]; sp = sp + sizeof(ptr)`

**ARM64 Translation:** `ldr x<dest>, [sp], #8`

---

### popv

**Syntax:** `popv <vecreg>`

**Description:** Pop 128-bit vector from stack.

**Operands:**
- Destination: Vector register

**Effect:** `dest = [sp]; sp = sp + 16`

---

### push

**Syntax:** `push <gpr>`

**Description:** Push value onto stack.

**Operands:**
- Source: GPR

**Effect:** `sp = sp - sizeof(ptr); [sp] = src`

**ARM64 Translation:** `str x<src>, [sp, #-8]!`

---

### pushv

**Syntax:** `pushv <vecreg>`

**Description:** Push 128-bit vector onto stack.

**Operands:**
- Source: Vector register

**Effect:** `sp = sp - 16; [sp] = src`

---

### removeArrayPtrTag

**Syntax:** `removeArrayPtrTag <gpr>`

**Description:** Remove pointer authentication code from array pointer.

**Operands:**
- Pointer: GPR (in-place removal)

**Effect:** Strips PAC from pointer (ARM64e specific)

---

### removeCodePtrTag

**Syntax:** `removeCodePtrTag <gpr>`

**Description:** Strip pointer authentication information from code pointer without authenticating its value.
(ARM64e specific, no effect on other platforms).

**Operands:**
- Pointer: GPR (in-place removal)

**ARM64E Translation:** `xpaci <gpr>`

---

### ret

**Syntax:** `ret`

**Description:** Return from function.

**Effect:** ARM64E only: authenticate the return address in the `lr` register using the IB key and `sp` as the discriminator, then jump to it if authentication succeeds. Other architectures: pop the return address and jump to it.

**ARM64E Translation:** `retab`

---

### roundd, roundf

**Syntax:** `round{d|f} <src>, <dest>`

**Description:** Round to nearest integer (ties to even).

**Variants:**
- `roundd` - double-precision float
- `roundf` - single-precision float

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = round(src)`

**ARM64 Translation:** `frintn` with appropriate size specifier

---

### rrotatei, rrotateq

**Syntax:** `rrotate{i|q} <count>, <dest>`

**Description:** Rotate integer right.

**Variants:**
- `rrotatei` - 32-bit integers
- `rrotateq` - 64-bit integers

**Operands:**
- Count: Immediate or GPR (rotation amount)
- Destination: GPR (value to rotate, in-place)

**Effect:** `dest = rotate_right(dest, count)`

---

### rshifti, rshiftp, rshiftq

**Syntax:** `rshift{i|p|q} <count>, <dest>` or `rshift{i|p|q} <count>, <src>, <dest>`

**Description:** Arithmetic (signed) shift right integer.

**Variants:**
- `rshifti` - 32-bit integers
- `rshiftp` - pointer-sized integers
- `rshiftq` - 64-bit integers

**Operands:**
- 2 operands: Count (immediate or GPR), Destination GPR (in-place shift)
- 3 operands: Count (immediate or GPR), Source GPR, Destination GPR

**Effect:** `dest = src >> count` (sign-extending)

**ARM64 Translation:** `asr` with appropriate size specifier

---

### sqrtd, sqrtf

**Syntax:** `sqrt{d|f} <src>, <dest>`

**Description:** Square root of floating-point number.

**Variants:**
- `sqrtd` - double-precision float
- `sqrtf` - single-precision float

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = sqrt(src)`

**ARM64 Translation:** `fsqrt` with appropriate size specifier

---

### store2ia

**Syntax:** `store2ia <gpr>, <gpr>, <address>`

**Description:** Store two adjacent 32-bit integers.

**Operands:**
- Source1: GPR
- Source2: GPR
- Destination: Address

**Effect:** Stores two consecutive 32-bit values

---

### storeb, stored, storef, storeh, storei, storep, storeq, storev

**Syntax:** `store{b|d|f|h|i|p|q|v} <src>, <address>`

**Description:** Store value to memory.

**Variants:**
- `storeb` - 8-bit byte (GPR or immediate source)
- `stored` - double-precision float (FPR source)
- `storef` - single-precision float (FPR source)
- `storeh` - 16-bit half-word (GPR or immediate source)
- `storei` - 32-bit integer (GPR or immediate source)
- `storep` - pointer-sized value (GPR or immediate source)
- `storeq` - 64-bit integer (GPR or immediate source)
- `storev` - 128-bit vector/SIMD (vector register source)

**Operands:**
- Source: GPR, FPR, VecReg, or Immediate
- Address: Memory address

**Effect:** `memory[address] = src`

**Example Usage:**
```asm
storei t0, [t1]              # Store t0 to [t1]
storei 42, [cfr, 8]          # Store constant to [cfr + 8]
```

**ARM64 Translation:** `str` with appropriate size specifier

---

### subd, subf, subi, subp, subq

**Syntax:** `sub{d|f|i|p|q} <src>, <dest>` or `sub{d|f|i|p|q} <src1>, <src2>, <dest>`

**Description:** Subtract two values.

**Variants:**
- `subd` - double-precision floats (FPR operands)
- `subf` - single-precision floats (FPR operands)
- `subi` - 32-bit integers (GPR operands, immediate or GPR source)
- `subp` - pointer-sized integers (GPR operands, immediate or GPR source)
- `subq` - 64-bit integers (GPR operands, immediate or GPR source)

**Operands:**
- 2 operands: Source, Destination (in-place subtract)
- 3 operands: Source1, Source2, Destination

**Effect:** `dest = src1 - src2` (3-operand) or `dest = dest - src` (2-operand)

**ARM64 Translation:** `sub/fsub` with appropriate size specifier

---

### sxb2i, sxb2p, sxb2q

**Syntax:** `sxb2{i|p|q} <src>, <dest>`

**Description:** Sign-extend byte to larger integer size.

**Variants:**
- `sxb2i` - byte to 32-bit integer
- `sxb2p` - byte to pointer size
- `sxb2q` - byte to 64-bit integer

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = sign_extend(src & 0xff)`

**ARM64 Translation:** `sxtb` with appropriate size specifier

---

### sxh2i, sxh2q

**Syntax:** `sxh2{i|q} <src>, <dest>`

**Description:** Sign-extend half-word to larger integer size.

**Variants:**
- `sxh2i` - half-word to 32-bit integer
- `sxh2q` - half-word to 64-bit integer

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = sign_extend(src & 0xffff)`

**ARM64 Translation:** `sxth` with appropriate size specifier

---

### sxi2q

**Syntax:** `sxi2q <gpr>, <gpr>`

**Description:** Sign-extend 32-bit integer to 64-bit.

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = sign_extend_64(src)`

**ARM64 Translation:** `sxtw x<dest>, w<src>`

---

### tagCodePtr

**Base Syntax:** `tagCodePtr <gpr1>, <gpr2>`

**Description:** Sign a code pointer in a register using the IB key and a discriminator in another register.
(ARM64e specific).

**Operands:**
- gpr1: A register containing the pointer to sign.
- gpr2: Discriminator.

**ARM64E translation:** `pacib <gpr1> <gpr2>`

**Expanded Syntax:** `tagCodePtr <gpr1>, <imm1>, <imm2>, <gpr2>`

**Description:** Sign a code pointer in a register using the IB key and a discriminator created by combining
a constant tag and the value of another register.

**Operands:**
- gpr1: A register containing the pointer to sign.
- imm1: A constant tag. Must be less than or equal to 0xFFFF.
- imm2: Must be the constant `AddressDiversified` (numeric value 1).
- gpr2: Discriminator.

**Effect:** Expands into the following sequence of offlineasm instructions:
```
move (imm1 << 48), tempGPR
xorp gpr2, tempGPR
tagCodePtr gpr1, tempGPR
```

---

### tagReturnAddress

**Syntax:** `tagReturnAddress <reg>`

**Description:** Sign the address in the `lr` register using the IB key and another register value as a discriminator.
(ARM64e specific).

**Operands:**
- Discriminator register: GPR or `sp`

**ARM64E Translation:** `pacibsp` or `pacib lr, GPR`

---

### tbs

**Syntax:** `tbs <gpr>, <imm>, <gpr>`

**Description:** Test byte and set destination to 1 if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) < 0) ? 1 : 0`

---

### tbz

**Syntax:** `tbz <gpr>, <imm>, <gpr>`

**Description:** Test byte and set destination to 1 if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) == 0) ? 1 : 0`

---

### tbnz

**Syntax:** `tbnz <gpr>, <imm>, <gpr>`

**Description:** Test byte and set destination to 1 if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) != 0) ? 1 : 0`

---

### td2i

**Syntax:** `td2i <fpr>, <gpr>`

**Description:** Truncate double to signed 32-bit integer.

**Operands:**
- Source: FPR (double)
- Destination: GPR (32-bit int)

**Effect:** `dest = (int32)src` (truncates toward zero)

**ARM64 Translation:** `fcvtzs w<dest>, d<src>`

---

### tis

**Syntax:** `tis <gpr>, <imm>, <gpr>`

**Description:** Test 32-bit integer and set destination to 1 if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) < 0) ? 1 : 0`

---

### tiz

**Syntax:** `tiz <gpr>, <imm>, <gpr>`

**Description:** Test 32-bit integer and set destination to 1 if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) == 0) ? 1 : 0`

---

### tinz

**Syntax:** `tinz <gpr>, <imm>, <gpr>`

**Description:** Test 32-bit integer and set destination to 1 if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) != 0) ? 1 : 0`

---

### tps

**Syntax:** `tps <gpr>, <imm>, <gpr>`

**Description:** Test pointer and set destination to 1 if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) < 0) ? 1 : 0`

---

### tpz

**Syntax:** `tpz <gpr>, <imm>, <gpr>`

**Description:** Test pointer and set destination to 1 if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) == 0) ? 1 : 0`

---

### tpnz

**Syntax:** `tpnz <gpr>, <imm>, <gpr>`

**Description:** Test pointer and set destination to 1 if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) != 0) ? 1 : 0`

---

### tqs

**Syntax:** `tqs <gpr>, <imm>, <gpr>`

**Description:** Test 64-bit integer and set destination to 1 if sign bit is set.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) < 0) ? 1 : 0`

---

### tqz

**Syntax:** `tqz <gpr>, <imm>, <gpr>`

**Description:** Test 64-bit integer and set destination to 1 if zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) == 0) ? 1 : 0`

---

### tqnz

**Syntax:** `tqnz <gpr>, <imm>, <gpr>`

**Description:** Test 64-bit integer and set destination to 1 if not zero.

**Operands:**
- Test value: GPR
- Bit mask: Immediate
- Destination: GPR

**Effect:** `dest = ((src & mask) != 0) ? 1 : 0`

---

### transferi, transferp, transferq

**Syntax:** `transfer{i|p|q} <src>, <dest>`

**Description:** Transfer/move value between GPRs (alias for move).

**Variants:**
- `transferi` - 32-bit values
- `transferp` - pointer-sized values
- `transferq` - 64-bit values

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = src`

---

### truncated, truncatef

**Syntax:** `truncate{d|f} <src>, <dest>`

**Description:** Truncate float to integer value (result stays as float type).

**Variants:**
- `truncated` - double-precision
- `truncatef` - single-precision

**Operands:**
- Source: FPR
- Destination: FPR

**Effect:** `dest = trunc(src)` (removes fractional part)

**ARM64 Translation:** `frintz` with appropriate size specifier

---

### truncated2i, truncated2is, truncated2q, truncated2qs

**Syntax:** `truncated2{i|is|q|qs} <src>, <dest>`

**Description:** Truncate double to signed integer.

**Variants:**
- `truncated2i` - to 32-bit integer
- `truncated2is` - to 32-bit integer with saturation
- `truncated2q` - to 64-bit integer
- `truncated2qs` - to 64-bit integer with saturation

**Operands:**
- Source: FPR (double)
- Destination: GPR

**Effect:** `dest = (int)src` (truncates toward zero)

**ARM64 Translation:** `fcvtzs` with appropriate size specifier

---

### truncatef2i, truncatef2is, truncatef2q, truncatef2qs

**Syntax:** `truncatef2{i|is|q|qs} <src>, <dest>`

**Description:** Truncate float to signed integer.

**Variants:**
- `truncatef2i` - to 32-bit integer
- `truncatef2is` - to 32-bit integer with saturation
- `truncatef2q` - to 64-bit integer
- `truncatef2qs` - to 64-bit integer with saturation

**Operands:**
- Source: FPR (float)
- Destination: GPR

**Effect:** `dest = (int)src` (truncates toward zero)

---

### tzcnti, tzcntq

**Syntax:** `tzcnt{i|q} <src>, <dest>`

**Description:** Count trailing zeros in integer.

**Variants:**
- `tzcnti` - 32-bit integers
- `tzcntq` - 64-bit integers

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = count_trailing_zeros(src)`

**ARM64 Translation:** `rbit + clz` (reverse bits then count leading zeros)

---

### untagArrayPtr

**Syntax:** `untagArrayPtr <gpr>`

**Description:** Remove pointer authentication and untag array pointer (ARM64e).

---

### untagReturnAddress

**Syntax:** `untagReturnAddress <reg>`

**Description:** Remove the PAC signature from the value in the  `lr` register if it successfully authenticates
using the IB key and the value of the specified GPR or `sp` as the discriminator.
(ARM64e specific).

**Operands:**
- reg: GPR or `sp` containing the discriminator value

**ARM64E Translation:** `autibsp` or `autib lr, GPR`

---

### urshifti, urshiftp, urshiftq

**Syntax:** `urshift{i|p|q} <count>, <dest>` or `urshift{i|p|q} <count>, <src>, <dest>`

**Description:** Logical (unsigned) shift right integer.

**Variants:**
- `urshifti` - 32-bit integers
- `urshiftp` - pointer-sized integers
- `urshiftq` - 64-bit integers

**Operands:**
- 2 operands: Count (immediate or GPR), Destination GPR (in-place shift)
- 3 operands: Count (immediate or GPR), Source GPR, Destination GPR

**Effect:** `dest = src >> count` (zero-extending)

**ARM64 Translation:** `lsr` with appropriate size specifier

---

### xori, xorp, xorq

**Syntax:** `xor{i|p|q} <src>, <dest>` or `xor{i|p|q} <src1>, <src2>, <dest>`

**Description:** Bitwise XOR of integers.

**Variants:**
- `xori` - 32-bit integers (immediate or GPR source)
- `xorp` - pointer-sized integers (immediate or GPR source)
- `xorq` - 64-bit integers (immediate or GPR source)

**Operands:**
- 2 operands: Source, Destination (in-place XOR)
- 3 operands: Source1, Source2, Destination

**Effect:** `dest = src1 ^ src2`

**ARM64 Translation:** `eor` with appropriate size specifier

---

### zxi2q

**Syntax:** `zxi2q <gpr>, <gpr>`

**Description:** Zero-extend 32-bit integer to 64-bit.

**Operands:**
- Source: GPR
- Destination: GPR

**Effect:** `dest = zero_extend_64(src)`

**ARM64 Translation:** `mov w<dest>, w<src>` (implicit zero-extension on ARM64)

---

## Architecture-Specific Instructions

### ARM64-Specific Instructions

#### bfiq
**Syntax:** `bfiq <gpr>, <imm>, <imm>, <gpr>`

**Description:** Bit field insert.

**Operands:**
- Source: GPR
- Last bit: Immediate
- Width: Immediate
- Destination: GPR

**ARM64 Translation:** `bfi` or `bfxil`

#### fence
**Syntax:** `fence`

**Description:** Full memory fence.

**ARM64 Translation:** `dmb sy`

#### globaladdr
**Syntax:** `globaladdr <label>, <gpr>`

**Description:** Load global address into register.

#### loadlinkacqb/h/i/q
**Syntax:** `loadlinkacq{b|h|i|q} <address>, <gpr>`

**Description:** Load-link with acquire semantics (for atomic operations).

**ARM64 Translation:** `ldaxrb/ldaxrh/ldaxr/ldaxr`

#### notq
**Syntax:** `notq <gpr>`

**Description:** Bitwise NOT of 64-bit integer.

**ARM64 Translation:** `mvn x<dest>, x<src>`

#### storecondrel{b|h|i|q}
**Syntax:** `storecondrel{b|h|i|q} <gpr>, <address>, <gpr>`

**Description:** Store-conditional with release semantics.

**ARM64 Translation:** `stlxrb/stlxrh/stlxr/stlxr`

---

## Register Naming Conventions

### General Purpose Registers
- `t0`-`t12` - Temporary registers
- `cfr` - C frame register (x29/fp)
- `csr0`-`csr9` - Callee-saved registers
- `sp` - Stack pointer
- `lr` - Link register (return address)

### Floating Point Registers
- `ft0`-`ft7` - Temporary FP registers
- `csfr0`-`csfr7` - Callee-saved FP registers

### Vector Registers
- `v0`-`v7` - Vector registers
- `v0_b`, `v0_h`, `v0_i`, `v0_q` - Vector with element size interpretation

---

## Notes

1. **Operand Order**: Generally follows AT&T syntax for multi-operand instructions: `operation source, destination` or `operation src1, src2, dest` for 3-operand forms.

2. **Address Modes**:
   - `[base]` - Register indirect
   - `[base, offset]` - Register + immediate offset
   - `[base, index, scale]` - Register + scaled index (BaseIndex)

3. **Immediate Values**: Prefixed with `#` in actual assembly output, but written without prefix in offlineasm source.

4. **Conditional Suffixes**:
   - `eq` - Equal
   - `neq` - Not equal
   - `a` - Above (unsigned >)
   - `aeq` - Above or equal (unsigned >=)
   - `b` - Below (unsigned <)
   - `beq` - Below or equal (unsigned <=)
   - `gt` - Greater than (signed >)
   - `gteq` - Greater or equal (signed >=)
   - `lt` - Less than (signed <)
   - `lteq` - Less or equal (signed <=)
   - `z` - Zero
   - `nz` - Not zero
   - `s` - Sign (negative)
   - `o` - Overflow

5. **Floating-Point Comparisons**:
   - Ordered comparisons (`bdeq`, `bdlt`, etc.) - Branch if comparison is true AND neither operand is NaN
   - Unordered comparisons (`bdequn`, `bdltun`, etc.) - Branch if comparison is true OR either operand is NaN

---

This reference covers the main MACRO_INSTRUCTIONS used across all platforms. Architecture-specific instructions (X86_INSTRUCTIONS, ARM_INSTRUCTIONS, etc.) are used for platform-specific optimizations and are translated directly to native instructions.

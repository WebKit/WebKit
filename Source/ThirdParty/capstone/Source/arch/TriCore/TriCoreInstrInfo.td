//===-- TriCoreInstrInfo.td - Target Description for TriCore ---*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the TriCore instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass.
//===----------------------------------------------------------------------===//

include "TriCoreInstrFormats.td"

//===----------------------------------------------------------------------===//
// TriCore specific DAG Nodes.
//

// Call
def SDT_TriCoreCmp      : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                           SDTCisSameAs<1, 2>,
                           SDTCisVT<3, i32>]>;
def SDT_TriCoreImask    : SDTypeProfile<1, 3, [SDTCisVT<0, i64>,
                           SDTCisVT<1, i32>,
                           SDTCisVT<2, i32>,
                           SDTCisVT<3, i32>]>;

def SDT_TriCoreExtract  : SDTypeProfile<1, 3, [SDTCisVT<0, i32>,
                           SDTCisVT<1, i32>,
                           SDTCisVT<2, i32>,
                           SDTCisVT<3, i32>]>;

def SDT_TriCoreLCmp     : SDTypeProfile<1, 4, [SDTCisSameAs<0, 1>,
                           SDTCisSameAs<1, 2>,
                           SDTCisSameAs<2, 3>,
                           SDTCisVT<4, i32>]>;
def SDT_TriCoreBrCC     : SDTypeProfile<0, 3, [SDTCisVT<0, OtherVT>,
                           SDTCisVT<1, i32>,
                           SDTCisVT<2, i32>]>;
def SDT_TriCoreCall     : SDTypeProfile<0, -1, [SDTCisPtrTy<0>]>;
def SDT_TriCoreSelectCC : SDTypeProfile<1, 4, [SDTCisSameAs<0, 1>,
                           SDTCisSameAs<1, 2>,
                           SDTCisVT<3, i32>,
                           SDTCisVT<4, i32>]>;
def SDT_TriCoreWrapper  : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                           SDTCisPtrTy<0>]>;

def SDT_TriCoreShift    : SDTypeProfile<1, 2, [SDTCisVT<0, i32>,
                           SDTCisVT<1, i32>,
                           SDTCisVT<2, i32>]>;

def SDT_TriCoreMovei32  : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                           SDTCisVT<0, i32>]>;

def SDT_TriCoreMovei64  : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                           SDTCisVT<0, i64>]>;

def TriCoreAbs     : SDNode<"TriCoreISD::ABS", SDTIntUnaryOp>;
def TriCoreAbsDif  : SDNode<"TriCoreISD::ABSDIF", SDTIntBinOp>;
def TriCoreBrCC    : SDNode<"TriCoreISD::BR_CC",
                      SDT_TriCoreBrCC, [SDNPHasChain, SDNPInGlue]>;
def TriCoreCall    : SDNode<"TriCoreISD::CALL", SDT_TriCoreCall,
                      [ SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic ]>;
def TriCoreCmp     : SDNode<"TriCoreISD::CMP",
                      SDT_TriCoreCmp, [SDNPOutGlue]>;
def TriCoreLogicCmp: SDNode<"TriCoreISD::LOGICCMP",
                      SDT_TriCoreLCmp, [SDNPInGlue, SDNPOutGlue]>;
def TriCoreWrapper : SDNode<"TriCoreISD::Wrapper", SDT_TriCoreWrapper>;
def TriCoreImask   : SDNode<"TriCoreISD::IMASK", SDT_TriCoreImask>;
def TriCoreSh      : SDNode<"TriCoreISD::SH",  SDT_TriCoreShift>;
def TriCoreSha     : SDNode<"TriCoreISD::SHA", SDT_TriCoreShift>;
def TriCoreExtr    : SDNode<"TriCoreISD::EXTR", SDT_TriCoreExtract>;
def TriCoreSelectCC: SDNode<"TriCoreISD::SELECT_CC", SDT_TriCoreSelectCC, []>;

def load_sym : SDNode<"TriCoreISD::LOAD_SYM", SDTIntUnaryOp>;

def movei32 : SDNode<"TriCoreISD::MOVEi32", SDT_TriCoreMovei32>;


def jmptarget : Operand<OtherVT> {
  let PrintMethod = "printPCRelImmOperand";
}

// Operand for printing out a condition code.
def cc : Operand<i32> {
  let PrintMethod = "printCCOperand";
}

def isPointer : Predicate<"isPointer() == true">;
def isnotPointer : Predicate<"isPointer() == false">;

// TriCore Condition Codes
def TriCore_COND_EQ : PatLeaf<(i32 0)>;
def TriCore_COND_NE : PatLeaf<(i32 1)>;
def TriCore_COND_GE : PatLeaf<(i32 2)>;
def TriCore_COND_LT : PatLeaf<(i32 3)>;
// TriCore Logic Codes
def TriCore_LOGIC_AND_EQ : PatLeaf<(i32 0)>;
def TriCore_LOGIC_AND_NE : PatLeaf<(i32 1)>;
def TriCore_LOGIC_AND_GE : PatLeaf<(i32 2)>;
def TriCore_LOGIC_AND_LT : PatLeaf<(i32 3)>;
def TriCore_LOGIC_OR_EQ  : PatLeaf<(i32 0)>;
def TriCore_LOGIC_OR_NE  : PatLeaf<(i32 1)>;
def TriCore_LOGIC_OR_GE  : PatLeaf<(i32 12)>;
def TriCore_LOGIC_OR_LT  : PatLeaf<(i32 13)>;

// These are target-independent nodes, but have target-specific formats.
def SDT_TriCoreCallSeqStart : SDCallSeqStart<[ SDTCisVT<0, i32> ]>;
def SDT_TriCoreCallSeqEnd   : SDCallSeqEnd<[ SDTCisVT<0, i32>,
                                        SDTCisVT<1, i32> ]>;

def TriCoreRetFlag    : SDNode<"TriCoreISD::RET_FLAG", SDTNone,
                           [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_TriCoreCallSeqStart,
                           [SDNPHasChain, SDNPOutGlue, SDNPSideEffect]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END",   SDT_TriCoreCallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                           SDNPSideEffect]>;

//===----------------------------------------------------------------------===//
// Instruction Pattern Stuff
//===----------------------------------------------------------------------===//

// Lower 32 bits of a 64-bit word
def LO32 : SDNodeXForm<imm, [{
 return CurDAG->getTargetConstant((uint32_t) N->getZExtValue(), SDLoc(N),
   MVT::i32);
}]>;

// Higher 32 bits of a 64-bit word
def HI32 : SDNodeXForm<imm, [{
 return CurDAG->getTargetConstant((uint32_t) (N->getZExtValue()>>32), SDLoc(N),
   MVT::i32);
}]>;

def INVERT_VAL : SDNodeXForm<imm, [{
 outs() << "vall: "<< N->getZExtValue() <<"\n";
 return CurDAG->getTargetConstant(-N->getZExtValue(), SDLoc(N), MVT::i32);
}]>;

def SHIFTAMT : SDNodeXForm<imm, [{
 outs() << "vall: "<< N->getZExtValue() <<"\n";
 return CurDAG->getTargetConstant(N->getZExtValue() - 32, SDLoc(N), MVT::i32);
}]>;

def SHIFTAMT_POS : SDNodeXForm<imm, [{
 outs() << "vall: "<< N->getZExtValue() <<"\n";
 return CurDAG->getTargetConstant((32 - N->getZExtValue()), SDLoc(N), MVT::i32);
}]>;

def SHIFTAMT_NEG : SDNodeXForm<imm, [{
 return CurDAG->getTargetConstant(-(N->getZExtValue() - 32), SDLoc(N),
   MVT::i32);
}]>;

def imm32_64 : PatLeaf<(imm),
[{
  uint64_t val = N->getZExtValue();
  return val >= 32 && val < 64;
}]>;

def imm0_31 : PatLeaf<(imm),
[{
  uint64_t val = N->getZExtValue();
 outs() <<"imm0_31: " << val << "\n";
  return val > 0 && val < 32;
}]>;

//Operands
def s4imm      : Operand<i32> { let PrintMethod = "printSExtImm<4>";  }
def s6imm      : Operand<i32> { let PrintMethod = "printSExtImm<6>";  }
def s8imm      : Operand<i32> { let PrintMethod = "printSExtImm<8>";  }
def s9imm      : Operand<i32> { let PrintMethod = "printSExtImm<9>";  }
def s10imm     : Operand<i32> { let PrintMethod = "printSExtImm<10>";  }
def s16imm     : Operand<i32> { let PrintMethod = "printSExtImm<16>"; }
def s24imm     : Operand<i32> { let PrintMethod = "printSExtImm<24>"; }
def u8imm      : Operand<i32> { let PrintMethod = "printZExtImm<8>";  }
def u4imm      : Operand<i32> { let PrintMethod = "printZExtImm<4>";  }
def u2imm      : Operand<i32> { let PrintMethod = "printZExtImm<2>";  }
def u9imm      : Operand<i32> { let PrintMethod = "printZExtImm<9>";  }
def u16imm     : Operand<i32> { let PrintMethod = "printZExtImm<16>";  }

def oext4imm: Operand<i32> { let PrintMethod = "printOExtImm<4>";  }

def off18imm   : Operand<i32> { let PrintMethod = "printOff18Imm";  }

def disp24imm  : Operand<i32> { let PrintMethod = "printDisp24Imm";  }
def disp15imm   : Operand<i32> { let PrintMethod = "printDisp15Imm";  }
def disp8imm   : Operand<i32> { let PrintMethod = "printDisp8Imm";  }
def disp4imm   : Operand<i32> { let PrintMethod = "printDisp4Imm";  }

def PairAddrRegsOp : RegisterOperand<PairAddrRegs, "printPairAddrRegsOperand">;

//Nodes
def immSExt4  : PatLeaf<(imm), [{ return isInt<4>(N->getSExtValue()); }]>;
def immSExt6  : PatLeaf<(imm), [{ return isInt<6>(N->getSExtValue()); }]>;
def immSExt9  : PatLeaf<(imm), [{ return isInt<9>(N->getSExtValue()); }]>;
def immSExt10  : PatLeaf<(imm), [{ return isInt<10>(N->getSExtValue()); }]>;
def immSExt16  : PatLeaf<(imm), [{ return isInt<16>(N->getSExtValue()); }]>;
def immSExt24  : PatLeaf<(imm), [{ return isInt<24>(N->getSExtValue()); }]>;

def immZExt2 : ImmLeaf<i32, [{return Imm == (Imm & 0x3);}]>;
def immZExt4 : ImmLeaf<i32, [{return Imm == (Imm & 0xf);}]>;
def immZExt8 : ImmLeaf<i32, [{return Imm == (Imm & 0xff);}]>;
def immZExt9 : ImmLeaf<i32, [{return Imm == (Imm & 0x1ff);}]>;
def immZExt16 : ImmLeaf<i32, [{return Imm == (Imm & 0xffff);}]>;

/// 16-Bit Opcode Formats

class ISC_D15C<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " d15, $const8", []>;

class ISC_A10C<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " sp, $const8", []>;

class ISC_A15A10C<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " a15, [sp]$const8", []>;

class ISC_D15A10C<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " d15, [sp]$const8", []>;

class ISC_A10CA15<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " [sp]$const8, a15", []>;

class ISC_A10CD15<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " [sp]$const8, d15", []>;

class ISC_C<bits<8> op1, string asmstr, Operand TypeC=u8imm> 
    : SC<op1, (outs), (ins TypeC:$const8),
      asmstr # " $const8", []>;

class ISRC_dC<bits<8> op1, string asmstr, RegisterClass RCd=RD, Operand TypeC=s4imm> 
    : SRC<op1, (outs RCd:$d), (ins TypeC:$const4),
          asmstr # " $d, $const4", []>;

class ISRC_dD15C<bits<8> op1, string asmstr, RegisterClass RCd=RD, Operand TypeC=s4imm> 
    : SRC<op1, (outs RCd:$d), (ins TypeC:$const4),
      asmstr # " $d, d15, $const4", []>;

class ISRC_D15dC<bits<8> op1, string asmstr, RegisterClass RCd=RD, Operand TypeC=s4imm> 
    : SRC<op1, (outs RCd:$d), (ins TypeC:$const4),
      asmstr # " d15, $d, $const4", []>;

multiclass mISRR_SRC<bits<8> op_srr, bits<8> op_src, string asmstr,
                     RegisterClass RCd=RD, RegisterClass RC2=RD, Operand Oc=u4imm, string posfix="">{
  def _srr#posfix: SRR<op_srr, (outs RCd:$d), (ins RC2:$s2),
                asmstr # " d15, $d, $s2", []>;
  def _src#posfix: SRC<op_src, (outs RCd:$d), (ins Oc:$const4),
                asmstr # " d15, $d, $const4", []>;
}

multiclass mISRC_a15a<bits<8> op1, bits<8> op2, bits<8> op3,
                        string asmstr> {
  def _src : ISRC_dC<op1, asmstr>;
  def _src_a15 : ISRC_dD15C<op2, asmstr>, Requires<[HasV120_UP]>;
  def _src_15a : ISRC_D15dC<op3, asmstr>;
}

/// 32-Bit Opcode Formats

/// RC

class IRC_C<bits<8> op1, bits<7> op2, string asmstr>
    : RC<op1, op2, (outs), (ins s9imm:$const9),
      asmstr # " $const9", []>;

class IRC<bits<8> op1, bits<7> op2, string asmstr, RegisterClass RCd=RD, RegisterClass RC1=RD, Operand TypeC=s9imm>
    : RC<op1, op2, (outs RCd:$d), (ins RC1:$s1, TypeC:$const9),
         asmstr # " $d, $s1, $const9", []>;

/// RR

class IRR_0<bits<8> op1, bits<8> op2, string asmstr>: RR<op1, op2, (outs), (ins), asmstr, []>;

class IRR_R1<bits<8> op1, bits<8> op2, string asmstr, RegisterClass RC=RD>
    : RR<op1, op2, (outs), (ins RC:$s1), asmstr # " $s1", []>;
class IRR_R2<bits<8> op1, bits<8> op2, string asmstr, RegisterClass RC=RD>
    : RR<op1, op2, (outs), (ins RC:$s2), asmstr # " $s2", []>;

/// op R[c], R[a]
class IRR_a<bits<8> op1, bits<8> op2, string asmstr, RegisterClass cd=RD, RegisterClass c1=RD>
    : RR<op1, op2, (outs cd:$d), (ins c1:$s1),
      asmstr # " $d, $s1", []>;

/// op R[c], R[b]
class IRR_b<bits<8> op1, bits<8> op2, string asmstr, RegisterClass cd=RD, RegisterClass c2=RD>
    : RR<op1, op2, (outs cd:$d), (ins c2:$s1, c2:$s2),
         asmstr # " $d, $s2", []>;

/// R[c], R[a], R[b]
class IRR_2<bits<8> op1, bits<8> op2, string asmstr
          , RegisterClass cd=RD, RegisterClass c1=RD, RegisterClass c2=RD>
    : RR<op1, op2, (outs cd:$d), (ins c1:$s1, c2:$s2), asmstr, []>;

class IRR_dab<bits<8> op1, bits<8> op2, string asmstr,
              RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD>
    : IRR_2<op1, op2, asmstr # " $d, $s1, $s2", RCd, RC1, RC2>;

class IRR_dba<bits<8> op1, bits<8> op2, string asmstr,
              RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD>
    : IRR_2<op1, op2, asmstr # " $d, $s2, $s1", RCd, RC1, RC2>;

class IRR_dabn<bits<8> op1, bits<8> op2, string asmstr,
              RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD, Operand TypeC=u2imm>
    : RR<op1, op2, (outs RCd:$d), (ins RC1:$s1, RC2:$s2, TypeC:$n), asmstr#" $d, $s1, $s2, $n", []>;
class IRR_dban<bits<8> op1, bits<8> op2, string asmstr,
              RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD, Operand TypeC=u2imm>
    : RR<op1, op2, (outs RCd:$d), (ins RC1:$s1, RC2:$s2, TypeC:$n), asmstr#" $d, $s2, $s1, $n", []>;

multiclass mIRR_RC<bits<8> rr1, bits<8> rr2, bits<8> rc1, bits<7> rc2, string asmstr,
                   RegisterClass RCd=RD, RegisterClass RC1=RD, Operand TypeC=s9imm> {
  def _rr : IRR_dab<rr1, rr2, asmstr, RCd, RC1>;
  def _rc : IRC<rc1, rc2, asmstr, RCd, RC1, TypeC>;
}

class IRLC<bits<8> op1, string asmstr, Operand TypeC=s16imm, RegisterClass RCd=RD, RegisterClass RC1=RD>
    : RLC<op1, (outs RCd:$d), (ins RC1:$s1, TypeC:$const16),
      asmstr # " $d, $s1, $const16",
      []>;


class ISRR_db<bits<8> op1, string asmstr, RegisterClass RCd=RD, RegisterClass RC2=RD>
    : SRR<op1, (outs RCd:$d), (ins RC2:$s2),
      asmstr # " $d, $s2", []>;

class ISRR_dD15b<bits<8> op1, string asmstr, RegisterClass RCd=RD, RegisterClass RC2=RD>
    : SRR<op1, (outs RCd:$d), (ins RC2:$s2),
      asmstr # " $d, d15, $s2", []>;

class ISRR_D15db<bits<8> op1, string asmstr, RegisterClass RCd=RD, RegisterClass RC2=RD>
    : SRR<op1, (outs RCd:$d), (ins RC2:$s2),
      asmstr # " d15, $d, $s2", []>;


multiclass mISRR_s<bits<8> op1, string asmstr>{
    def _srr : ISRR_db<op1, asmstr>;
}

multiclass mISRR_a15a<bits<8> op1, bits<8> op2, bits<8> op3,
                    string asmstr>{
  def _srr     : ISRR_db<op1, asmstr>;
  def _srr_a15 : ISRR_dD15b<op2, asmstr>, Requires<[HasV120_UP]>;
  def _srr_15a : ISRR_D15db<op3, asmstr>;
}

class IBIT<bits<8> op1, bits<2> op2, string asmstr>
    : BIT<op1, op2, (outs RD:$d), (ins RD:$s1, RD:$s2, u4imm:$pos1, u4imm:$pos_r),
      asmstr # " $d, $s1, $pos1, $s2, $pos_r",
      []>;

class NsRequires<list<Predicate> Ps> : Requires<Ps> {
  string DecoderNamespace = !cond(!eq(HasV110, !head(Ps)): "v110",
                                  !eq(HasV120, !head(Ps)): "v120",
                                  !eq(HasV130, !head(Ps)): "v130",
                                  !eq(HasV131, !head(Ps)): "v131",
                                  !eq(HasV160, !head(Ps)): "v160",
                                  !eq(HasV161, !head(Ps)): "v161",
                                  !eq(HasV162, !head(Ps)): "v162",
                                  true: "");
}

//===----------------------------------------------------------------------===//
// Pseudo Instructions
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

// Arithmetic Instructions

// Absolute Value Instructions
let Defs = [PSW] in {
   def ABS_rr : RR<0x0B, 0x1C, (outs RD:$d),
                              (ins RD:$s2),
                  "abs $d, $s2",
                  [(set RD:$d, (TriCoreAbs RD:$s2))]>;
   def ABS_B_rr : RR<0x0B, 0x5C, (outs RD:$d),
                                (ins RD:$s2),
                                 "abs.b $d, $s2",
                  [(set RD:$d, (TriCoreAbs RD:$s2))]>;
   def ABS_H_rr : RR<0x0B, 0x7C, (outs RD:$d),
                                (ins RD:$s2),
                                 "abs.h $d, $s2",
                  [(set RD:$d, (TriCoreAbs RD:$s2))]>;

   def ABSDIF_rc : RC<0x8B, 0x0E, (outs RD:$d),
                  (ins RD:$s1, s9imm:$const9), "absdif $d, $s1, $const9",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, immSExt9:$const9))]>;
   def ABSDIF_rr : RR<0x0B, 0x0E, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "absdif $d, $s1, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;
   def ABSDIF_B_rr : RR<0x0B, 0x4E, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "absdif.b $d, $s1, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;
   def ABSDIF_H_rr : RR<0x0B, 0x6E, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "absdif.h $d, $s1, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;

   def ABSDIFS_rc : RC<0x8B, 0x0F, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "absdifs $d, $s1, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;
   def ABSDIFS_rr : RR<0x0B, 0x0F, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "absdifs $d, $s1, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;
   def ABSDIFS_B_rr_v110 : RR<0x0B, 0x4F, (outs RD:$d),
                        (ins RD:$s1, RD:$s2), "absdifs.b $d, $s1, $s2",
                        [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>
                    , NsRequires<[HasV110]>;
   def ABSDIFS_H_rr : RR<0x0B, 0x6F, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "absdifs.h $d, $s1, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;

   def ABSS_rr : RR<0x0B, 0x1D, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "abss $d, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;
   def ABSS_B_rr_v110 : RR<0x0B, 0x5D, (outs RD:$d),
                        (ins RD:$s1, RD:$s2), "abss.b $d, $s2",
                        [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>
                    , NsRequires<[HasV110]>;
   def ABSS_H_rr : RR<0x0B, 0x7D, (outs RD:$d),
                  (ins RD:$s1, RD:$s2), "abss.h $d, $s2",
                  [(set RD:$d, (TriCoreAbsDif RD:$s1, RD:$s2))]>;
}

multiclass mIB_H<bits<8> brr1, bits<8> brr2, bits<8> hrr1, bits<8> hrr2,
                        string asmstr> {
  def _B_rr : IRR_dab<brr1, brr2, asmstr # ".b">;
  def _H_rr : IRR_dab<hrr1, hrr2, asmstr # ".h">;
}

// - ADD Instructions

defm ADD : mIRR_RC<0x0B, 0x00, 0x8B, 0x00, "add">,
           mISRC_a15a<0xC2, 0x92, 0x9A, "add">,
           mISRR_a15a<0x42, 0x12, 0x1A, "add">,
           mIB_H<0x0B, 0x40, 0x0B, 0x60, "add">;

multiclass mIRR_SRC_SRR__A<bits<8> rr1, bits<8> rr2, bits<8> src1, bits<8> srr1,
                        string asmstr> {
  def _rr : IRR_dab<rr1, rr2, asmstr, RA, RA, RA>;
  def _src : ISRC_dC<src1, asmstr, RA>, Requires<[HasV120_UP]>;
  def _srr : ISRR_db<srr1, asmstr, RA, RA>, Requires<[HasV120_UP]>;
}

defm ADD_A : mIRR_SRC_SRR__A<0x01, 0x01, 0xB0, 0x30, "add.a">;
defm ADDC : mIRR_RC<0x0B, 0x05, 0x8B, 0x05, "addc">;

multiclass mIRLC<bits<8> op1, bits<8> op2, bits<8> op3, string asmstr>{
  def _rlc    : IRLC<op1, asmstr>;
  def H_rlc   : IRLC<op2, asmstr # "h", u16imm>;
  def H_A_rlc : IRLC<op3, asmstr # "h.a", u16imm, RA, RA>;
}

defm ADDI : mIRLC<0x1B, 0x9B, 0x11, "addi">;

multiclass mIH_HU_U<bits<8> h1, bits<8> h2,
                    bits<8> hu1, bits<8> hu2,
                    bits<8> u1, bits<8> u2,
                    string asmstr>{
  def _H  : IRR_dab<h1, h2, asmstr # ".h">;
  def _HU : IRR_dab<hu1,hu2,asmstr # ".hu">;
  def _U  : IRR_dab<u1, u2, asmstr # ".u">;
}

defm ADDS : mIRR_RC<0x0B, 0x02, 0x8B, 0x02, "adds">,
            mISRR_s<0x22, "adds">,
            mIH_HU_U<0x0B, 0x62, 0x0B, 0x63, 0x0B, 0x03, "adds">;
def ADDS_U_rc : IRC<0x8B, 0x03, "adds.u">;
def ADDS_B_rr : IRR_dab<0x0B, 0x42, "adds.b">, NsRequires<[HasV110]>;

def ADDSC_A_srrs_v110 : SRRS<0x10, (outs RA:$d), (ins RD:$s2, u2imm:$n),
                            "addsc.a $d, $s2, $n", []>
                      , NsRequires<[HasV110]>;
def ADDSC_A_srrs: SRRS<0x10, (outs RA:$d), (ins RA:$s2, u2imm:$n),
                      "addsc.a $d, $s2, d15, $n", []>
                , Requires<[HasV120_UP]>;

def ADDSC_A_rr_v110: IRR_dabn<0x01, 0x60, "addsc.a", RA, RA, RD>, NsRequires<[HasV110]>;
def ADDSC_A_rr     : IRR_dban<0x01, 0x60, "addsc.a", RA, RD, RA>, Requires<[HasV120_UP]>;

def ADDSC_AT_rr_v110 : IRR_dab<0x01, 0x62, "addsc.at", RA, RA>, NsRequires<[HasV110]>;
def ADDSC_AT_rr : IRR_dba<0x01, 0x62, "addsc.at", RA, RD, RA>, Requires<[HasV120_UP]>;

def ADDS_BU_rr_v110 : IRR_dab<0x0B, 0x43, "adds.bu">, Requires<[HasV110]>;

defm ADDX : mIRR_RC<0x0B, 0x04, 0x8B, 0x04, "addx">;


/// AND Instructions

defm AND : mIRR_RC<0x0F, 0x08, 0x8F, 0x08, "and">;

def AND_srr      : ISRR_db<0x26, "and">, Requires<[HasV120_UP]>;
def AND_srr_v110 : ISRR_db<0x16, "and">, NsRequires<[HasV110]>;
def AND_sc       : ISC_D15C<0x16, "and">, Requires<[HasV120_UP]>;
def AND_sc_v110  : ISC_D15C<0x96, "and">, NsRequires<[HasV110]>;

def AND_AND_T  : IBIT<0x47, 0x00, "and.and.t">;
def AND_ANDN_T : IBIT<0x47, 0x03, "and.andn.t">;
def AND_NOR_T  : IBIT<0x47, 0x02, "and.nor.t">;
def AND_OR_T   : IBIT<0x47, 0x01, "and.or.t">;
def AND_T      : IBIT<0x87, 0x00, "and.t">;
def ANDN_T     : IBIT<0x87, 0x03, "andn.t">;

defm AND_EQ   : mIRR_RC<0x0B, 0x20, 0x8B, 0x20, "and.eq">;
defm AND_GE   : mIRR_RC<0x0B, 0x24, 0x8B, 0x24, "and.ge">;
defm AND_GE_U : mIRR_RC<0x0B, 0x25, 0x8B, 0x25, "and.ge.u">;
defm AND_LT   : mIRR_RC<0x0B, 0x22, 0x8B, 0x22, "and.lt">;
defm AND_LT_U : mIRR_RC<0x0B, 0x23, 0x8B, 0x23, "and.lt.u">;
defm AND_NE   : mIRR_RC<0x0B, 0x21, 0x8B, 0x21, "and.ne">;

defm ANDN     : mIRR_RC<0x0F, 0x0E, 0x8F, 0x0E, "andn">;

/// BISR
def BISR_rc : IRC_C<0xAD, 0x00, "bisr">;
def BISR_rc_v161 : IRC_C<0xAD, 0x01, "bisr">, NsRequires<[HasV161]>;

def BISR_sc_v110 : ISC_C<0xC0, "bisr">, NsRequires<[HasV110]>;
def BISR_sc : ISC_C<0xE0, "bisr">, Requires<[HasV120_UP]>;

/// Multiple Instructions (RR)
def BMERGAE_rr_v110 : IRR_dab<0x4B, 0x00, "bmerge">, NsRequires<[HasV110]>;
def BMERGE_rr : IRR_dab<0x4B, 0x01, "bmerge">, Requires<[HasV120_UP]>;

def BSPLIT_rr_v110: IRR_a<0x4B, 0x60, "bsplit", RE>, NsRequires<[HasV110]>;
def BSPLIT_rr     : IRR_a<0x4B, 0x09, "bsplit", RE>, Requires<[HasV120_UP]>;

/// BO Opcode Formats
// A[b], off10 (BO) (Base + Short Offset Addressing Mode)
class IBO_bso<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RA:$s2, s10imm:$off10),
      asmstr # " [$s2]$off10", []>;
// P[b] (BO) (Bit Reverse Addressing Mode)
class IBO_r<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RP:$s2),
      asmstr # " [${s2}+r]", []>;
// P[b], off10 (BO) (Circular Addressing Mode)
class IBO_c<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RP:$s2, s10imm:$off10),
      asmstr # " [${s2}+c]$off10", []>;
// A[b], off10 (BO) (Post-increment Addressing Mode)

class IBO_pos<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RA:$s2, s10imm:$off10),
      asmstr # " [${s2}+]$off10", []>;
// A[b], off10 (BO) (Pre-increment Addressing Mode)
class IBO_pre<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RA:$s2, s10imm:$off10),
      asmstr # " [+${s2}]$off10", []>;


multiclass mI_CACHEI_<bits<8> prefix, bits<6> op12, bits<6> op22, bits<6> op32, string asmstr> {
    def _bo_bso : IBO_bso<prefix, op12, asmstr>;
    def _bo_pos : IBO_pos<prefix, op22, asmstr>;
    def _bo_pre : IBO_pre<prefix, op32, asmstr>;
}

multiclass mI_CACHE_<bits<8> prefixi, bits<8> prefix_r_c, bits<6> bso, bits<6> pos_r, bits<6> pre_c, string asmstr>{
  defm "" : mI_CACHEI_<prefixi, bso, pos_r, pre_c, asmstr>;
  def _bo_r: IBO_r<prefix_r_c, pos_r, asmstr>;
  def _bo_c: IBO_c<prefix_r_c, pre_c, asmstr>;
}

/// CACHEA.* Instructions

defm CACHEA_I : mI_CACHE_<0x89, 0xA9, 0x2E, 0x0E, 0x1E, "cachea.i">, Requires<[HasV120_UP]>;
defm CACHEA_W : mI_CACHE_<0x89, 0xA9, 0x2C, 0x0C, 0x1C, "cachea.w">, Requires<[HasV120_UP]>;
defm CACHEA_WI: mI_CACHE_<0x89, 0xA9, 0x2D, 0x0D, 0x1D, "cachea.wi">, Requires<[HasV120_UP]>;

defm CACHEI_W : mI_CACHEI_<0x89, 0x2B, 0x0B, 0x1B, "cachei.w">, Requires<[HasV131_UP]>;
defm CACHEI_I : mI_CACHEI_<0x89, 0x2A, 0x0A, 0x1A, "cachei.i">, Requires<[HasV160_UP]>;
defm CACHEI_WI: mI_CACHEI_<0x89, 0x2F, 0x0F, 0x1F, "cachei.wi">, Requires<[HasV131_UP]>;


/// RRR Opcodes Formats

class IRRR<bits<8>op1, bits<4> op2, string asmstr,
           RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD, RegisterClass RC3=RD>
    : RRR<op1, op2, (outs RCd:$d), (ins RC1:$s1, RC2:$s2, RC3:$s3),
          asmstr # " $d, $s3, $s1, $s2", []>;

class IRRR_d31<bits<8>op1, bits<4> op2, string asmstr,
             RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD, RegisterClass RC3=RD>
    : RRR<op1, op2, (outs RCd:$d), (ins RC1:$s1, RC2:$s2, RC3:$s3), asmstr # " $d, $s3, $s1", []>;

class IRRR_d32<bits<8>op1, bits<4> op2, string asmstr,
              RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC2=RD, RegisterClass RC3=RD>
    : RRR<op1, op2, (outs RCd:$d), (ins RC1:$s1, RC2:$s2, RC3:$s3),
          asmstr # " $d, $s3, $s2", []>;

/// RCR Opcodes Formats
class IRCR<bits<8> op1, bits<3> op2, string asmstr,
           RegisterClass RCd=RD, RegisterClass RC1=RD, RegisterClass RC3=RD, Operand TypeC=s9imm>
    : RCR<op1, op2, (outs RCd:$d), (ins RC1:$s1, RC3:$s3, TypeC:$const9),
          asmstr # " $d, $s3, $s1, $const9", []>;

multiclass mIRCR<bits<8>op1, bits<3> op2, bits<8>op3, bits<3> op4, string asmstr>{
  def _rcr   : IRCR<op1, op2, asmstr>;
  def _rcr_e : IRCR<op3, op4, asmstr, RE, RD, RE, s9imm>, Requires<[HasV120_UP]>;
}

/// CADD Instructions
def CADD_srr_v110 : ISRR_dD15b<0x0A, "cadd">, NsRequires<[HasV110]>;

def CADD_rcr : IRCR<0xAB, 0x00, "cadd">;
def CADD_rrr : IRRR<0x2B, 0x00, "cadd">;
def CADD_src : ISRC_dD15C<0x8A, "cadd">;

multiclass mI_CADDnA_CSUBnA_v110_<bits<8> rrr1, bits<4> rrr2, bits<8> rcr1, bits<3> rcr2, string asmstr>{
  def _rrr_v110: IRRR<rrr1, rrr2, asmstr, RA, RA, RA, RD>, NsRequires<[HasV110]>;
  if !or(!eq(asmstr, "cadd.a"), !eq(asmstr, "caddn.a")) then {
    def _rcr_v110: RCR<rcr1, rcr2, (outs RA:$d), (ins RA:$s1, RD:$s3, s9imm:$const9),
                     asmstr#" $d, $s3, $s1, $const9", []>
               , NsRequires<[HasV110]>;
  }
}

defm CADD_A: mI_CADDnA_CSUBnA_v110_<0x21, 0x00, 0xA1, 0x00, "cadd.a">;

def CADDN_srr_v110  : ISRR_dD15b<0x4A, "caddn">
                    , NsRequires<[HasV110]>;

def CADDN_rcr : IRCR<0xAB, 0x01, "caddn">;
def CADDN_rrr : IRRR<0x2B, 0x01, "caddn">;
def CADDN_src : ISRC_dD15C<0xCA, "caddn">;

defm CADDN_A: mI_CADDnA_CSUBnA_v110_<0x21, 0x01, 0xA1, 0x01, "caddn.a">;

// Call Instructions

class IB<bits<8> op1, string asmstr>
    : B<op1, (outs), (ins disp24imm:$disp24),
        asmstr # " $disp24", []>;

// The target of a 24-bit call instruction.
def call_target : Operand<i32> {
  let EncoderMethod = "encodeCallTarget";
}

class ISB<bits<8> op1, string asmstr>
    : SB<op1, (outs), (ins disp8imm:$disp8), asmstr # " $disp8", []>;

class ISB_D15D<bits<8> op1, string asmstr>
    : SB<op1, (outs), (ins disp8imm:$disp8), asmstr # " d15, $disp8", []>;

let isCall = 1,
    Defs = [A11],
    Uses = [A10] in {
  def CALL_b    : IB<0x6D, "call">;
  def CALL_sb   : ISB<0x5C, "call">, Requires<[HasV120_UP]>;
  def CALLA_b   : IB<0xED, "calla">;
  def CALLI_rr_v110: IRR_R2<0x2D, 0x00, "calli", RA>, NsRequires<[HasV110]>;
  def CALLI_rr  : IRR_R1<0x2D, 0x00, "calli", RA>, Requires<[HasV120_UP]>;
}

multiclass mI_H<bits<8> op1,bits<8> op2,bits<8> op3, bits<8> op4, string asmstr> {
  def _rr   : IRR_a<op1, op2, asmstr>;
  def _H_rr : IRR_a<op3, op4, asmstr # ".h">;
}

defm CLO : mI_H<0x0F, 0x1C, 0x0F, 0x7D, "clo">;
def CLO_B_rr_v110 : IRR_a<0x0F, 0x3D, "clo.b">, NsRequires<[HasV110]>;
defm CLS : mI_H<0x0F, 0x1D, 0x0F, 0x7E, "cls">;
def CLS_B_rr_v110 : IRR_a<0x0F, 0x3E, "cls.b">, NsRequires<[HasV110]>;
defm CLZ : mI_H<0x0F, 0x1B, 0x0F, 0x7C, "clz">;
def CLZ_B_rr_v110 : IRR_a<0x0F, 0x3C, "clz.b">, NsRequires<[HasV110]>;

def CMOV_src  : ISRC_dD15C<0xAA, "cmov">;
def CMOV_srr  : ISRR_dD15b<0x2A, "cmov">;
def CMOVN_src : ISRC_dD15C<0xEA, "cmovn">;
def CMOVN_srr : ISRR_dD15b<0x6A, "cmovn">;

// A[b], off10, E[a] (BO)(Base + Short Offset Addressing Mode)
class IBO_bsoAbOEa<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RE:$s1, RA:$s2, s10imm:$off10),
      asmstr # " [$s2]$off10, $s1", []>;
// P[b], E[a] (BO)(Bit-reverse Addressing Mode)
class IBO_rPbEa<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RE:$s1, RP:$s2),
      asmstr # " [${s2}+r], $s1", []>;
// P[b], off10, E[a] (BO)(Circular Addressing Mode)
class IBO_cPbOEa<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RE:$s1, RP:$s2, s10imm:$off10),
      asmstr # " [${s2}+c]$off10, $s1", []>;
// A[b], off10, E[a] (BO)(Post-increment Addressing Mode)
class IBO_posAbOEa<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RE:$s1, RA:$s2, s10imm:$off10),
      asmstr # " [${s2}+]$off10, $s1", []>;
// A[b], off10, E[a] (BO)(Pre-increment Addressing Mode)
class IBO_preAbOEa<bits<8> op1, bits<6> op2, string asmstr> 
    : BO<op1, op2, (outs), (ins RE:$s1, RA:$s2, s10imm:$off10),
      asmstr # " [+$s2]$off10, $s1", []>;


multiclass mIBO_Ea<bits<8> bso1, bits<6> bso2, ///_bso
                   bits<8> r1, bits<6> r2, ///_r
                   bits<8> c1, bits<6> c2, ///_c
                   bits<8> pos1, bits<6> pos_r, ///_post
                   bits<8> pre1, bits<6> pre_c, ///_pre
                   string asmstr>{
  def _bo_bso  : IBO_bsoAbOEa<bso1, bso2, asmstr>;
  def _bo_pos  : IBO_posAbOEa<pos1, pos_r, asmstr>;
  def _bo_pre  : IBO_preAbOEa<pre1, pre_c, asmstr>;
  def _bo_r    : IBO_rPbEa<r1, r2, asmstr>;
  def _bo_c    : IBO_cPbOEa<c1, c2, asmstr>;
}

defm CMPSWAP_W : mIBO_Ea<0x49, 0x23, 0x69, 0x03,
                         0x69, 0x13, 0x49,0x03,
                         0x49, 0x13, "cmpswap.w">
               , Requires<[HasV161_UP]>;

def CRC32_B_rr  : IRR_dba<0x4B, 0x06, "crc32.b">, Requires<[HasV162]>;
def CRC32B_W_rr : IRR_dba<0x4B, 0x03, "crc32b.w">, Requires<[HasV162]>;
def CRC32L_W_rr : IRR_dba<0x4B, 0x07, "crc32l.w">, Requires<[HasV162]>;
def CRCN_rrr    : IRRR<0x6B, 0x01, "crcn">, Requires<[HasV162]>;

def CSUB_rrr    : IRRR<0x2B, 0x02, "csub">;
def CSUBN_rrr   : IRRR<0x2B, 0x03, "csubn">;

defm CSUB_A_: mI_CADDnA_CSUBnA_v110_<0x21, 0x02, 0, 0, "csub.a">;
defm CSUBN_A_: mI_CADDnA_CSUBnA_v110_<0x21, 0x03, 0, 0, "csubn.a">;

class ISR_0<bits<8> op1, bits<4> op2, string asmstr>
    : SR<op1, op2, (outs), (ins),
        asmstr, []>;

class ISR_1<bits<8> op1, bits<4> op2, string asmstr, RegisterClass RC1=RD>
    : SR<op1, op2, (outs), (ins RC1:$s1),
        asmstr # " $s1", []>;

class ISYS_0<bits<8> op1, bits<6> op2, string asmstr>
    : SYS<op1, op2, (outs), (ins),
        asmstr, []>;
class ISYS_1<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC1=RD>
    : SYS<op1, op2, (outs), (ins RC1:$s1),
        asmstr # " $s1", []>;

def DEBUG_sr  : ISR_0<0x00, 0x0A, "debug">;
def DEBUG_sys : ISYS_0<0x0D, 0x04, "debug">;

/// RRRR Instruction Formats
/// op D[c], D[a], D[b], D[d]
class IRRRR<bits<8> op1, bits<3> op2, string asmstr> 
    : RRRR<op1, op2, (outs RD:$d), (ins RD:$s1, RD:$s2, RD:$s3),
            asmstr # " $d, $s1, $s2, $s3", []>;
/// op D[c], D[a], D[d]
class IRRRR_ad<bits<8> op1, bits<3> op2, string asmstr, RegisterClass RC3=RD> 
    : RRRR<op1, op2, (outs RD:$d), (ins RD:$s1, RC3:$s3),
            asmstr # " $d, $s1, $s3", []>;

/// op D[c], D[a], D[b], D[d], width
class IRRRW_cabdw<bits<8> op1, bits<3> op2, string asmstr>
     : RRRW<op1, op2, (outs RD:$d), (ins RD:$s1, RD:$s2, RD:$s3, i32imm:$width),
            asmstr # " $d, $s1, $s2, $s3, $width", []>;
/// op D[c], D[a], D[d], width
class IRRRW_cadw<bits<8> op1, bits<3> op2, string asmstr>
     : RRRW<op1, op2, (outs RD:$d), (ins RD:$s1, RD:$s2, RD:$s3, i32imm:$width),
            asmstr # " $d, $s1, $s3, $width", []>;
/// op E[c], D[b], D[d], width
class IRRRW_cEbdw<bits<8> op1, bits<3> op2, string asmstr>
     : RRRW<op1, op2, (outs RE:$d), (ins RD:$s1, RD:$s2, RD:$s3, i32imm:$width),
            asmstr # " $d, $s2, $s3, $width", []>;

def DEXTR_rrpw : RRPW<0x77, 0x00, (outs RD:$d), (ins RD:$s1, RD:$s2, i32imm:$pos, i32imm:$width),
                      "dextr $d, $s1, $s2, $pos", []>;
def DEXTR_rrrr : IRRRR<0x17, 0x04, "dextr">;

def DIFSC_A_rr_v110 : IRR_dabn<0x01, 0x50, "difsc.a", RD, RA, RA>, NsRequires<[HasV110]>;

def DISABLE_sys   : ISYS_0<0x0D, 0x0D, "disable">;
def DISABLE_sys_1 : ISYS_1<0x0D, 0x0F, "disable">, Requires<[HasV160_UP]>;

def DSYNC_sys : ISYS_0<0x0D, 0x12, "dsync">;

def DVADJ_srr_v110 : ISRR_db<0x72, "dvadj", RE, RD>, NsRequires<[HasV110]>;
def DVADJ_rrr_v110 : IRRR_d32<0x2B, 0x08, "dvadj", RE, RD, RD, RE>, NsRequires<[HasV110]>;
def DVADJ_rrr : IRRR_d32<0x6B, 0x0D, "dvadj", RE, RD, RD, RE>, Requires<[HasV120_UP]>;

multiclass mI_U_RR_Eab<bits<8> op1, bits<8> op2, bits<8> op3, bits<8> op4,
                string asmstr, string posfix = ""> {
  def _rr   # posfix : IRR_dab<op1, op2, asmstr, RE>;
  def _U_rr # posfix : IRR_dab<op3, op4, asmstr # ".u", RE>;
}

multiclass mIU_RR_Eab<bits<8> op1, bits<8> op2, bits<8> op3, bits<8> op4,
                string asmstr, string posfix = ""> {
  def _rr  # posfix : IRR_dab<op1, op2, asmstr, RE>;
  def U_rr # posfix : IRR_dab<op3, op4, asmstr # "u", RE>;
}

multiclass mI_DVINIT_<bits<8> oprefix,
                   bits<8> op, bits<8> op_u,
                   bits<8> opb, bits<8> opbu,
                   bits<8> oph, bits<8> ophu,
                   string asmstr, string posfix = ""> {
defm "": mI_U_RR_Eab<oprefix, op,  oprefix, op_u, asmstr, posfix>;
defm _B: mIU_RR_Eab <oprefix, opb, oprefix, opbu, asmstr # ".b", posfix>;
defm _H: mIU_RR_Eab <oprefix, oph, oprefix, ophu, asmstr # ".h", posfix>;
}

defm DIV     : mI_U_RR_Eab<0x4B, 0x20, 0x4B, 0x21, "div">, Requires<[HasV160_UP]>;

defm DVINIT : mI_DVINIT_<0x4F, 0x00, 0x01, 0x04, 0x05, 0x02, 0x03, "dvinit", "_v110">, NsRequires<[HasV110]>;
defm DVINIT : mI_DVINIT_<0x4B, 0x1A, 0x0A, 0x5A, 0x4A, 0x3A, 0x2A, "dvinit">, Requires<[HasV120_UP]>;

multiclass mI_U_RRR_EEdb<bits<8> op1, bits<4> op2, bits<8> op3, bits<4> op4,
                string asmstr, string posfix = ""> {
  def _rrr # posfix  : IRRR_d32<op1, op2, asmstr, RE, RD, RD, RE>;
  def _U_rrr # posfix: IRRR_d32<op3, op4, asmstr # ".u", RE, RD, RD, RE>;
}

multiclass mI_U_SRR_sds2<bits<8> op1, bits<8> op2, string asmstr, 
                        string posfix = "", RegisterClass RC1, RegisterClass RC2>{
  def "" # posfix: ISRR_db<op1, asmstr, RC1, RC2>;
  def _U # posfix: ISRR_db<op2, asmstr # ".u", RC1, RC2>;
}

defm DVSTEP : mI_U_SRR_sds2<0x32, 0xB2, "dvstep", "v110", RE, RD>, NsRequires<[HasV110]>;
defm DVSTEP : mI_U_RRR_EEdb<0x2B, 0x09, 0x2B, 0x0A, "dvstep", "v110">, NsRequires<[HasV110]>;
defm DVSTEP : mI_U_RRR_EEdb<0x6B, 0x0F, 0x6B, 0x0E, "dvstep">, Requires<[HasV120_UP]>;

def ENABLE_sys   : ISYS_0<0x0D, 0x0C, "enable">;

multiclass mIB_H_W<bits<8> brr1, bits<8> brr2,
                   bits<8> hrr1, bits<8> hrr2,
                   bits<8> wrr1, bits<8> wrr2,
                   string asmstr>
        :  mIB_H<brr1, brr2, hrr1, hrr2, asmstr>{
  def _W_rr : IRR_dab<wrr1, wrr2, asmstr # ".w">;
}

defm EQ    : mIRR_RC<0x0B, 0x10, 0x8B, 0x10, "eq">
           , mIB_H_W<0x0B, 0x50, 0x0B, 0x70, 0x0B, 0x90, "eq">;
def EQ_src : ISRC_D15dC<0xBA, "eq">;
def EQ_srr : ISRR_D15db<0x3A, "eq">;
def EQ_A_rr: IRR_dab<0x01, 0x40, "eq.a", RD, RA, RA>;

defm EQANY_B : mIRR_RC<0x0B, 0x56, 0x8B, 0x56, "eqany.b">;
defm EQANY_H : mIRR_RC<0x0B, 0x76, 0x8B, 0x76, "eqany.h">;

def EQZ_A_rr : IRR_a<0x01, 0x48, "eqz.a", RD, RA>;

def EXTR_rrpw : RRPW<0x37, 0x02, (outs RD:$d), (ins RD:$s1, RD:$s2, i32imm:$pos, i32imm:$width),
                    "extr $d, $s1, $pos, $width", []>;
def EXTR_rrrr : IRRRR_ad<0x17, 0x02, "extr", RE>;
def EXTR_rrrw : IRRRW_cadw<0x57, 0x02, "extr">;

def EXTR_U_rrpw : RRPW<0x37, 0x03, (outs RD:$d), (ins RD:$s1, RD:$s2, i32imm:$pos, i32imm:$width),
                      "extr.u $d, $s1, $pos, $width", []>;
def EXTR_U_rrrr : IRRRR_ad<0x17, 0x03, "extr.u", RE>;
def EXTR_U_rrrw : IRRRW_cadw<0x57, 0x03, "extr.u">;

def FCALL_b  : IB<0x61, "fcall">, Requires<[HasV160_UP]>;
def FCALLA_b : IB<0xE1, "fcalla">, Requires<[HasV160_UP]>;
def FCALLA_i : IRR_R1<0x2D, 0x01, "fcalli", RA>, Requires<[HasV160_UP]>;

def FRET_sr : ISR_0<0x00, 0x07, "fret">, Requires<[HasV160_UP]>;
def FRET_sys : ISYS_0<0x0D, 0x03, "fret">, Requires<[HasV160_UP]>;

multiclass mI_U__RR_RC<bits<8> op1, bits<8> op2, bits<8> op3, bits<7> op4,
                      bits<8> uop1, bits<8> uop2, bits<8> uop3, bits<7> uop4,
                      string asmstr> {
  defm ""  : mIRR_RC<op1, op2, op3, op4, asmstr>;
  defm _U  : mIRR_RC<uop1, uop2, uop3, uop4, asmstr # ".u">;
}

defm GE : mI_U__RR_RC<0x0B, 0x14, 0x8B, 0x14,
                     0x0B, 0x15, 0x8B, 0x15, "ge">;
def GE_A_rr : IRR_dab<0x01, 0x43, "ge.a", RD, RA, RA>;

def IMASK_rcpw : RCPW<0xB7, 0x01, (outs RE:$d), (ins RD:$s1, i32imm:$const4, i32imm:$pos, i32imm:$width),
                      "imask $d, $const4, $pos, $width", []>;
def IMASK_rcrw : RCRW<0xD7, 0x01, (outs RE:$d), (ins RD:$s1, RD:$s3, i32imm:$const4, i32imm:$width),
                      "imask $d, $const4, $s3, $width", []>;
def IMASK_rrpw : RRPW<0x37, 0x01, (outs RE:$d), (ins RD:$s1, RD:$s2, i32imm:$pos, i32imm:$width),
                      "imask $d, $s2, $pos, $width", []>;
def IMASK_rrrw : IRRRW_cEbdw<0x57, 0x01, "imask">;

def INS_T  : IBIT<0x67, 0x00, "ins.t">;
def INSN_T : IBIT<0x67, 0x01, "insn.t">;

def INSERT_rcpw : RCPW<0xB7, 0x00,  (outs RD:$d), (ins RD:$s1, i32imm:$const4, i32imm:$pos, i32imm:$width),
                      "insert $d, $s1, $const4, $pos, $width", []>;
def INSERT_rcrr : RCRR<0x97, 0x00, (outs RD:$d), (ins RD:$s1, i32imm:$const4, RE:$s3),
                      "insert $d, $s1, $const4, $s3", []>;
def INSERT_rcrw : RCRW<0xD7, 0x00, (outs RD:$d), (ins RD:$s1, RD:$s3, i32imm:$const4, i32imm:$width),
                        "insert $d, $s1, $const4, $s3, $width", []>;
def INSERT_rrpw : RRPW<0x37, 0x00, (outs RD:$d), (ins RD:$s1, RD:$s2, i32imm:$pos, i32imm:$width),
                      "insert $d, $s1, $s2, $pos, $width", []>;
def INSERT_rrrr : RRRW<0x17, 0x00, (outs RD:$d), (ins RD:$s1, RD:$s2, RE:$s3),
                      "insert $d, $s1, $s2, $s3", []>;
def INSERT_rrrw : IRRRW_cabdw<0x57, 0x00, "insert">;

def ISYNC_sys : ISYS_0<0x0D, 0x13, "isync">;

defm IXMAX : mI_U_RRR_EEdb<0x6B, 0x0A, 0x6B, 0x0B, "ixmax">, Requires<[HasV130_UP]>;
defm IXMIN : mI_U_RRR_EEdb<0x6B, 0x08, 0x6B, 0x09, "ixmin">, Requires<[HasV130_UP]>;

def J_b  : IB<0x1D, "j">;
def J_sb_v110 : ISB<0x5C, "j">, NsRequires<[HasV110]>;
def J_sb : ISB<0x3C, "j">, Requires<[HasV120_UP]>;
def JA_b : IB<0x9D, "ja">;

// disp15
class IBRR_0<bits<8> op1, bits<1> op2, string asmstr>
    : BRR<op1, op2, (outs), (ins disp15imm:$disp15), asmstr # " $disp15", []>;
// A[a], disp15
class IBRR_1<bits<8> op1, bits<1> op2, string asmstr>
    : BRR<op1, op2, (outs), (ins RA:$s1, disp15imm:$disp15), asmstr # " $s1, $disp15", []>;
// D[a], D[b], disp15
class IBRR<bits<8> op1, bits<1> op2, string asmstr, RegisterClass RC1=RD, RegisterClass RC2=RD>
    : BRR<op1, op2, (outs), (ins RC1:$s1, RC2:$s2, disp15imm:$disp15),
          asmstr # " $s1, $s2, $disp15", []>;


class IBRC<bits<8> op1, bits<1> op2, string asmstr, Operand TypeC=u4imm>
    : BRC<op1, op2, (outs), (ins RD:$s1, TypeC:$const4, disp15imm:$disp15),
      !strconcat(asmstr, " $s1, $const4, $disp15"), []>;

class ISBC<bits<8> op1, string asmstr>
    : SBC<op1, (outs), (ins disp4imm:$disp4, s4imm:$const4),
      !strconcat(asmstr, " d15, $const4, $disp4"), []>;

// D[15], D[b], disp4 (SBR)
class ISBR_15b<bits<8> op1, string asmstr>
    : SBR<op1, (outs), (ins RD:$s2, disp4imm:$disp4),
      !strconcat(asmstr, " d15, $s2, $disp4"), []>;
// D[b], disp4 (SBR)
class ISBR_b<bits<8> op1, string asmstr, RegisterClass RC2=RD>
    : SBR<op1, (outs), (ins RC2:$s2, disp4imm:$disp4),
      !strconcat(asmstr, " $s2, $disp4"), []>;

// D[b](SBR)
class ISBR<bits<8> op1, string asmstr, RegisterClass RC2=RD>
    : SBR<op1, (outs), (ins RC2:$s2),
      !strconcat(asmstr, " $s2"), []>;

multiclass mIBRC_BRR<bits<8> c1, bits<1> c2, bits<8> r1, bits<1> r2, string asmstr, Operand TypeC=u4imm>{
  def _brc : IBRC<c1, c2, asmstr, TypeC>;
  def _brr : IBRR<r1, r2, asmstr>;
}

multiclass mI_JnEq_<bits<8> c1, bits<1> c2, bits<8> r1, bits<1> r2,
                    bits<8> x1, bits<8> x2, bits<8> x3, bits<8> x4,
                    bits<8> x5, bits<1> x6, bits<8> v1, bits<8> v2, string asmstr>{
  defm "": mIBRC_BRR<c1, c2, r1, r2, asmstr, s4imm>;

  def _sbr_v110 : ISBR_15b<v1, asmstr>, NsRequires<[HasV110]>;
  def _sbc_v110 : ISBC<v2, asmstr>, NsRequires<[HasV110]>;

  def _sbc1 : ISBC<x1, asmstr>, Requires<[HasV120_UP]>;
  def _sbc2 : ISBC<x2, asmstr>, Requires<[HasV160_UP]>;
  
  if !eq(asmstr, "jne") then def _sbr1 : ISBR_15b<x3, asmstr>, Requires<[HasV120_UP]>;
  if !eq(asmstr, "jeq") then def _sbr1 : ISBR_15b<x3, asmstr>, Requires<[HasV130_UP]>;

  def _sbr2 : ISBR_15b<x4, asmstr>, Requires<[HasV160_UP]>;
  def _A_brr: IBRR<x5, x6, asmstr # ".a", RA, RA>;
}

defm JEQ : mI_JnEq_<0xDF, 0x00, 0x5F, 0x00,
                    0x1E, 0x9E, 0x3E, 0xBE,
                    0x7D, 0x00, 0x1E, 0x6E, "jeq">; 

defm JGE   : mIBRC_BRR<0xFF, 0x00, 0x7F, 0x00, "jge", s4imm>;
defm JGE_U : mIBRC_BRR<0xFF, 0x01, 0x7F, 0x01, "jge.u">;

def JGEZ_sbr_v110 : ISBR_b<0xFE, "jgez">, NsRequires<[HasV110]>;
def JGEZ_sbr : ISBR_b<0xCE, "jgez">, Requires<[HasV120_UP]>;
def JGTZ_sbr_v110 : ISBR_b<0x7E, "jgtz">, NsRequires<[HasV110]>;
def JGTZ_sbr : ISBR_b<0x4E, "jgtz">, Requires<[HasV120_UP]>;

def JI_sbr_v110 : ISBR<0x3C, "ji", RA>, NsRequires<[HasV110]>;
def JI_rr_v110  : IRR_R1<0x2D, 0x03, "ji", RA>, NsRequires<[HasV110]>;
def JI_rr : IRR_R1<0x2D, 0x03, "ji", RA>, Requires<[HasV120_UP]>;
def JI_sr : SR<0xDC, 0x00, (outs), (ins RA:$s1), "ji $s1", []>, Requires<[HasV120_UP]>;

def JL_b  : IB<0x5D, "jl">;
def JLA_b : IB<0xDD, "jla">;

def JLEZ_sbr_v110 : ISBR_b<0xBE, "jlez">, NsRequires<[HasV110]>;
def JLEZ_sbr : ISBR_b<0x8E, "jlez">, Requires<[HasV120_UP]>;

def JLI_rr_v110 : IRR_R1<0x2D, 0x02, "jli", RA>, NsRequires<[HasV110]>;
def JLI_rr : IRR_R1<0x2D, 0x02, "jli", RA>, Requires<[HasV120_UP]>;

defm JLT   : mIBRC_BRR<0xBF, 0x00, 0x3F, 0x00, "jlt">;
defm JLT_U : mIBRC_BRR<0xBF, 0x01, 0x3F, 0x01, "jlt.u">;

def JLTZ_sbr_v110 : ISBR_b<0x3E, "jltz">, NsRequires<[HasV110]>;
def JLTZ_sbr : ISBR_b<0x0E, "jltz">, Requires<[HasV120_UP]>;

defm JNE : mI_JnEq_<0xDF, 0x01, 0x5F, 0x01,
                    0x5E, 0xDE, 0x7E, 0xFE,
                    0x7D, 0x01, 0x9E, 0xEE, "jne">;

defm JNED : mIBRC_BRR<0x9F, 0x01, 0x1F, 0x01, "jned">;
defm JNEI : mIBRC_BRR<0x9F, 0x00, 0x1F, 0x00, "jnei">;

multiclass mI_JnZ_<bits<8> sb, bits<8> sbr,
                   bits<8> abrr1, bits<1> abrr2, bits<8> asbr,
                   bits<7> brn1, bits<1> brn2, bits<8> sbrn,
                   bits<8> sbv, bits<8> sbrv, bits<8> sbrnv,
                   string asmstr> {
  def _sb_v110  : ISB_D15D<sbv, asmstr>, NsRequires<[HasV110]>;
  def _sbr_v110 : ISBR_b<sbrv, asmstr>, NsRequires<[HasV110]>;
  def _T_sbrn_v110: SBRN<sbrnv, (outs), (ins i32imm:$n, disp4imm:$disp4), asmstr # ".t d15, $n, $disp4", []>
             , NsRequires<[HasV110]>;

  def _sb  : ISB_D15D<sb, asmstr>, Requires<[HasV120_UP]>;
  def _sbr : ISBR_b<sbr, asmstr>, Requires<[HasV120_UP]>;
  def _A_brr : IBRR_1<abrr1, abrr2, asmstr # ".a">;
  def _A_sbr : ISBR_b<asbr, asmstr # ".a", RA>;
  def _T_brn : BRN<brn1, brn2, (outs), (ins RD:$s1, i32imm:$n, disp15imm:$disp15), asmstr # ".t $s1, $n, $disp15", []>;
  def _T_sbrn: SBRN<sbrn, (outs), (ins i32imm:$n, disp4imm:$disp4), asmstr # ".t d15, $n, $disp4", []>
             , Requires<[HasV120_UP]>;
}

defm JNZ : mI_JnZ_<0xEE, 0xF6, 0xBD, 0x01, 0x7C, 0x6F, 0x01, 0xAE, 0xAE, 0xDE, 0x4E, "jnz">;
defm JZ  : mI_JnZ_<0x6E, 0x76, 0xBD, 0x00, 0xBC, 0x6F, 0x00, 0x2E, 0x2E, 0x5E, 0x0E, "jz">;


class IABS_off18<bits<8> op1, bits<2> op2, string asmstr> 
    : ABS<op1, op2, (outs), (ins off18imm:$off18),
      asmstr # " $off18", []>;
class IABS_RO<bits<8> op1, bits<2> op2, string asmstr, RegisterClass dc> 
    : ABS<op1, op2, (outs dc:$d), (ins off18imm:$off18),
      asmstr # " $d, $off18", []>;
class IABS_OR<bits<8> op1, bits<2> op2, string asmstr, RegisterClass s1c> 
    : ABS<op1, op2, (outs), (ins s1c:$s1, off18imm:$off18),
      asmstr # " $off18, $s1", []>;

class IBOL_RAaO<bits<8> op1, string asmstr, RegisterClass RC> 
    : BOL<op1, (outs RC:$s1), (ins RA:$s2, s16imm:$off16),
      asmstr # " $s1, [$s2]$off16", []>;

class IBOL_AbOR<bits<8> op1, string asmstr, RegisterClass RC> 
    : BOL<op1, (outs RA:$s2), (ins RC:$s1, s16imm:$off16),
      asmstr # " [$s2]$off16, $s1", []>;

class ISLR<bits<8> op1, string asmstr, RegisterClass dc>
    : SLR<op1, (outs dc:$d), (ins RA:$s2),
      asmstr # " $d, [$s2]", []>;
class ISLR_pos<bits<8> op1, string asmstr, RegisterClass dc>
    : SLR<op1, (outs dc:$d), (ins RA:$s2),
      asmstr # " $d, [${s2}+]", []>;

class ISLRO<bits<8> op1, string asmstr, RegisterClass dc>
    : SLRO<op1, (outs dc:$d), (ins u4imm:$off4),
      asmstr # " $d, [a15]$off4", []>;

class ISRO_A15RO<bits<8> op1, string asmstr, RegisterClass s2c>
    : SRO<op1, (outs), (ins s2c:$s2, u4imm:$off4),
      asmstr # " a15, [$s2]$off4", []>;

class ISRO_ROA15<bits<8> op1, string asmstr, RegisterClass s2c>
    : SRO<op1, (outs), (ins s2c:$s2, u4imm:$off4),
      asmstr # " [$s2]$off4, a15", []>;

class ISRO_D15RO<bits<8> op1, string asmstr, RegisterClass s2c>
    : SRO<op1, (outs), (ins s2c:$s2, u4imm:$off4),
      asmstr # " d15, [$s2]$off4", []>;

class ISRO_ROD15<bits<8> op1, string asmstr, RegisterClass s2c>
    : SRO<op1, (outs), (ins s2c:$s2, u4imm:$off4),
      asmstr # " [$s2]$off4, d15", []>;

// A|D[a], A[b], off10 (BO) (Base + Short Offset Addressing Mode)
class IBO_RAbso<bits<8> op1, bits<6> op2, string asmstr, RegisterClass dc> 
    : BO<op1, op2, (outs dc:$d), (ins RA:$s2, s10imm:$off10),
      asmstr # " $d, [$s2]$off10", []>;
// A|D[a], P[b] (BO) (Bit Reverse Addressing Mode)
class IBO_RPr<bits<8> op1, bits<6> op2, string asmstr, RegisterClass dc>
    : BO<op1, op2, (outs dc:$d), (ins RP:$s2),
      asmstr # " $d, [${s2}+r]", []>;
// A|D[a], P[b], off10 (BO) (Circular Addressing Mode)
class IBO_RPc<bits<8> op1, bits<6> op2, string asmstr, RegisterClass dc> 
    : BO<op1, op2, (outs dc:$d), (ins RP:$s2, s10imm:$off10),
      asmstr # " $d, [${s2}+c]$off10", []>;
// A|D[a], A[b], off10 (BO)(Post-increment Addressing Mode)
class IBO_RApos<bits<8> op1, bits<6> op2, string asmstr, RegisterClass dc>
    : BO<op1, op2, (outs), (ins dc:$s1, RA:$s2, s10imm:$off10),
      asmstr # " $s1, [${s2}+]$off10", []>;
// A|D[a], A[b], off10 (BO) (Pre-increment Addressing Mode)
class IBO_RApre<bits<8> op1, bits<6> op2, string asmstr, RegisterClass dc>
    : BO<op1, op2, (outs), (ins dc:$s1, RA:$s2, s10imm:$off10),
      asmstr # " $s1, [+${s2}]$off10", []>;


multiclass mI_LD_<bits<8> abs1, bits<2> abs2,   ///_abs
                    bits<8> prefix1, bits<8> prefix2,
                    bits<6> bso2, ///_bso
                    bits<6> pos_r, ///_pos|_r
                    bits<6> pre_c, ///_pre|_c
                    string asmstr, RegisterClass RC>{
  def _abs     : IABS_RO<abs1, abs2, asmstr, RC>;
  def _bo_bso  : IBO_RAbso<prefix1, bso2, asmstr, RC>;
  def _bo_pos  : IBO_RApos<prefix1, pos_r, asmstr, RC>;
  def _bo_pre  : IBO_RApre<prefix1, pre_c, asmstr, RC>;
  def _bo_r    : IBO_RPr<prefix2, pos_r, asmstr, RC>;
  def _bo_c    : IBO_RPc<prefix2, pre_c, asmstr, RC>;
}

multiclass mI_LD_2_<bits<8> slr, bits<8> slrp, bits<8> slro, bits<8> sro,
                    string asmstr, RegisterClass RC, string posfix="">{
  def _slr # posfix: ISLR<slr, asmstr, RC>;
  def _slr_post # posfix: ISLR_pos<slrp, asmstr, RC>;
  def _slro # posfix: ISLRO<slro, asmstr, RC>;
  if !eq(RC, RD) then def _sro # posfix: ISRO_D15RO<sro, asmstr, RA>;
  if !eq(RC, RA) then def _sro # posfix: ISRO_A15RO<sro, asmstr, RA>;
}

defm LD_A: mI_LD_<0x85, 0x02, 0x09, 0x29, 0x26, 0x06, 0x16, "ld.a", RA>;
defm LD_A: mI_LD_2_<0xB8, 0x64, 0x0C, 0x28, "ld.a", RA, "_v110">, NsRequires<[HasV110]>;
defm LD_A: mI_LD_2_<0xD4, 0xC4, 0xC8, 0xCC, "ld.a", RA>, Requires<[HasV120_UP]>;
def LD_A_bol : IBOL_RAaO<0x99, "ld.a", RA>;
def LD_A_sc  : ISC_A15A10C<0xD8, "ld.a">, Requires<[HasV120_UP]>;

defm LD_B: mI_LD_2_<0x98, 0x44, 0x34, 0x08, "ld.b", RD, "_v110">, NsRequires<[HasV110]>;
defm LD_B: mI_LD_<0x05, 0x00, 0x09, 0x29, 0x20, 0x00, 0x10,"ld.b", RD>;
def LD_B_bol : IBOL_RAaO<0x79, "ld.b", RD>, Requires<[HasV160_UP]>;

defm LD_BU: mI_LD_<0x05, 0x01, 0x09, 0x29, 0x21, 0x01, 0x11, "ld.bu", RD>;
defm LD_BU: mI_LD_2_<0x58, 0xC4, 0xB4, 0x88, "ld.bu", RD, "_v110">, NsRequires<[HasV110]>;
defm LD_BU: mI_LD_2_<0x14, 0x04, 0x08, 0x0C, "ld.bu", RD>, Requires<[HasV120_UP]>;
def LD_BU_bol : IBOL_RAaO<0x39, "ld.bu", RD>, Requires<[HasV160_UP]>;

defm LD_D  : mI_LD_<0x85, 0x01, 0x09, 0x29, 0x25, 0x05, 0x15, "ld.d", RE>;
defm LD_DA : mI_LD_<0x85, 0x03, 0x09, 0x29, 0x27, 0x07, 0x17, "ld.da", RP>;

defm LD_H  : mI_LD_<0x05, 0x02, 0x09, 0x29, 0x22, 0x02, 0x12, "ld.h", RD>;
defm LD_H: mI_LD_2_<0xD8, 0x24, 0x74, 0x48, "ld.h", RD, "_v110">, NsRequires<[HasV110]>;
defm LD_H: mI_LD_2_<0x94, 0x84, 0x88, 0x8C, "ld.h", RD>, Requires<[HasV120_UP]>;
def LD_H_bol : IBOL_RAaO<0xC9, "ld.h", RD>, Requires<[HasV160_UP]>;

defm LD_HU  : mI_LD_<0x05, 0x03, 0x09, 0x29, 0x23, 0x03, 0x13, "ld.hu", RD>;
def LD_HU_bol : IBOL_RAaO<0xB9, "ld.hu", RD>, Requires<[HasV160_UP]>;

defm LD_Q  : mI_LD_<0x45, 0x00, 0x09, 0x29, 0x28, 0x08, 0x18, "ld.q", RD>;

defm LD_W: mI_LD_<0x85, 0x00, 0x09, 0x29, 0x24, 0x04, 0x14, "ld.w", RD>;
defm LD_W: mI_LD_2_<0x38, 0xA4, 0xF4, 0xC8, "ld.w", RD, "_v110">, NsRequires<[HasV110]>;
defm LD_W: mI_LD_2_<0x54, 0x44, 0x48, 0x4C, "ld.w", RD>, Requires<[HasV120_UP]>;
def LD_W_bol : IBOL_RAaO<0x19, "ld.w", RD>;
def LD_W_sc  : ISC_D15A10C<0x58, "ld.w">, Requires<[HasV120_UP]>;


def LDLCX_abs    : IABS_off18<0x15, 0x02, "ldlcx">;
def LDLCX_bo_bso : IBO_bso<0x49, 0x24, "ldlcx">;

def LDMST_abs : IABS_OR<0xE5, 0x01, "ldmst", RE>;
defm LDMST    : mIBO_Ea<0x49, 0x21, 0x69, 0x01, 0x69, 0x11, 0x49, 0x01, 0x49, 0x11, "ldmst">;

def LDUCX_abs    : IABS_off18<0x15, 0x03, "lducx">;
def LDUCX_bo_bso : IBO_bso<0x49, 0x25, "lducx">;

def LEA_abs    : IABS_RO<0xC5, 0x00, "lea", RA>;
def LEA_bo_bso : IBO_RAbso<0x49, 0x28, "lea", RA>;
def LEA_bol    : IBOL_RAaO<0xD9, "lea", RA>;

def LHA_abs    : IABS_RO<0xC5, 0x01, "lha", RA>, Requires<[HasV162_UP]>;

def LOOP_brr : IBRR_1<0xFD, 0x00, "loop">;

def LOOP_sbr : SBR<0xFC, (outs), (ins RA:$s2, oext4imm:$disp4),
                   "loop $s2, $disp4", []>;
def LOOPU_brr : IBRR_0<0xFD, 0x01, "loopu">, Requires<[HasV120_UP]>;

defm LT : mIRR_RC<0x0B, 0x12, 0x8B, 0x12, "lt">;
defm LT : mISRR_SRC<0x7A, 0xFA, "lt", RD, RD, s4imm>;

defm LT_U : mIRR_RC<0x0B, 0x13, 0x8B, 0x13, "lt.u">;
defm LT_U : mISRR_SRC<0x06, 0x86, "lt.u", RD, RD, u4imm, "v110">, NsRequires<[HasV110]>;
def LT_A_rr : IRR_dab<0x01, 0x42, "lt.a", RD, RA, RA>;

multiclass mIU__RR_ab<bits<8> op1, bits<8> op2,
                      bits<8> uop1, bits<8> uop2,
                      string asmstr> {
  def ""  : IRR_dab<op1, op2, asmstr>;
  def U   : IRR_dab<uop1, uop2, asmstr # "u">;
}

defm LT_B : mIU__RR_ab<0x0B, 0x52, 0x0B, 0x53, "lt.b">;
defm LT_H : mIU__RR_ab<0x0B, 0x72, 0x0B, 0x73, "lt.h">;
defm LT_W : mIU__RR_ab<0x0B, 0x92, 0x0B, 0x93, "lt.w">;

class IRRR1_label<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC, string label>
    : RRR1<op1, op2, (outs RC:$d), (ins RD:$s1, RD:$s2, RC:$s3, u2imm:$n),
           asmstr # " $d, $s3, $s1, ${s2}" # label # ", $n", []>;
class IRRR1_label2<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC, string label1, string label2>
    : RRR1<op1, op2, (outs RC:$d), (ins RD:$s1, RD:$s2, RC:$s3, u2imm:$n),
           asmstr # " $d, $s3, ${s1}" # label1 # ", ${s2}" # label2 # ", $n", []>;
class IRRR1_n<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC=RD>
    : RRR1<op1, op2, (outs RC:$d), (ins RD:$s1, RD:$s2, RC:$s3, u2imm:$n),
           asmstr # " $d, $s3, $s1, $s2, $n", []>;
class IRRR1<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC=RD>
    : RRR1<op1, op2, (outs RC:$d), (ins RD:$s1, RD:$s2, RC:$s3, u2imm:$n),
           asmstr # " $d, $s3, $s1, $s2", []>;

class IRRR2<bits<8> op1, bits<8> op2, string asmstr, RegisterClass RC>
    : RRR2<op1, op2, (outs RC:$d), (ins RD:$s1, RD:$s2, RC:$s3),
           asmstr # " $d, $s3, $s1, $s2", []>;

multiclass mIRRR2<bits<8> op1, bits<8> op2, bits<8> op3, bits<8> op4, string asmstr>{
  def _rrr2   : IRRR2<op1, op2, asmstr, RD>;
  def _rrr2_e : IRRR2<op3, op4, asmstr, RE>, Requires<[HasV120_UP]>;
}

multiclass mIRCR_RRR2<bits<8> op_rcr1, bits<3> op_rcr2, bits<8> op_rrr21, bits<8> op_rrr22,
                      string asmstr, string posfix="",
                      Operand Type3=s9imm, RegisterClass RC1=RE, RegisterClass RC2=RD>{
  def _rcr#posfix: IRCR<op_rcr1, op_rcr2, asmstr, RC1, RC2, RC1, Type3>;
  def _rrr2#posfix: IRRR2<op_rrr21, op_rrr22, asmstr, RC1>;
}

multiclass mIRRR1_LU2<bits<8> prefix, bits<6> ll, bits<6> lu,
                      bits<6> ul, bits<6> uu,
                      string asmstr, RegisterClass RC>{
  def _rrr1_LL : IRRR1_label<prefix, ll, asmstr, RC, "ll">;
  def _rrr1_LU : IRRR1_label<prefix, lu, asmstr, RC, "lu">;
  def _rrr1_UL : IRRR1_label<prefix, ul, asmstr, RC, "ul">;
  def _rrr1_UU : IRRR1_label<prefix, uu, asmstr, RC, "uu">;
}
multiclass mI_MADD_H_MSUB_H_<bits<8> pre, bits<6> ll, bits<6> lu,
                       bits<6> ul, bits<6> uu, string asmstr, bit hasv110=true, RegisterClass RC=RE>{
  if hasv110 then {
      if !or(!eq("maddm.h", asmstr), !eq("msubm.h", asmstr)) then
        def _rrr1_v110 : IRRR1<pre, ul, asmstr, RC>, NsRequires<[HasV110]>;
      else
        def _rrr1_v110 : IRRR1_n<pre, ul, asmstr, RC>, NsRequires<[HasV110]>;
  }
  defm "" : mIRRR1_LU2<pre, ll, lu, ul, uu, asmstr, RC>, Requires<[HasV120_UP]>;
}

multiclass mI_MADDRsH_MSUBRsH_<bits<8> pre2, bits<6> ul2, bits<8> pre1, bits<6> ll, bits<6> lu,
                      bits<6> ul, bits<6> uu, string asmstr>{ 
  def _rrr1_v110: RRR1<pre2, ul2, (outs RD:$d), (ins RD:$s1, RD:$s2, RE:$s3, u2imm:$n),
                       asmstr # " $d, $s3, $s1, $s2, $n", []>, NsRequires<[HasV110]>;
  def _rrr1_UL_2: RRR1<pre2, ul2, (outs RD:$d), (ins RD:$s1, RD:$s2, RE:$s3, u2imm:$n),
                       asmstr # " $d, $s3, $s1, ${s2}ul, $n", []>, Requires<[HasV120_UP]>;
  defm "" : mIRRR1_LU2<pre1, ll, lu, ul, uu, asmstr, RD>, Requires<[HasV120_UP]>;
}

multiclass mI_MADDsQ_MSUBsQ_<bits<8> prefix, bits<6> op, bits<6> eop, bits<6> l, bits<6> el, bits<6> u, bits<6> eu,
                             bits<6> ll, bits<6> ell, bits<6> uu, bits<6> euu, string asmstr>{
  def _rrr1_UU2_v110: IRRR1_n<prefix, uu, asmstr, RD>, NsRequires<[HasV110]>;
  def _rrr1: IRRR1_n<prefix, op, asmstr, RD>, Requires<[HasV120_UP]>;
  def _rrr1_e: IRRR1_n<prefix, eop, asmstr, RE>, Requires<[HasV120_UP]>;
  def _rrr1_L: IRRR1_label<prefix, l, asmstr, RD, "l">, Requires<[HasV120_UP]>;
  def _rrr1_e_L: IRRR1_label<prefix, el, asmstr, RE, "l">, Requires<[HasV120_UP]>;
  def _rrr1_U: IRRR1_label<prefix, u, asmstr, RD, "u">, Requires<[HasV120_UP]>;
  def _rrr1_e_U: IRRR1_label<prefix, eu, asmstr, RE, "u">, Requires<[HasV120_UP]>;
  def _rrr1_L_L: IRRR1_label2<prefix, ll, asmstr, RD, "l", "l">, Requires<[HasV120_UP]>;
  def _rrr1_e_L_L: IRRR1_label2<prefix, ell, asmstr, RE, "l", "l">, Requires<[HasV120_UP]>;
  def _rrr1_U_U: IRRR1_label2<prefix, uu, asmstr, RD, "u", "u">, Requires<[HasV120_UP]>;
  def _rrr1_e_U_U: IRRR1_label2<prefix, euu, asmstr, RE, "u", "u">, Requires<[HasV120_UP]>;
}

defm MADD : mIRCR<0x13, 0x01, 0x13, 0x03, "madd">
          , mIRRR2<0x03, 0x0A, 0x03, 0x6A, "madd">;

defm MADDS : mIRCR<0x13, 0x05, 0x13, 0x07, "madds">
           , mIRRR2<0x03, 0x8A, 0x03, 0xEA, "madds">;

defm MADD_H  : mI_MADD_H_MSUB_H_<0x83, 0x1A, 0x19, 0x18, 0x1B, "madd.h">;
defm MADDS_H : mI_MADD_H_MSUB_H_<0x83, 0x3A, 0x39, 0x38, 0x3B, "madds.h">;

defm MADD_Q  : mI_MADDsQ_MSUBsQ_<0x43, 0x02, 0x1B, 0x01, 0x19, 0x00, 0x18, 0x05, 0x1D, 0x04, 0x1C, "madd.q">;
defm MADDS_Q : mI_MADDsQ_MSUBsQ_<0x43, 0x22, 0x3B, 0x21, 0x39, 0x20, 0x38, 0x25, 0x3D, 0x24, 0x3C, "madds.q">;

defm MADD_U: mIRCR_RRR2<0x13, 0x02, 0x03, 0x68, "madd.u", "", u9imm>, Requires<[HasV120_UP]>;

defm MADDS_U: mIRCR<0x13, 0x04, 0x13, 0x06, "madds.u">
             , mIRRR2<0x03, 0x88, 0x03, 0xE8, "madds.u">;

defm MADDM: mIRCR_RRR2<0x13, 0x03, 0x03, 0x6A, "maddm", "_v110">, NsRequires<[HasV110]>;
def MADDM_Q_rrr1_v110: IRRR1<0x43, 0x70, "maddm.q", RE>, NsRequires<[HasV110]>;
defm MADDM_U: mIRCR_RRR2<0x13, 0x02, 0x03, 0x68, "maddm.u", "_v110", u9imm>, NsRequires<[HasV110]>;

defm MADDM_H  : mI_MADD_H_MSUB_H_<0x83, 0x1E, 0x1D, 0x1C, 0x1F, "maddm.h">;

defm MADDMS: mIRCR_RRR2<0x13, 0x07, 0x03, 0xEA, "maddms", "_v110">, NsRequires<[HasV110]>;
defm MADDMS_U: mIRCR_RRR2<0x13, 0x06, 0x03, 0xE8, "maddms.u", "_v110", u9imm>, NsRequires<[HasV110]>;
defm MADDMS_H : mI_MADD_H_MSUB_H_<0x83, 0x3E, 0x3D, 0x3C, 0x3F, "maddms.h", false>;

defm MADDR_H  : mI_MADDRsH_MSUBRsH_<0x43, 0x1E, 0x83, 0x0E, 0x0D, 0x0C, 0x0F, "maddr.h">;
defm MADDRS_H : mI_MADDRsH_MSUBRsH_<0x43, 0x3E, 0x83, 0x2E, 0x2D, 0x2C, 0x2F, "maddrs.h">;

multiclass mI_MADDRsQ_MSUBRsQ_<bits<8> prefix, bits<6> op, bits<6> eop, string asmstr> {
  def _rrr1_L_L : IRRR1_label2<prefix, op, asmstr, RD, "l", "l">, Requires<[HasV120_UP]>;
  def _rrr1_U_U : IRRR1_label2<prefix, eop, asmstr, RD, "u", "u">, Requires<[HasV120_UP]>;
  def _rrr1_v110: IRRR1_n<prefix, eop, asmstr, RD>, NsRequires<[HasV110]>;
}

defm MADDR_Q : mI_MADDRsQ_MSUBRsQ_<0x43, 0x07, 0x06, "maddr.q">;
defm MADDRS_Q: mI_MADDRsQ_MSUBRsQ_<0x43, 0x27, 0x26, "maddrs.q">;

defm MADDSU_H   : mI_MADD_H_MSUB_H_<0xC3, 0x1A, 0x19, 0x18, 0x1B, "maddsu.h", false>;
defm MADDSUS_H  : mI_MADD_H_MSUB_H_<0xC3, 0x3A, 0x39, 0x38, 0x3B, "maddsus.h", false>;
defm MADDSUM_H  : mI_MADD_H_MSUB_H_<0xC3, 0x1E, 0x1D, 0x1C, 0x1F, "maddsum.h", false>;
defm MADDSUMS_H : mI_MADD_H_MSUB_H_<0xC3, 0x3E, 0x3D, 0x3C, 0x3F, "maddsums.h", false>;
defm MADDSUR_H  : mI_MADD_H_MSUB_H_<0xC3, 0x0E, 0x0D, 0x0C, 0x0F, "maddsur.h", false, RD>;
defm MADDSURS_H : mI_MADD_H_MSUB_H_<0xC3, 0x2E, 0x2D, 0x2C, 0x2F, "maddsurs.h", false, RD>;

defm MAX   : mIRR_RC<0x0B, 0x1A, 0x8B, 0x1A, "max">;
defm MAX_U : mIRR_RC<0x0B, 0x1B, 0x8B, 0x1B, "max.u">;

defm MAX_B : mIU__RR_ab<0x0B, 0x5A, 0x0B, 0x5B, "max.b">;
defm MAX_H : mIU__RR_ab<0x0B, 0x7A, 0x0B, 0x7B, "max.h">;

defm MIN   : mIRR_RC<0x0B, 0x18, 0x8B, 0x18, "min">;
defm MIN_U : mIRR_RC<0x0B, 0x19, 0x8B, 0x19, "min.u">;

defm MIN_B : mIU__RR_ab<0x0B, 0x58, 0x0B, 0x59, "min.b">;
defm MIN_H : mIU__RR_ab<0x0B, 0x78, 0x0B, 0x79, "min.h">;

class IRLC_1<bits<8> op1, string asmstr, RegisterClass RC=RD, Operand TypeC=u16imm>
    : RLC<op1, (outs RC:$d), (ins TypeC:$const16),
      asmstr # " $d, $const16", []>;

class ISRC_1<bits<8> op1, string asmstr, RegisterClass RC=RD> 
    : SRC<op1, (outs RC:$d), (ins s4imm:$const4),
      asmstr # " $d, $const4", []>;

def MOV_rlc  : IRLC_1<0x3B, "mov", RD, s16imm>;
def MOV_rlc_e: IRLC_1<0xFB, "mov", RE>, Requires<[HasV160_UP]>;

def MOV_rr  : IRR_b<0x0B, 0x1F, "mov">;
def MOV_rr_e: IRR_b<0x0B, 0x80, "mov", RE>, Requires<[HasV160_UP]>;
def MOV_rr_eab : IRR_dab<0x0B, 0x81, "mov", RE>, Requires<[HasV160_UP]>;

def MOV_sc_v110: ISC_D15C<0xC6, "mov">, NsRequires<[HasV110]>;
def MOV_sc : ISC_D15C<0xDA, "mov">, Requires<[HasV120_UP]>;

def MOV_src: ISRC_dC<0x82, "mov">;
def MOV_src_e: ISRC_1<0xD2, "mov", RE>, Requires<[HasV160_UP]>;

def MOV_srr  : ISRR_db<0x02, "mov">;

multiclass mI_MOV_srr<bits<8> srr110,bits<8> srr1, string asmstr, RegisterClass RCd=RA, RegisterClass RC1=RD>{
  def _srr_v110: ISRR_db<srr110, asmstr, RCd, RC1>, NsRequires<[HasV110]>;
  def _srr: ISRR_db<srr1, asmstr, RCd, RC1>, Requires<[HasV120_UP]>;
}

multiclass mI_MOVA_<bits<8> rr1, bits<8> rr2, bits<8> src1, bits<8> srr110,bits<8> srr1, string asmstr> {
  def _rr : IRR_b<rr1, rr2, asmstr, RA>;
  def _src: ISRC_dC<src1, asmstr, RA, u4imm>, Requires<[HasV120_UP]>;
  defm "" : mI_MOV_srr<srr110, srr1, asmstr>;
}

defm MOV_A : mI_MOVA_<0x01, 0x63, 0xA0, 0x30, 0x60, "mov.a">;

def MOV_AA_rr : IRR_b<0x01, 0x00, "mov.aa", RA, RA>;
defm MOV_AA_srr: mI_MOV_srr<0x80, 0x40, "mov.aa", RA, RA>;

def MOV_D_rr : IRR_b<0x01, 0x4C, "mov.d", RD, RA>;
defm MOV_D_srr : mI_MOV_srr<0x20, 0x80, "mov.d", RD, RA>;

def MOV_U_rlc  : IRLC_1<0xBB, "mov.u">;
def MOVH_rlc   : IRLC_1<0x7B, "movh", RD, u16imm>;
def MOVH_A_rlc : IRLC_1<0x91, "movh.a", RA, u16imm>;
def MOVZ_A_sr: ISR_1<0x00, 0x01, "movz.a", RA>, NsRequires<[HasV110]>;

defm MSUB : mIRCR<0x33, 0x01, 0x33, 0x03, "msub">
          , mIRRR2<0x23, 0x0A, 0x23, 0x6A, "msub">;
defm MSUBS: mIRCR<0x33, 0x05, 0x33, 0x07, "msubs">
          , mIRRR2<0x23, 0x8A, 0x23, 0xEA, "msubs">;

defm MSUB_H  : mI_MADD_H_MSUB_H_<0xA3, 0x1A, 0x19, 0x18, 0x1B, "msub.h">;
defm MSUBS_H : mI_MADD_H_MSUB_H_<0xA3, 0x3A, 0x39, 0x38, 0x3B, "msubs.h">;
defm MSUB_Q  : mI_MADDsQ_MSUBsQ_<0x63, 0x02, 0x1B, 0x01, 0x19, 0x00, 0x18, 0x05, 0x1D, 0x04, 0x1C, "msub.q">;
defm MSUBS_Q : mI_MADDsQ_MSUBsQ_<0x63, 0x22, 0x3B, 0x21, 0x39, 0x20, 0x38, 0x25, 0x3D, 0x24, 0x3C, "msubs.q">;

defm MSUB_U: mIRCR_RRR2<0x33, 0x02, 0x23, 0x68, "msub.u", "", u9imm>, Requires<[HasV120_UP]>;
defm MSUBS_U : mIRCR<0x33, 0x04, 0x33, 0x06, "msubs.u">
             , mIRRR2<0x23, 0x88, 0x23, 0xE8, "msubs.u">;

defm MSUBAD_H   : mI_MADD_H_MSUB_H_<0xE3, 0x1A, 0x19, 0x18, 0x1B, "msubad.h", false>;
defm MSUBADS_H  : mI_MADD_H_MSUB_H_<0xE3, 0x3A, 0x39, 0x38, 0x3B, "msubads.h", false>;
defm MSUBADM_H  : mI_MADD_H_MSUB_H_<0xE3, 0x1E, 0x1D, 0x1C, 0x1F, "msubadm.h", false>;
defm MSUBADMS_H : mI_MADD_H_MSUB_H_<0xE3, 0x3E, 0x3D, 0x3C, 0x3F, "msubadms.h", false>;
defm MSUBADR_H  : mI_MADD_H_MSUB_H_<0xE3, 0x0E, 0x0D, 0x0C, 0x0F, "msubadr.h", true, RD>;
defm MSUBADRS_H : mI_MADD_H_MSUB_H_<0xE3, 0x2E, 0x2D, 0x2C, 0x2F, "msubadrs.h", true, RD>;

defm MSUBM: mIRCR_RRR2<0x33, 0x03, 0x23, 0x6A, "msubm", "v110">, NsRequires<[HasV110]>;
def  MSUBM_Q_rrr1_v110: IRRR1<0x63, 0x1C, "msubm.q", RE>, NsRequires<[HasV110]>;
defm MSUBM_U: mIRCR_RRR2<0x33, 0x02, 0x23, 0x68, "msubm.u", "v110">, NsRequires<[HasV110]>;
defm MSUBMS: mIRCR_RRR2<0x33, 0x07, 0x23, 0xEA, "msubms", "v110">, NsRequires<[HasV110]>;
defm MSUBMS_U: mIRCR_RRR2<0x33, 0x06, 0x23, 0xE8, "msubms.u", "v110">, NsRequires<[HasV110]>;

defm MSUBM_H    : mI_MADD_H_MSUB_H_<0xA3, 0x1E, 0x1D, 0x1C, 0x1F, "msubm.h">;
defm MSUBMS_H   : mI_MADD_H_MSUB_H_<0xA3, 0x3E, 0x3D, 0x3C, 0x3F, "msubms.h", false>;

defm MSUBR_H : mI_MADDRsH_MSUBRsH_<0x63, 0x1E, 0xA3, 0x0E, 0x0D, 0x0C, 0x0F, "msubr.h">;
defm MSUBRS_H: mI_MADDRsH_MSUBRsH_<0x63, 0x3E, 0xA3, 0x2E, 0x2D, 0x2C, 0x2F, "msubrs.h">;

defm MSUBR_Q : mI_MADDRsQ_MSUBRsQ_<0x63, 0x07, 0x06, "msubr.q">;
defm MSUBRS_Q: mI_MADDRsQ_MSUBRsQ_<0x63, 0x27, 0x26, "msubrs.q">;

class IRLC_CR<bits<8> op1, string asmstr, RegisterClass RC=RD>
    : RLC<op1, (outs), (ins s16imm:$const16, RC:$d),
          asmstr # " $const16, $d", []>;

def MTCR_rlc : IRLC_CR<0xCD, "mtcr">;
def MFCR_rlc : IRLC_1 <0x4D, "mfcr">;

class IRR2<bits<8> op1, bits<12> op2, string asmstr,
           RegisterClass RCd=RD, RegisterClass RCa=RD, RegisterClass RCb=RD>
    : RR2<op1, op2, (outs RCd:$d), (ins RCa:$s1, RCb:$s2), asmstr # " $d, $s1, $s2", []>;

def MUL_rc  : RC<0x53, 0x01, (outs RD:$d), (ins RD:$s1, s9imm:$const9),
                "mul $d, $s1, $const9", []>;
def MUL_rc_e: RC<0x53, 0x03, (outs RE:$d), (ins RD:$s1, s9imm:$const9),
                "mul $d, $s1, $const9", []>
            , Requires<[HasV120_UP]>;

def MUL_rr2  : IRR2<0x73, 0x0A, "mul">, Requires<[HasV120_UP]>;
def MUL_rr2_e: IRR2<0x73, 0x6A, "mul", RE>, Requires<[HasV120_UP]>;

def MUL_srr : ISRR_db<0xE2, "mul">;
def MUL_rr_v110: IRR_dab<0x73, 0x0A, "mul">, NsRequires<[HasV110]>;

multiclass mI_MUL_<bits<8> rc1, bits<7> rc2, bits<8> oprr1, bits<12> oprr2, string asmstr,
                   RegisterClass RCd=RD>{
  if !eq(asmstr, "mul.u") then
      def _rc  : IRC<rc1, rc2, asmstr, RCd>, Requires<[HasV120_UP]>;
  else{
      def _rc  : IRC<rc1, rc2, asmstr, RCd>;
      def _rr_v110: IRR_dab<oprr1, oprr2{7-0}, asmstr, RCd>, NsRequires<[HasV110]>;
  }

  def _rr2 : IRR2<oprr1, oprr2, asmstr, RCd>, Requires<[HasV120_UP]>;
}

defm MULS : mI_MUL_<0x53, 0x05, 0x73, 0x8A, "muls", RD>;

class IRR1<bits<8> op1, bits<10> op2, string asmstr,
           RegisterClass RCd, string labela, string labelb>
    : RR1<op1, op2, (outs RCd:$d), (ins RD:$s1, RD:$s2, u2imm:$n),
          asmstr # " $d, ${s1}" # labela # ", ${s2}" # labelb # ", $n", []>;

multiclass mI_MUL_H_<bits<8> pre, bits<10> ll, bits<10> lu, bits<10> ul, bits<10> uu, string asmstr
                   , bit hasv110=false, bits<8> rr=0, RegisterClass RCd=RE>{
  if hasv110 then
    def _rr_v110 : IRR_dabn<pre, rr, asmstr, RCd>, NsRequires<[HasV110]>;
  def _rr1_LL2e : IRR1<pre, ll, asmstr, RCd, "", "ll">, Requires<[HasV120_UP]>;
  def _rr1_LU2e : IRR1<pre, lu, asmstr, RCd, "", "lu">, Requires<[HasV120_UP]>;
  def _rr1_UL2e : IRR1<pre, ul, asmstr, RCd, "", "ul">, Requires<[HasV120_UP]>;
  def _rr1_UU2e : IRR1<pre, uu, asmstr, RCd, "", "uu">, Requires<[HasV120_UP]>;
}

defm MUL_H : mI_MUL_H_<0xB3, 0x1A, 0x19, 0x18, 0x1B, "mul.h", true, 0x18>;

multiclass mI_MULQ_<bits<8> pre, bits<8> rr, bits<10> op1, bits<10> op2, bits<10> op3, bits<10> op4,
                      bits<10> op5, bits<10> op6, bits<10> op7, bits<10> op8, string asmstr>{
  def _rr_v110 : IRR_dabn<pre, rr, asmstr, RD>, NsRequires<[HasV110]>;
  def _rr1_2   : IRR1<pre, op1, asmstr, RD, "", "">, Requires<[HasV120_UP]>;
  def _rr1_2__e: IRR1<pre, op2, asmstr, RE, "", "">, Requires<[HasV120_UP]>;

  def _rr1_2_L : IRR1<pre, op3, asmstr, RD, "", "l">, Requires<[HasV120_UP]>;
  def _rr1_2_Le: IRR1<pre, op4, asmstr, RE, "", "l">, Requires<[HasV120_UP]>;
  def _rr1_2_U : IRR1<pre, op5, asmstr, RD, "", "u">, Requires<[HasV120_UP]>;
  def _rr1_2_Ue: IRR1<pre, op6, asmstr, RE, "", "u">, Requires<[HasV120_UP]>;

  def _rr1_2LL : IRR1<pre, op7, asmstr, RD, "l", "l">, Requires<[HasV120_UP]>;
  def _rr1_2UU : IRR1<pre, op8, asmstr, RD, "u", "u">, Requires<[HasV120_UP]>;
}

defm MUL_Q : mI_MULQ_<0x93, 0x04, 0x02, 0x1B, 0x01, 0x19, 0x00, 0x18, 0x05, 0x04, "mul.q">;

defm MUL_U : mI_MUL_<0x53, 0x02, 0x73, 0x68, "mul.u", RE>;
defm MULS_U : mI_MUL_<0x53, 0x04, 0x73, 0x88, "muls.u", RD>;

defm MULM: mIRR_RC<0x73, 0x6A, 0x53, 0x03, "mulm", RE>, NsRequires<[HasV110]>;
defm MULM_U: mIRR_RC<0x73, 0x68, 0x53, 0x02, "mulm.u", RE>, NsRequires<[HasV110]>;
defm MULMS_H: mI_MUL_H_<0xB3, 0x3E,0x3D, 0x3C, 0x3F, "mulms.h">;

defm MULM_H : mI_MUL_H_<0xB3, 0x1E, 0x1D, 0x1C, 0x1F, "mulm.h">;
defm MULR_H : mI_MUL_H_<0xB3, 0x0E, 0x0D, 0x0C, 0x0F, "mulr.h", true, 0x0C, RD>;

def MULR_Q_rr_v110 : IRR_dabn<0x93, 0x06, "mulr.q">, NsRequires<[HasV110]>;
def MULR_Q_rr1_2LL : IRR1<0x93, 0x07, "mulr.q", RD, "l", "l">, Requires<[HasV120_UP]>;
def MULR_Q_rr1_2UU : IRR1<0x93, 0x06, "mulr.q", RD, "u", "u">, Requires<[HasV120_UP]>;

defm NAND : mIRR_RC<0x0F, 0x09, 0x8F, 0x09, "nand">;
def NAND_T : IBIT<0x07, 0x00, "nand.t">;

defm NE : mIRR_RC<0x0B, 0x11, 0x8B, 0x11, "ne">;
def NE_A : IRR_dab<0x01, 0x41, "ne.a", RD, RA, RA>;
def NEZ_A : IRR_a<0x01, 0x49, "nez.a", RD, RA>;

def NOP_sr : ISR_0<0x00, 0x00, "nop">;
def NOP_sys : ISYS_0<0x0D, 0x00, "nop">;

multiclass mISR_1<bits<8> sr1op1, bits<4> sr1op2, bits<8> sr2op1, bits<4> sr2op2,
                string asmstr>{
  def _sr : ISR_1<sr1op1, sr1op2, asmstr>, Requires<[HasV120_UP]>;
  def _sr_v110 : ISR_1<sr2op1, sr2op2, asmstr>, NsRequires<[HasV110]>;
}

defm NOR : mIRR_RC<0x0F, 0x0B, 0x8F, 0x0B, "nor">;
def NOR_T : IBIT<0x87, 0x02, "nor.t">;

defm NOR : mISR_1<0x46, 0x00, 0x36, 0x00, "nor">;

def NOT_sr_v162 : ISR_1<0x46, 0x00, "not">, NsRequires<[HasV162]>;


defm OR : mIRR_RC<0x0F, 0x0A, 0x8F, 0x0A, "or", RD, RD, u9imm>;
def OR_sc : ISC_D15C<0x96, "or">, Requires<[HasV120_UP]>;
def OR_srr : ISRR_db<0xA6, "or">, Requires<[HasV120_UP]>;
def OR_sc_v110 : ISC_D15C<0xD6, "or">, NsRequires<[HasV110]>;
def OR_srr_v110 : ISRR_db<0x56, "or">, NsRequires<[HasV110]>;

def OR_AND_T  : IBIT<0xC7, 0x00, "or.and.t">;
def OR_ANDN_T : IBIT<0xC7, 0x03, "or.andn.t">;
def OR_NOR_T  : IBIT<0xC7, 0x02, "or.nor.t">;
def OR_OR_T   : IBIT<0xC7, 0x01, "or.or.t">;

defm OR_EQ   : mIRR_RC<0x0B, 0x27, 0x8B, 0x27, "or.eq">;
defm OR_GE   : mIRR_RC<0x0B, 0x2B, 0x8B, 0x2B, "or.ge">;
defm OR_GE_U : mIRR_RC<0x0B, 0x2C, 0x8B, 0x2C, "or.ge.u">;
defm OR_LT   : mIRR_RC<0x0B, 0x29, 0x8B, 0x29, "or.lt">;
defm OR_LT_U : mIRR_RC<0x0B, 0x2A, 0x8B, 0x2A, "or.lt.u">;
defm OR_NE   : mIRR_RC<0x0B, 0x28, 0x8B, 0x28, "or.ne">;

def OR_T : IBIT<0x87, 0x01, "or.t">;

defm ORN : mIRR_RC<0x0F, 0x0F, 0x8F, 0x0F, "orn">;

def ORN_T : IBIT<0x07, 0x01, "orn.t">;

def PACK_rrr : IRRR_d31<0x6B, 0x00, "pack", RD, RD, RD, RE>;

multiclass mISYS_0<bits<8> sys1op1, bits<6> sys1op2, bits<8> sys2op1, bits<6> sys2op2,
                string asmstr>{
  def _sys : ISYS_0<sys1op1, sys1op2, asmstr>, Requires<[HasV120_UP]>;
  def _sys_v110 : ISYS_0<sys2op1, sys2op2, asmstr>, NsRequires<[HasV110]>;
}

def PARITY_rr : IRR_a<0x4B, 0x02, "parity">, Requires<[HasV120_UP]>;
def PARITY_rr_v110 : IRR_a<0x4B, 0x08, "parity">, NsRequires<[HasV110]>;

def POPCNT_W_rr : IRR_a<0x4B, 0x22, "popcnt.w">, NsRequires<[HasV162]>;

def RESTORE_sys : ISYS_1<0x0D, 0x0E, "restore">, Requires<[HasV160_UP]>;

def RET_sr : ISR_0<0x00, 0x09, "ret">;
defm RET : mISYS_0<0x0D, 0x06, 0x0D, 0x05, "ret">;

def RFE_sr : ISR_0<0x00, 0x08, "rfe">;
defm RFE_sys : mISYS_0<0x0D, 0x07, 0x0D, 0x06, "rfe">;

def RFM_sys : ISYS_0<0x0D, 0x05, "rfm">;

def RSLCX_sys : ISYS_0<0x0D, 0x09, "rslcx">;

def RSTV_sys : ISYS_0<0x2F, 0x00, "rstv">;

def RSUB_rc : IRC<0x8B, 0x08, "rsub">;
defm RSUB_sr : mISR_1<0x32, 0x05, 0xD2, 0x05, "rsub">;

def RSUBS_rc   : IRC<0x8B, 0x0A, "rsubs">;
def RSUBS_U_rc : IRC<0x8B, 0x0B, "rsubs.u">;

multiclass mI_SAT_<bits<8> r1, bits<8> r2, bits<8> s1, bits<4> s2, bits<8> vs1, bits<4> vs2, string asmstr>{
  def _rr : IRR_a<r1, r2, asmstr>;
  defm "" : mISR_1<s1, s2, vs1, vs2, asmstr>;
}

defm SAT_B  : mI_SAT_<0x0B, 0x5E, 0x32, 0x00, 0xD2, 0x00, "sat.b">;
defm SAT_BU : mI_SAT_<0x0B, 0x5F, 0x32, 0x01, 0xD2, 0x01, "sat.bu">;
defm SAT_H  : mI_SAT_<0x0B, 0x7E, 0x32, 0x02, 0xD2, 0x02, "sat.h">;
defm SAT_HU : mI_SAT_<0x0B, 0x7F, 0x32, 0x03, 0xD2, 0x03, "sat.hu">;

def SEL_rcr : IRCR<0xAB, 0x04, "sel">;
def SEL_rrr : IRRR<0x2B, 0x04, "sel">;

def SEL_A_rcr_v110: IRCR<0xA1, 0x04, "sel.a", RA, RA>, NsRequires<[HasV110]>;
def SEL_A_rrr_v110: IRRR<0x21, 0x04, "sel.a", RA, RA, RA>, NsRequires<[HasV110]>;

def SELN_rcr : IRCR<0xAB, 0x05, "seln">;
def SELN_rrr : IRRR<0x2B, 0x05, "seln">;

def SELN_A_rcr_v110: IRCR<0xA1, 0x05, "seln.a", RA, RA>, NsRequires<[HasV110]>;
def SELN_A_rrr_v110: IRRR<0x21, 0x05, "seln.a", RA, RA, RA>, NsRequires<[HasV110]>;

multiclass mISRC_1<bits<8> op1, bits<8> op2, string asmstr>{
  def _src: ISRC_1<op1, asmstr>, Requires<[HasV120_UP]>;
  def _src_v110: ISRC_1<op2, asmstr>, NsRequires<[HasV110]>;
}

defm SH : mISRC_1<0x06, 0x26, "sh">;
defm SH : mIRR_RC<0x0F, 0x00, 0x8F, 0x00, "sh">;
defm SH_B : mIRR_RC<0x0F, 0x20, 0x8F, 0x20, "sh.b">, NsRequires<[HasV110]>;
defm SH_H : mIRR_RC<0x0F, 0x40, 0x8F, 0x40, "sh.h">;

defm SH_EQ : mIRR_RC<0x0B, 0x37, 0x8B, 0x37, "sh.eq">;
defm SH_NE : mIRR_RC<0x0B, 0x38, 0x8B, 0x38, "sh.ne">;
defm SH_GE : mIRR_RC<0x0B, 0x3B, 0x8B, 0x3B, "sh.ge">;
defm SH_GE_U : mIRR_RC<0x0B, 0x3C, 0x8B, 0x3C, "sh.ge.u">;
defm SH_LT : mIRR_RC<0x0B, 0x39, 0x8B, 0x39, "sh.lt">;
defm SH_LT_U : mIRR_RC<0x0B, 0x3A, 0x8B, 0x3A, "sh.lt.u">;

def SH_AND_T  : IBIT<0x27, 0x00, "sh.and.t">;
def SH_ANDN_T : IBIT<0x27, 0x03, "sh.andn.t">;
def SH_NAND_T : IBIT<0xA7, 0x00, "sh.nand.t">;
def SH_NOR_T  : IBIT<0x27, 0x02, "sh.nor.t">;
def SH_OR_T   : IBIT<0x27, 0x01, "sh.or.t">;
def SH_ORN_T  : IBIT<0xA7, 0x01, "sh.orn.t">;
def SH_XNOR_T : IBIT<0xA7, 0x02, "sh.xnor.t">;
def SH_XOR_T  : IBIT<0xA7, 0x03, "sh.xor.t">;


defm SHA : mISRC_1<0x86, 0xA6, "sha">;
defm SHA : mIRR_RC<0x0F, 0x01, 0x8F, 0x01, "sha">;
defm SHA_B : mIRR_RC<0x0F, 0x21, 0x8F, 0x21, "sha.b">, NsRequires<[HasV110]>;
defm SHA_H : mIRR_RC<0x0F, 0x41, 0x8F, 0x41, "sha.h">;
defm SHAS : mIRR_RC<0x0F, 0x02, 0x8F, 0x02, "shas">;

def SHUFFLE_rc : IRC<0x8F, 0x07, "shuffle">, Requires<[HasV162]>;

// A[b], off10, A[a] (BO)(Base + Short Offset Addressing Mode)
class IBO_bso_st<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC> 
    : BO<op1, op2, (outs RC:$d), (ins RA:$s1, s10imm:$off10),
      asmstr # " [$s1]$off10, $d", []>;
// P[b], A[a] (BO)(Bit-reverse Addressing Mode)
class IBO_r_st<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC>
    : BO<op1, op2, (outs RP:$d), (ins RC:$s1),
      asmstr # " [${d}+r], $s1", []>;
// P[b], off10, A[a] (BO)(Circular Addressing Mode)
class IBO_c_st<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC> 
    : BO<op1, op2, (outs RP:$d), (ins RC:$s1, s10imm:$off10),
      asmstr # " [${d}+c]$off10, $s1", []>;
// A[b], off10, A[a] (BO)(Post-increment Addressing Mode)
class IBO_pos_st<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC>
    : BO<op1, op2, (outs), (ins RC:$s1, RA:$s2, s10imm:$off10),
      asmstr # " [${s2}+]$off10, $s1", []>;
// A[b], off10, A[a] (BO)(Pre-increment Addressing Mode)
class IBO_pre_st<bits<8> op1, bits<6> op2, string asmstr, RegisterClass RC>
    : BO<op1, op2, (outs), (ins RC:$s1, RA:$s2, s10imm:$off10),
      asmstr # " [+${s2}]$off10, $s1", []>;


multiclass mIBO_st<bits<8> prefix1, bits<8> prefix2,
                  bits<6> bso2, ///_bso
                  bits<6> pos_r, ///_pos|_r
                  bits<6> pre_c, ///_pre|_c
                  string asmstr, RegisterClass RC>{
  def _bo_bso  : IBO_bso_st<prefix1, bso2, asmstr, RC>;
  def _bo_pos  : IBO_pos_st<prefix1, pos_r, asmstr, RC>;
  def _bo_pre  : IBO_pre_st<prefix1, pre_c, asmstr, RC>;
  def _bo_r    : IBO_r_st<prefix2, pos_r, asmstr, RC>;
  def _bo_c    : IBO_c_st<prefix2, pre_c, asmstr, RC>;
}

multiclass mI_ST_<bits<8> abs1, bits<2> abs2,   ///_abs
                    bits<8> prefix1, bits<8> prefix2,
                    bits<6> bso, ///_bso
                    bits<6> pos_r, ///_pos|_r
                    bits<6> pre_c, ///_pre|_c
                    string asmstr, RegisterClass RC>
          : mIBO_st<prefix1, prefix2, bso, pos_r, pre_c, asmstr, RC>{
  def _abs : IABS_OR<abs1, abs2, asmstr, RC>;
}

defm ST_A  : mI_ST_<0xA5, 0x02, 0x89, 0xA9, 0x26, 0x06, 0x16, "st.a", RA>;
defm ST_B  : mI_ST_<0x25, 0x00, 0x89, 0xA9, 0x20, 0x00, 0x10, "st.b", RD>;
defm ST_D  : mI_ST_<0xA5, 0x01, 0x89, 0xA9, 0x25, 0x05, 0x15, "st.d", RE>;
defm ST_DA : mI_ST_<0xA5, 0x03, 0x89, 0xA9, 0x27, 0x07, 0x17, "st.da", RP>;
defm ST_H  : mI_ST_<0x25, 0x02, 0x89, 0xA9, 0x22, 0x02, 0x12, "st.h", RD>;
defm ST_Q  : mI_ST_<0x65, 0x00, 0x89, 0xA9, 0x28, 0x08, 0x18, "st.q", RD>;

multiclass mI_ST_2_<bits<8> sro, bits<8> ssr, bits<8> ssrpos, bits<8> ssro,
                             bits<8> srov, bits<8> ssrv, bits<8> ssrposv, bits<8> ssrov,
                             string asmstr,  RegisterClass RC>{
  if !eq(RC,RD) then {
    def _sro_v110: ISRO_ROD15<srov, asmstr, RA>, NsRequires<[HasV110]>;
    def _sro  : ISRO_ROD15<sro, asmstr, RA>, Requires<[HasV120_UP]>;
  } else if !eq(RC,RA) then {
    def _sro_v110: ISRO_ROA15<srov, asmstr, RA>, NsRequires<[HasV110]>;
    def _sro  : ISRO_ROA15<sro, asmstr, RA>, Requires<[HasV120_UP]>;
  }
  def _ssr_v110    : SSR<ssrv, (outs RA:$d), (ins RC:$s1),
                        asmstr # " [$d], $s1", []>
                  , NsRequires<[HasV110]>;
  def _ssr_pos_v110: SSR<ssrposv, (outs RA:$d), (ins RC:$s1),
                        asmstr # " [${d}+], $s1", []>
                  , NsRequires<[HasV110]>;
  def _ssro_v110: SSRO<ssrov, (outs), (ins RC:$s1, u4imm:$off4),
                      asmstr # " [a15]$off4, $s1", []>
                , NsRequires<[HasV110]>;

  def _ssr     : SSR<ssr, (outs RA:$d), (ins RC:$s1),
                     asmstr # " [$d], $s1", []>
              , Requires<[HasV120_UP]>;
  def _ssr_pos : SSR<ssrpos, (outs RA:$d), (ins RC:$s1),
                     asmstr # " [${d}+], $s1", []>
              , Requires<[HasV120_UP]>;
  def _ssro    : SSRO<ssro, (outs), (ins RC:$s1, u4imm:$off4),
                      asmstr # " [a15]$off4, $s1", []>
              , Requires<[HasV120_UP]>;
}

def ST_A_bol : IBOL_AbOR<0xB5, "st.a", RA>, Requires<[HasV160_UP]>;
def ST_A_sc : ISC_A10CA15<0xF8, "st.a">, Requires<[HasV120_UP]>;
defm ST_A : mI_ST_2_<0xEC, 0xF4, 0xE4, 0xE8, 0x18, 0x84, 0x54, 0x2C, "st.a", RA>;

def ST_B_bol : IBOL_AbOR<0xE9, "st.b", RD>, Requires<[HasV160_UP]>;
defm ST_B : mI_ST_2_<0x2C, 0x34, 0x24, 0x28, 0xA8, 0x78, 0xE4, 0x8C, "st.b", RD>;

def ST_H_bol : IBOL_AbOR<0xF9, "st.h", RD>, Requires<[HasV160_UP]>;
defm ST_H : mI_ST_2_<0xAC, 0xB4, 0xA4, 0xA8, 0x68, 0xF8, 0x14, 0x4C, "st.h", RD>;

def ST_T : ABSB<0xD5, 0x00, (outs), (ins off18imm:$off18, i32imm:$bpos3, i32imm:$b),
                "st.t $off18, $bpos3, $b", []>;

defm ST_W : mI_ST_<0xA5, 0x00, 0x89, 0xA9, 0x24, 0x04, 0x14, "st.w", RD>
          , mI_ST_2_<0x6C, 0x74, 0x64, 0x68, 0xE8, 0x04, 0x94, 0xCC, "st.w", RD>;
def ST_W_bol : IBOL_AbOR<0x59, "st.w", RD>;
def ST_W_sc : ISC_A10CD15<0x78, "st.w">, Requires<[HasV120_UP]>;

def STLCX_abs : IABS_off18<0x15, 0x00, "stlcx">; 
def STLCX_bo_bso : IBO_bso<0x49, 0x26, "stlcx">; 

def STUCX_abs : IABS_off18<0x15, 0x01, "stucx">; 
def STUCX_bo_bso : IBO_bso<0x49, 0x27, "stucx">; 

def SUB_rr : IRR_dab<0x0B, 0x08, "sub">;
defm SUB : mISRR_a15a<0xA2, 0x52, 0x5A, "sub">
         , mIB_H<0x0B, 0x48, 0x0B, 0x68, "sub">;


multiclass mISC_A10C<bits<8> scv, bits<8> sc, string asmstr>{
  def _sc_v110: ISC_A10C<scv, asmstr>, NsRequires<[HasV110]>;
  def _sc     : ISC_A10C<sc, asmstr>, Requires<[HasV120_UP]>;
}

def SUB_A_rr : IRR_dab<0x01, 0x02, "sub.a", RA, RA, RA>;
defm SUB_A : mISC_A10C<0x40, 0x20, "sub.a">;
def SUBSC_A_rr: IRR_dabn<0x01, 0x61, "subsc.a", RA, RA, RD>, NsRequires<[HasV110]>;

def SUBC_rr : IRR_dab<0x0B, 0x0D, "subc">;

def SUBS_rr : IRR_dab<0x0B, 0x0A, "subs">;
def SUBS_srr : ISRR_db<0x62, "subs">;

def SUBS_U_rr : IRR_dab<0x0B, 0x0B, "subs.u">;
def SUBS_B_rr : IRR_dab<0x0B, 0x4A, "subs.b">, NsRequires<[HasV110]>;
def SUBS_BU_rr: IRR_dab<0x0B, 0x4B, "subs.bu">, NsRequires<[HasV110]>;
def SUBS_H_rr : IRR_dab<0x0B, 0x6A, "subs.h">;
def SUBS_HU_rr: IRR_dab<0x0B, 0x6B, "subs.hu">;
def SUBX_rr : IRR_dab<0x0B, 0x0C, "subx">;

def SVLCX_sys : ISYS_0<0x0D, 0x08, "svlcx">;

multiclass mI_SWAP_1<bits<8> prefix_bso_pos_pre, bits<8> prefix_r_c,
                    bits<6> bso, ///_bso
                    bits<6> pos_r, ///_pos|_r
                    bits<6> pre_c, ///_pre|_c
                    string asmstr, RegisterClass RC=RA>{
  def _bo_bso: BO<prefix_bso_pos_pre, bso, (outs RC:$d), (ins RA:$s1, s10imm:$off10),
                  asmstr # " [$s1]$off10, $d", []>;
  def _bo_pos: BO<prefix_bso_pos_pre, pos_r, (outs), (ins RC:$s1, RA:$s2, s10imm:$off10),
                  asmstr # " [${s2}+]$off10, $s1", []>;
  def _bo_pre: BO<prefix_bso_pos_pre, pre_c, (outs), (ins RC:$s1, RA:$s2, s10imm:$off10),
                  asmstr # " [+${s2}]$off10, $s1", []>;

  def _bo_r  : BO<prefix_r_c, pos_r, (outs RP:$d), (ins RC:$s1),
                  asmstr # " [${d}+r], $s1", []>;
  def _bo_c  : BO<prefix_r_c, pre_c, (outs RP:$d), (ins RC:$s1, s10imm:$off10),
                  asmstr # " [${d}+c]$off10, $s1", []>;
}

multiclass mI_SWAP_<bits<8> abs1, bits<2> abs2,   ///_abs
                    bits<8> prefix_bso_pos_pre, bits<8> prefix_r_c,
                    bits<6> bso, ///_bso
                    bits<6> pos_r, ///_pos|_r
                    bits<6> pre_c, ///_pre|_c
                    string asmstr, RegisterClass RC=RA>{
  def _abs: IABS_OR<abs1, abs2, asmstr, RC>;
  defm "" : mI_SWAP_1<prefix_bso_pos_pre, prefix_r_c, bso, pos_r, pre_c, asmstr, RC>;
}

defm SWAP_A    : mI_SWAP_<0xE5, 0x02, 0x49, 0x69, 0x22, 0x02, 0x12, "swap.a">, NsRequires<[HasV110]>;
defm SWAP_W    : mI_SWAP_<0xE5, 0x00, 0x49, 0x69, 0x20, 0x00, 0x10, "swap.w", RD>;
def SWAP_W_bo_i: BO<0x69, 0x20, (outs RD:$d), (ins RP:$s1, s10imm:$off10),
                          "swap.w [${s1}+i], $d", []>, Requires<[HasV160_UP]>;

defm SWAPMSK_W : mI_SWAP_1<0x49, 0x69, 0x22, 0x02, 0x12, "swapmsk.w", RE>, Requires<[HasV161_UP]>;
def SWAPMSK_W_bo_i: BO<0x69, 0x22, (outs RE:$d), (ins RP:$s1, s10imm:$off10),
                          "swapmsk.w [${s1}+i], $d", []>, Requires<[HasV161_UP]>;

def SYSCALL_rc : IRC_C<0xAD, 0x04, "syscall">;

def TLBDEMAP_rr  : IRR_R1<0x75, 0x00, "tlbdemap">, Requires<[HasV130_UP]>;
def TLBFLUSH_A_rr: IRR_0<0x75, 0x04, "tlbflush.a">, Requires<[HasV130_UP]>;
def TLBFLUSH_B_rr: IRR_0<0x75, 0x05, "tlbflush.b">, Requires<[HasV130_UP]>;
def TLBMAP_rr  : IRR_R1<0x75, 0x40, "tlbmap", RE>, Requires<[HasV130_UP]>;
def TLBPROBE_A_rr: IRR_R1<0x75, 0x08, "tlbprobe.a">, Requires<[HasV130_UP]>;
def TLBPROBE_I_rr: IRR_R1<0x75, 0x09, "tlbprobe.i">, Requires<[HasV130_UP]>;

def TRAPSV_sys : ISYS_0<0x0D, 0x15, "trapsv">;
def TRAPV_sys  : ISYS_0<0x0D, 0x14, "trapv">;

multiclass mIRR_a<bits<8> pre, bits<8> op1, bits<8> op2, string asmstr, RegisterClass RC=RD>{
  def _rr_v110: IRR_a<pre, op1, asmstr, RC>, NsRequires<[HasV110]>;
  def _rr : IRR_a<pre, op2, asmstr, RC>, Requires<[HasV120_UP]>;
}

defm UNPACK_rr : mIRR_a<0x4B, 0x50, 0x08, "unpack", RE>;

def WAIT_sys  : ISYS_0<0x0D, 0x16, "wait">, Requires<[HasV161_UP]>;

defm XNOR : mIRR_RC<0x0F, 0x0D, 0x8F, 0x0D, "xnor">;
def XNOR_T : IBIT<0x07, 0x02, "xnor.t">;

defm XOR : mIRR_RC<0x0F, 0x0C, 0x8F, 0x0C, "xor">;
def XOR_srr : ISRR_db<0xC6, "xor">, Requires<[HasV120_UP]>;
def XOR_T : IBIT<0x07, 0x03, "xor.t">;

defm XOR_EQ   : mIRR_RC<0x0B, 0x2F, 0x8B, 0x2F, "xor.eq">;
defm XOR_NE   : mIRR_RC<0x0B, 0x30, 0x8B, 0x30, "xor.ne">;
defm XOR_GE   : mIRR_RC<0x0B, 0x33, 0x8B, 0x33, "xor.ge">;
defm XOR_GE_U : mIRR_RC<0x0B, 0x34, 0x8B, 0x34, "xor.ge.u">;
defm XOR_LT   : mIRR_RC<0x0B, 0x31, 0x8B, 0x31, "xor.lt">;
defm XOR_LT_U : mIRR_RC<0x0B, 0x32, 0x8B, 0x32, "xor.lt.u">;



/// FPU Instructions

def MADD_F_rrr : IRRR<0x6B, 0x06, "madd.f">, Requires<[HasV130_UP]>;
def MSUB_F_rrr : IRRR<0x6B, 0x07, "msub.f">, Requires<[HasV130_UP]>;
def ADD_F_rrr : IRRR_d31<0x6B, 0x02, "add.f">, Requires<[HasV130_UP]>;
def SUB_F_rrr : IRRR_d31<0x6B, 0x03, "sub.f">, Requires<[HasV130_UP]>;
def MUL_F_rrr : IRR_dab<0x4B, 0x04, "mul.f">, Requires<[HasV130_UP]>;
def DIV_F_rr  : IRR_dab<0x4B, 0x05, "div.f">, Requires<[HasV130_UP]>;
def CMP_F_rr  : IRR_dab<0x4B, 0x00, "cmp.f">, Requires<[HasV130_UP]>;

def FTOI_rr   : IRR_a<0x4B, 0x10, "ftoi">, Requires<[HasV130_UP]>;
def FTOIZ_rr  : IRR_a<0x4B, 0x13, "ftoiz">, Requires<[HasV131_UP]>;

def FTOQ31_rr : IRR_dab<0x4B, 0x11, "ftoq31">, Requires<[HasV130_UP]>;
def FTOQ31Z_rr: IRR_dab<0x4B, 0x18, "ftoq31z">, Requires<[HasV131_UP]>;

def FTOU_rr   : IRR_a<0x4B, 0x12, "ftou">, Requires<[HasV130_UP]>;
def FTOUZ_rr  : IRR_a<0x4B, 0x17, "ftouz">, Requires<[HasV131_UP]>;

def FTOHP_rr  : IRR_a<0x4B, 0x25, "ftohp">, Requires<[HasV162_UP]>;

def HPTOF_rr  : IRR_a<0x4B, 0x24, "hptof">, Requires<[HasV162_UP]>;
def ITOF_rr   : IRR_a<0x4B, 0x14, "itof">, Requires<[HasV130_UP]>;

def Q31TOF_rr  : IRR_dab<0x4B, 0x15, "q31tof">, Requires<[HasV130_UP]>;
def QSEED_F_rr : IRR_a<0x4B, 0x19, "qseed.f">, Requires<[HasV130_UP]>;

def UPDFL_rr   : IRR_R1<0x4B, 0x0C, "updfl">, Requires<[HasV130_UP]>;
def UTOF_rr    : IRR_a<0x4B, 0x16, "utof">, Requires<[HasV130_UP]>;

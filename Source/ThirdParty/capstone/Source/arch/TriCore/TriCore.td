//===-- TriCore.td - Describe the TriCore Target Machine ---*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the top level entry point for the TriCore target.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Descriptions
//===----------------------------------------------------------------------===//

// Specify whether target support specific TRICORE ISA variants.

def HasV110Ops   : SubtargetFeature<"v1.1", "HasV110Ops", "true",
								   "Support TriCore v1.1 instructions",
								   []>;
def HasV120Ops   : SubtargetFeature<"v1.2", "HasV120Ops", "true",
								   "Support TriCore v1.2 instructions",
								   []>;
def HasV130Ops   : SubtargetFeature<"v1.3", "HasV130Ops", "true",
								   "Support TriCore v1.3 instructions",
								   []>;
def HasV131Ops   : SubtargetFeature<"v1.3.1", "HasV131Ops", "true",
								   "Support TriCore v1.3.1 instructions",
								   []>;
def HasV160Ops   : SubtargetFeature<"v1.6", "HasV160Ops", "true",
								   "Support TriCore v1.6 instructions",
								   []>;
def HasV161Ops   : SubtargetFeature<"v1.6.1", "HasV161Ops", "true",
								   "Support TriCore v1.6.1 instructions",
								   []>;
def HasV162Ops   : SubtargetFeature<"v1.6.2", "HasV162Ops", "true",
								   "Support TriCore v1.6.2 instructions",
								   []>;

def HasV110     : Predicate<"HasV120Ops()">, AssemblerPredicate<(all_of HasV110Ops), "v1.1">;
def HasV120     : Predicate<"HasV120Ops()">, AssemblerPredicate<(all_of HasV120Ops), "v1.2">;
def HasV130     : Predicate<"HasV130Ops()">, AssemblerPredicate<(all_of HasV130Ops), "v1.3">;
def HasV131     : Predicate<"HasV131Ops()">, AssemblerPredicate<(all_of HasV131Ops), "v1.3.1">;
def HasV160     : Predicate<"HasV160Ops()">, AssemblerPredicate<(all_of HasV160Ops), "v1.6">;
def HasV161     : Predicate<"HasV161Ops()">, AssemblerPredicate<(all_of HasV161Ops), "v1.6.1">;
def HasV162     : Predicate<"HasV162Ops()">, AssemblerPredicate<(all_of HasV162Ops), "v1.6.2">;

def HasV120_UP  : Predicate<"HasV120Ops() || HasV130Ops() || HasV131Ops() || HasV160Ops() || HasV161Ops() || HasV162Ops()">
				, AssemblerPredicate<(any_of HasV120Ops, HasV130Ops, HasV131Ops, HasV160Ops, HasV161Ops, HasV162Ops), "v120up">;
def HasV130_UP  : Predicate<"HasV130Ops() || HasV131Ops() || HasV160Ops() || HasV161Ops() || HasV162Ops()">
				, AssemblerPredicate<(any_of HasV130Ops, HasV131Ops, HasV160Ops, HasV161Ops, HasV162Ops), "v130up">;
def HasV131_UP  : Predicate<"HasV131Ops() || HasV160Ops() || HasV161Ops() || HasV162Ops()">
				, AssemblerPredicate<(any_of HasV131Ops, HasV160Ops, HasV161Ops, HasV162Ops), "v131up">;
def HasV160_UP  : Predicate<"HasV160Ops() || HasV161Ops() || HasV162Ops()">
				, AssemblerPredicate<(any_of HasV160Ops, HasV161Ops, HasV162Ops), "v160up">;
def HasV161_UP  : Predicate<"HasV161Ops() || HasV162Ops()">
				, AssemblerPredicate<(any_of HasV161Ops, HasV162Ops), "v161up">;
def HasV162_UP  : Predicate<"HasV162Ops()">
				, AssemblerPredicate<(any_of HasV162Ops), "v162up">;

def HasV120_DN : Predicate<"HasV120Ops() || HasV110Ops()">,
				AssemblerPredicate<(any_of HasV120Ops, HasV110Ops), "v120dn">;
def HasV130_DN : Predicate<"HasV130Ops() || HasV120Ops() || HasV110Ops()">,
				AssemblerPredicate<(any_of HasV130Ops, HasV120Ops, HasV110Ops), "v130dn">;
def HasV131_DN : Predicate<"HasV131Ops() || HasV130Ops() || HasV120Ops() || HasV110Ops()">,
				AssemblerPredicate<(any_of HasV131Ops, HasV130Ops, HasV120Ops, HasV110Ops), "v131dn">;
def HasV160_DN : Predicate<"HasV160Ops() || HasV131Ops() || HasV130Ops() || HasV120Ops() || HasV110Ops()">,
				AssemblerPredicate<(any_of HasV160Ops, HasV131Ops, HasV130Ops, HasV120Ops, HasV110Ops), "v160dn">;
def HasV161_DN : Predicate<"HasV161Ops() || HasV160Ops() || HasV131Ops() || HasV130Ops() || HasV120Ops() || HasV110Ops()">,
				AssemblerPredicate<(any_of HasV161Ops, HasV160Ops, HasV131Ops, HasV130Ops, HasV120Ops, HasV110Ops), "v161dn">;
def HasV162_DN : Predicate<"HasV162Ops() || HasV161Ops() || HasV160Ops() || HasV131Ops() || HasV130Ops() || HasV120Ops() || HasV110Ops()">,
				AssemblerPredicate<(any_of HasV162Ops, HasV161Ops, HasV160Ops, HasV131Ops, HasV130Ops, HasV120Ops, HasV110Ops), "v162dn">;


class Architecture<string fname, string aname, list<SubtargetFeature> features = []>
  : SubtargetFeature<fname, "TriCoreArch", aname,
					 !strconcat(aname, " architecture"), features>;

class ProcNoItin<string Name, list<SubtargetFeature> Features>
  : Processor<Name, NoItineraries, Features>;


def TRICORE_V1_1    : Architecture<"tricore-v1.1",    "TRICOREv110", [HasV110Ops]>;
def TRICORE_V1_2    : Architecture<"tricore-V1.2",    "TRICOREv120", [HasV120Ops]>;
def TRICORE_V1_3    : Architecture<"tricore-V1.3",    "TRICOREv130", [HasV130Ops]>;
def TRICORE_V1_3_1  : Architecture<"tricore-V1.3.1",  "TRICOREv131", [HasV131Ops]>;
def TRICORE_V1_6    : Architecture<"tricore-V1.6",    "TRICOREv160", [HasV160Ops]>;
def TRICORE_V1_6_1  : Architecture<"tricore-V1.6.1",  "TRICOREv161", [HasV161Ops]>;
def TRICORE_V1_6_2  : Architecture<"tricore-V1.6.2",  "TRICOREv162", [HasV162Ops]>;
def TRICORE_PCP     : Architecture<"tricore-PCP",     "TRICOREpcp">;
def TRICORE_PCP2    : Architecture<"tricore-PCP2",    "TRICOREpcp2">;

def TRICORE_RIDER_A : Architecture<"tricore-rider-a", "TRICOREv110", [TRICORE_V1_1]>;


include "TriCoreRegisterInfo.td"
include "TriCoreInstrInfo.td"
include "TriCoreCallingConv.td"

//===----------------------------------------------------------------------===//
// TriCore processors supported.
//===----------------------------------------------------------------------===//

def : ProcNoItin<"tc1796",  [TRICORE_V1_3]>;
def : ProcNoItin<"tc1797",  [TRICORE_V1_3_1]>;
def : ProcNoItin<"tc27x",   [TRICORE_V1_6_1]>;
def : ProcNoItin<"tc161",   [TRICORE_V1_6_1]>;
def : ProcNoItin<"tc162",   [TRICORE_V1_6_2]>;
def : ProcNoItin<"tc16",    [TRICORE_V1_6]>;
def : ProcNoItin<"tc131",   [TRICORE_V1_3_1]>;
def : ProcNoItin<"tc13",    [TRICORE_V1_3]>;

def TriCoreAsmWriter : AsmWriter {
  int PassSubtarget = 1;
}

def TriCoreInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def TriCore : Target {
  let InstructionSet = TriCoreInstrInfo;
  let AssemblyWriters = [TriCoreAsmWriter];
}

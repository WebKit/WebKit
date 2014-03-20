/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef DFGCommon_h
#define DFGCommon_h

#include "DFGCompilationMode.h"

#if ENABLE(DFG_JIT)

#include "CodeOrigin.h"
#include "Options.h"
#include "VirtualRegister.h"

namespace JSC { namespace DFG {

struct Node;

typedef uint32_t BlockIndex;
static const BlockIndex NoBlock = UINT_MAX;

struct NodePointerTraits {
    static Node* defaultValue() { return 0; }
    static bool isEmptyForDump(Node* value) { return !value; }
};

// Use RefChildren if the child ref counts haven't already been adjusted using
// other means and either of the following is true:
// - The node you're creating is MustGenerate.
// - The place where you're inserting a reference to the node you're creating
//   will not also do RefChildren.
enum RefChildrenMode {
    RefChildren,
    DontRefChildren
};

// Use RefNode if you know that the node will be used from another node, and you
// will not already be ref'ing the node to account for that use.
enum RefNodeMode {
    RefNode,
    DontRefNode
};

inline bool verboseCompilationEnabled(CompilationMode mode = DFGMode)
{
    return Options::verboseCompilation() || Options::dumpGraphAtEachPhase() || (isFTL(mode) && Options::verboseFTLCompilation());
}

inline bool logCompilationChanges(CompilationMode mode = DFGMode)
{
    return verboseCompilationEnabled(mode) || Options::logCompilationChanges();
}

inline bool shouldDumpGraphAtEachPhase()
{
    return Options::dumpGraphAtEachPhase();
}

inline bool validationEnabled()
{
#if !ASSERT_DISABLED
    return true;
#else
    return Options::validateGraph() || Options::validateGraphAtEachPhase();
#endif
}

inline bool enableInt52()
{
#if USE(JSVALUE64)
    return true;
#else
    return false;
#endif
}

enum NoResultTag { NoResult };

enum OptimizationFixpointState { BeforeFixpoint, FixpointNotConverged, FixpointConverged };

// Describes the form you can expect the entire graph to be in.
enum GraphForm {
    // LoadStore form means that basic blocks may freely use GetLocal, SetLocal,
    // GetLocalUnlinked, and Flush for accessing local variables and indicating
    // where their live ranges ought to be. Data flow between local accesses is
    // implicit. Liveness is only explicit at block heads (variablesAtHead).
    // This is only used by the DFG simplifier and is only preserved by same.
    //
    // For example, LoadStore form gives no easy way to determine which SetLocal's
    // flow into a GetLocal. As well, LoadStore form implies no restrictions on
    // redundancy: you can freely emit multiple GetLocals, or multiple SetLocals
    // (or any combination thereof) to the same local in the same block. LoadStore
    // form does not require basic blocks to declare how they affect or use locals,
    // other than implicitly by using the local ops and by preserving
    // variablesAtHead. Finally, LoadStore allows flexibility in how liveness of
    // locals is extended; for example you can replace a GetLocal with a Phantom
    // and so long as the Phantom retains the GetLocal's children (i.e. the Phi
    // most likely) then it implies that the local is still live but that it need
    // not be stored to the stack necessarily. This implies that Phantom can
    // reference nodes that have no result, as long as those nodes are valid
    // GetLocal children (i.e. Phi, SetLocal, SetArgument).
    //
    // LoadStore form also implies that Phis need not have children. By default,
    // they end up having no children if you enter LoadStore using the canonical
    // way (call Graph::dethread).
    //
    // LoadStore form is suitable for CFG transformations, as well as strength
    // reduction, folding, and CSE.
    LoadStore,
    
    // ThreadedCPS form means that basic blocks list up-front which locals they
    // expect to be live at the head, and which locals they make available at the
    // tail. ThreadedCPS form also implies that:
    //
    // - GetLocals and SetLocals to uncaptured variables are not redundant within
    //   a basic block.
    //
    // - All GetLocals and Flushes are linked directly to the last access point
    //   of the variable, which must not be another GetLocal if the variable is
    //   uncaptured.
    //
    // - Phantom(Phi) is not legal, but PhantomLocal is.
    //
    // ThreadedCPS form is suitable for data flow analysis (CFA, prediction
    // propagation), register allocation, and code generation.
    ThreadedCPS,
    
    // SSA form. See DFGSSAConversionPhase.h for a description.
    SSA
};

// Describes the state of the UnionFind structure of VariableAccessData's.
enum UnificationState {
    // BasicBlock-local accesses to variables are appropriately unified with each other.
    LocallyUnified,
    
    // Unification has been performed globally.
    GloballyUnified
};

// Describes how reference counts in the graph behave.
enum RefCountState {
    // Everything has refCount() == 1.
    EverythingIsLive,

    // Set after DCE has run.
    ExactRefCount
};

enum OperandSpeculationMode { AutomaticOperandSpeculation, ManualOperandSpeculation };

enum ProofStatus { NeedsCheck, IsProved };

inline bool isProved(ProofStatus proofStatus)
{
    ASSERT(proofStatus == IsProved || proofStatus == NeedsCheck);
    return proofStatus == IsProved;
}

inline ProofStatus proofStatusForIsProved(bool isProved)
{
    return isProved ? IsProved : NeedsCheck;
}

enum KillStatus { DoesNotKill, DoesKill };

inline bool doesKill(KillStatus killStatus)
{
    ASSERT(killStatus == DoesNotKill || killStatus == DoesKill);
    return killStatus == DoesKill;
}

inline KillStatus killStatusForDoesKill(bool doesKill)
{
    return doesKill ? DoesKill : DoesNotKill;
}

template<typename T, typename U>
bool checkAndSet(T& left, U right)
{
    if (left == right)
        return false;
    left = right;
    return true;
}

} } // namespace JSC::DFG

namespace WTF {

void printInternal(PrintStream&, JSC::DFG::OptimizationFixpointState);
void printInternal(PrintStream&, JSC::DFG::GraphForm);
void printInternal(PrintStream&, JSC::DFG::UnificationState);
void printInternal(PrintStream&, JSC::DFG::RefCountState);
void printInternal(PrintStream&, JSC::DFG::ProofStatus);

} // namespace WTF

#endif // ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

// Put things here that must be defined even if ENABLE(DFG_JIT) is false.

enum CapabilityLevel {
    CannotCompile,
    CanInline,
    CanCompile,
    CanCompileAndInline,
    CapabilityLevelNotSet
};

inline bool canCompile(CapabilityLevel level)
{
    switch (level) {
    case CanCompile:
    case CanCompileAndInline:
        return true;
    default:
        return false;
    }
}

inline bool canInline(CapabilityLevel level)
{
    switch (level) {
    case CanInline:
    case CanCompileAndInline:
        return true;
    default:
        return false;
    }
}

inline CapabilityLevel leastUpperBound(CapabilityLevel a, CapabilityLevel b)
{
    switch (a) {
    case CannotCompile:
        return CannotCompile;
    case CanInline:
        switch (b) {
        case CanInline:
        case CanCompileAndInline:
            return CanInline;
        default:
            return CannotCompile;
        }
    case CanCompile:
        switch (b) {
        case CanCompile:
        case CanCompileAndInline:
            return CanCompile;
        default:
            return CannotCompile;
        }
    case CanCompileAndInline:
        return b;
    case CapabilityLevelNotSet:
        ASSERT_NOT_REACHED();
        return CannotCompile;
    }
    ASSERT_NOT_REACHED();
    return CannotCompile;
}

// Unconditionally disable DFG disassembly support if the DFG is not compiled in.
inline bool shouldShowDisassembly(CompilationMode mode = DFGMode)
{
#if ENABLE(DFG_JIT)
    return Options::showDisassembly() || Options::showDFGDisassembly() || (isFTL(mode) && Options::showFTLDisassembly());
#else
    UNUSED_PARAM(mode);
    return false;
#endif
}

} } // namespace JSC::DFG

#endif // DFGCommon_h


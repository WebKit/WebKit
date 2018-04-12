/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "AssemblyHelpers.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "StackAlignment.h"
#include <wtf/FunctionTraits.h>

namespace JSC {

#if CPU(MIPS) || (OS(WINDOWS) && CPU(X86_64))
#define POKE_ARGUMENT_OFFSET 4
#else
#define POKE_ARGUMENT_OFFSET 0
#endif

// EncodedJSValue in JSVALUE32_64 is a 64-bit integer. When being compiled in ARM EABI, it must be aligned even-numbered register (r0, r2 or [sp]).
// To avoid assemblies from using wrong registers, let's occupy r1 or r3 with a dummy argument when necessary.
#if (COMPILER_SUPPORTS(EABI) && CPU(ARM)) || CPU(MIPS)
#define EABI_32BIT_DUMMY_ARG      CCallHelpers::TrustedImm32(0),
#else
#define EABI_32BIT_DUMMY_ARG
#endif

class ExecState;
class Structure;
namespace DFG {
class RegisteredStructure;
};

class CCallHelpers : public AssemblyHelpers {
public:
    CCallHelpers(CodeBlock* codeBlock = 0)
        : AssemblyHelpers(codeBlock)
    {
    }

    // Wrapper to encode JSCell GPR into JSValue.
    class CellValue {
    public:
        explicit CellValue(GPRReg gpr)
            : m_gpr(gpr)
        {
        }

        GPRReg gpr() const { return m_gpr; }

    private:
        GPRReg m_gpr;
    };

    // The most general helper for setting arguments that fit in a GPR, if you can compute each
    // argument without using any argument registers. You usually want one of the setupArguments*()
    // methods below instead of this. This thing is most useful if you have *a lot* of arguments.
    template<typename Functor>
    void setupArgument(unsigned argumentIndex, const Functor& functor)
    {
        unsigned numberOfRegs = GPRInfo::numberOfArgumentRegisters; // Disguise the constant from clang's tautological compare warning.
        if (argumentIndex < numberOfRegs) {
            functor(GPRInfo::toArgumentRegister(argumentIndex));
            return;
        }

        functor(GPRInfo::nonArgGPR0);
        poke(GPRInfo::nonArgGPR0, POKE_ARGUMENT_OFFSET + argumentIndex - GPRInfo::numberOfArgumentRegisters);
    }

private:

    template<unsigned NumberOfRegisters, typename RegType>
    ALWAYS_INLINE void setupStubArgs(std::array<RegType, NumberOfRegisters> destinations, std::array<RegType, NumberOfRegisters> sources)
    {
        if (!ASSERT_DISABLED) {
            RegisterSet set;
            for (RegType dest : destinations)
                set.set(dest);
            ASSERT_WITH_MESSAGE(set.numberOfSetRegisters() == NumberOfRegisters, "Destinations should not be aliased.");
        }

        typedef std::pair<RegType, RegType> RegPair;
        Vector<RegPair, NumberOfRegisters> pairs;

        for (unsigned i = 0; i < NumberOfRegisters; ++i) {
            if (sources[i] != destinations[i])
                pairs.append(std::make_pair(sources[i], destinations[i]));
        }

#if !ASSERT_DISABLED
        auto numUniqueSources = [&] () -> unsigned {
            RegisterSet set;
            for (auto& pair : pairs) {
                RegType source = pair.first;
                set.set(source);
            }
            return set.numberOfSetRegisters();
        };

        auto numUniqueDests = [&] () -> unsigned {
            RegisterSet set;
            for (auto& pair : pairs) {
                RegType dest = pair.second;
                set.set(dest);
            }
            return set.numberOfSetRegisters();
        };
#endif

        while (pairs.size()) {
            RegisterSet freeDestinations;
            for (auto& pair : pairs) {
                RegType dest = pair.second;
                freeDestinations.set(dest);
            }
            for (auto& pair : pairs) {
                RegType source = pair.first;
                freeDestinations.clear(source);
            }

            if (freeDestinations.numberOfSetRegisters()) {
                bool madeMove = false;
                for (unsigned i = 0; i < pairs.size(); i++) {
                    auto& pair = pairs[i];
                    RegType source = pair.first;
                    RegType dest = pair.second;
                    if (freeDestinations.get(dest)) {
                        move(source, dest);
                        pairs.remove(i);
                        madeMove = true;
                        break;
                    }
                }
                ASSERT_UNUSED(madeMove, madeMove);
                continue;
            }

            ASSERT(numUniqueDests() == numUniqueSources());
            ASSERT(numUniqueDests() == pairs.size());
            // The set of source and destination registers are equivalent sets. This means we don't have
            // any free destination registers that won't also clobber a source. We get around this by
            // exchanging registers.

            RegType source = pairs[0].first;
            RegType dest = pairs[0].second;
            swap(source, dest);
            pairs.remove(0);

            RegType newSource = source;
            for (auto& pair : pairs) {
                RegType source = pair.first;
                if (source == dest) {
                    pair.first = newSource;
                    break;
                }
            }

            // We may have introduced pairs that have the same source and destination. Remove those now.
            for (unsigned i = 0; i < pairs.size(); i++) {
                auto& pair = pairs[i];
                if (pair.first == pair.second) {
                    pairs.remove(i);
                    i--;
                }
            }
        }
    }

    template<typename RegType>
    using InfoTypeForReg = decltype(toInfoFromReg(RegType(-1)));

    // extraPoke is used to track 64-bit argument types passed on the stack.
    template<unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke>
    struct ArgCollection {
        ArgCollection()
        {
            gprSources.fill(InvalidGPRReg);
            gprDestinations.fill(InvalidGPRReg);
            fprSources.fill(InvalidFPRReg);
            fprDestinations.fill(InvalidFPRReg);
        }

        template<unsigned a, unsigned b, unsigned c, unsigned d, unsigned e>
        ArgCollection(ArgCollection<a, b, c, d, e>& other)
        {
            gprSources = other.gprSources;
            gprDestinations = other.gprDestinations;
            fprSources = other.fprSources;
            fprDestinations = other.fprDestinations;
        }

        ArgCollection<numGPRArgs + 1, numGPRSources + 1, numFPRArgs, numFPRSources, extraPoke> pushRegArg(GPRReg argument, GPRReg destination)
        {
            ArgCollection<numGPRArgs + 1, numGPRSources + 1, numFPRArgs, numFPRSources, extraPoke> result(*this);

            result.gprSources[numGPRSources] = argument;
            result.gprDestinations[numGPRSources] = destination;
            return result;
        }

        ArgCollection<numGPRArgs, numGPRSources, numFPRArgs + 1, numFPRSources + 1, extraPoke> pushRegArg(FPRReg argument, FPRReg destination)
        {
            ArgCollection<numGPRArgs, numGPRSources, numFPRArgs + 1, numFPRSources + 1, extraPoke> result(*this);

            result.fprSources[numFPRSources] = argument;
            result.fprDestinations[numFPRSources] = destination;
            return result;
        }

        ArgCollection<numGPRArgs + 1, numGPRSources, numFPRArgs, numFPRSources, extraPoke> addGPRArg()
        {
            return ArgCollection<numGPRArgs + 1, numGPRSources, numFPRArgs, numFPRSources, extraPoke>(*this);
        }

        ArgCollection<numGPRArgs + 1, numGPRSources, numFPRArgs, numFPRSources, extraPoke> addStackArg(GPRReg)
        {
            return ArgCollection<numGPRArgs + 1, numGPRSources, numFPRArgs, numFPRSources, extraPoke>(*this);
        }

        ArgCollection<numGPRArgs, numGPRSources, numFPRArgs + 1, numFPRSources, extraPoke> addStackArg(FPRReg)
        {
            return ArgCollection<numGPRArgs, numGPRSources, numFPRArgs + 1, numFPRSources, extraPoke>(*this);
        }

        ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke + 1> addPoke()
        {
            return ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke + 1>(*this);
        }

#if OS(WINDOWS) && CPU(X86_64)
        unsigned argCount(GPRReg) { return numGPRArgs + numFPRArgs; }
        unsigned argCount(FPRReg) { return numGPRArgs + numFPRArgs; }
#else
        unsigned argCount(GPRReg) { return numGPRArgs; }
        unsigned argCount(FPRReg) { return numFPRArgs; }
#endif

        std::array<GPRReg, GPRInfo::numberOfRegisters> gprSources;
        std::array<GPRReg, GPRInfo::numberOfRegisters> gprDestinations;
        std::array<FPRReg, FPRInfo::numberOfRegisters> fprSources;
        std::array<FPRReg, FPRInfo::numberOfRegisters> fprDestinations;
    };

    template<unsigned TargetSize, typename RegType>
    std::array<RegType, TargetSize> clampArrayToSize(std::array<RegType, InfoTypeForReg<RegType>::numberOfRegisters> sourceArray)
    {
        static_assert(TargetSize <= sourceArray.size(), "TargetSize is bigger than source.size()");
        RELEASE_ASSERT(TargetSize <= InfoTypeForReg<RegType>::numberOfRegisters);

        std::array<RegType, TargetSize> result { };

        for (unsigned i = 0; i < TargetSize; i++) {
            ASSERT(sourceArray[i] != InfoTypeForReg<RegType>::InvalidIndex);
            result[i] = sourceArray[i];
        }

        return result;
    }

    template<typename ArgType>
    ALWAYS_INLINE void pokeForArgument(ArgType arg, unsigned currentGPRArgument, unsigned currentFPRArgument, unsigned extraPoke)
    {
        // Clang claims that it cannot find the symbol for FPRReg/GPRReg::numberOfArgumentRegisters when they are passed directly to std::max... seems like a bug
        unsigned numberOfFPArgumentRegisters = FPRInfo::numberOfArgumentRegisters;
        unsigned numberOfGPArgumentRegisters = GPRInfo::numberOfArgumentRegisters;
        ASSERT(currentGPRArgument >= GPRInfo::numberOfArgumentRegisters || currentFPRArgument >= FPRInfo::numberOfArgumentRegisters);

        unsigned pokeOffset = POKE_ARGUMENT_OFFSET + extraPoke;
        pokeOffset += std::max(currentGPRArgument, numberOfGPArgumentRegisters) - numberOfGPArgumentRegisters;
        pokeOffset += std::max(currentFPRArgument, numberOfFPArgumentRegisters) - numberOfFPArgumentRegisters;
        poke(arg, pokeOffset);
    }

    // In the auto-calling convention code below the order of operations is:
    //    1) spill arguments to stack slots
    //    2) shuffle incomming argument values in registers to argument registers
    //    3) fill immediate values to argument registers
    // To do this, we recurse forwards through our args collecting argument values in registers and spilling stack slots.
    // when we run out of args we then run our shuffling code to relocate registers. Finally, as we unwind from our
    // recursion we can fill immediates.

#define CURRENT_ARGUMENT_TYPE typename FunctionTraits<OperationType>::template ArgumentType<numGPRArgs + numFPRArgs>
#define RESULT_TYPE typename FunctionTraits<OperationType>::ResultType

#if USE(JSVALUE64)

    // Avoid MSVC optimization time explosion associated with __forceinline in recursive templates.
    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename RegType, typename... Args>
    ALWAYS_INLINE_EXCEPT_MSVC void marshallArgumentRegister(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, RegType arg, Args... args)
    {
        using InfoType = InfoTypeForReg<RegType>;
        unsigned numArgRegisters = InfoType::numberOfArgumentRegisters;
#if OS(WINDOWS) && CPU(X86_64)
        unsigned currentArgCount = argSourceRegs.argCount(arg) + (std::is_same<RESULT_TYPE, SlowPathReturnType>::value ? 1 : 0);
#else
        unsigned currentArgCount = argSourceRegs.argCount(arg);
#endif
        if (currentArgCount < numArgRegisters) {
            auto updatedArgSourceRegs = argSourceRegs.pushRegArg(arg, InfoType::toArgumentRegister(currentArgCount));
            setupArgumentsImpl<OperationType>(updatedArgSourceRegs, args...);
            return;
        }

        pokeForArgument(arg, numGPRArgs, numFPRArgs, extraPoke);
        setupArgumentsImpl<OperationType>(argSourceRegs.addStackArg(arg), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE void setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, FPRReg arg, Args... args)
    {
        static_assert(std::is_same<CURRENT_ARGUMENT_TYPE, double>::value, "We should only be passing FPRRegs to a double");
        marshallArgumentRegister<OperationType>(argSourceRegs, arg, args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE void setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, GPRReg arg, Args... args)
    {
        marshallArgumentRegister<OperationType>(argSourceRegs, arg, args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE void setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, JSValueRegs arg, Args... args)
    {
        marshallArgumentRegister<OperationType>(argSourceRegs, arg.gpr(), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE void setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, CellValue arg, Args... args)
    {
        marshallArgumentRegister<OperationType>(argSourceRegs, arg.gpr(), args...);
    }

#else // USE(JSVALUE64)

    // These functions are a hack for X86 since it has no argument gprs...

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE void setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, FPRReg arg, Args... args)
    {
        static_assert(std::is_same<CURRENT_ARGUMENT_TYPE, double>::value, "We should only be passing FPRRegs to a double");
        pokeForArgument(arg, numGPRArgs, numFPRArgs, extraPoke);
        setupArgumentsImpl<OperationType>(argSourceRegs.addStackArg(arg).addPoke(), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE std::enable_if_t<sizeof(CURRENT_ARGUMENT_TYPE) <= 4>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, GPRReg arg, Args... args)
    {
        pokeForArgument(arg, numGPRArgs, numFPRArgs, extraPoke);
        setupArgumentsImpl<OperationType>(argSourceRegs.addGPRArg(), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE std::enable_if_t<std::is_same<CURRENT_ARGUMENT_TYPE, EncodedJSValue>::value>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, JSValueRegs arg, Args... args)
    {
        pokeForArgument(arg.payloadGPR(), numGPRArgs, numFPRArgs, extraPoke);
        pokeForArgument(arg.tagGPR(), numGPRArgs, numFPRArgs, extraPoke + 1);
        setupArgumentsImpl<OperationType>(argSourceRegs.addGPRArg().addPoke(), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename... Args>
    ALWAYS_INLINE std::enable_if_t<std::is_same<CURRENT_ARGUMENT_TYPE, EncodedJSValue>::value>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, CellValue arg, Args... args)
    {
        pokeForArgument(arg.gpr(), numGPRArgs, numFPRArgs, extraPoke);
        pokeForArgument(TrustedImm32(JSValue::CellTag), numGPRArgs, numFPRArgs, extraPoke + 1);
        setupArgumentsImpl<OperationType>(argSourceRegs.addGPRArg().addPoke(), args...);
    }

#endif // USE(JSVALUE64)

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename Arg, typename... Args>
    ALWAYS_INLINE std::enable_if_t<
        std::is_base_of<TrustedImm, Arg>::value
        || std::is_convertible<Arg, TrustedImm>::value> // We have this since DFGSpeculativeJIT has it's own implementation of TrustedImmPtr
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, Arg arg, Args... args)
    {
        // Right now this only supports non-floating point immediate arguments since we never call operations with non-register values.
        // If we ever needed to support immediate floating point arguments we would need to duplicate this logic for both types, which sounds
        // gross so it's probably better to do that marshalling before the call operation...
        static_assert(!std::is_floating_point<CURRENT_ARGUMENT_TYPE>::value, "We don't support immediate floats/doubles in setupArguments");
        auto numArgRegisters = GPRInfo::numberOfArgumentRegisters;
#if OS(WINDOWS) && CPU(X86_64)
        auto currentArgCount = numGPRArgs + numFPRArgs + (std::is_same<RESULT_TYPE, SlowPathReturnType>::value ? 1 : 0);
#else
        auto currentArgCount = numGPRArgs;
#endif
        if (currentArgCount < numArgRegisters) {
            setupArgumentsImpl<OperationType>(argSourceRegs.addGPRArg(), args...);
            move(arg, GPRInfo::toArgumentRegister(currentArgCount));
            return;
        }

        pokeForArgument(arg, numGPRArgs, numFPRArgs, extraPoke);
        setupArgumentsImpl<OperationType>(argSourceRegs.addGPRArg(), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename Arg, typename... Args>
    ALWAYS_INLINE std::enable_if_t<
        std::is_same<CURRENT_ARGUMENT_TYPE, Arg>::value
        && std::is_integral<CURRENT_ARGUMENT_TYPE>::value
        && (sizeof(CURRENT_ARGUMENT_TYPE) <= 4)>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, Arg arg, Args... args)
    {
        setupArgumentsImpl<OperationType>(argSourceRegs, TrustedImm32(arg), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename Arg, typename... Args>
    ALWAYS_INLINE std::enable_if_t<
        std::is_same<CURRENT_ARGUMENT_TYPE, Arg>::value
        && std::is_integral<CURRENT_ARGUMENT_TYPE>::value
        && (sizeof(CURRENT_ARGUMENT_TYPE) == 8)>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, Arg arg, Args... args)
    {
        setupArgumentsImpl<OperationType>(argSourceRegs, TrustedImm64(arg), args...);
    }

    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename Arg, typename... Args>
    ALWAYS_INLINE std::enable_if_t<
        std::is_pointer<CURRENT_ARGUMENT_TYPE>::value
        && ((std::is_pointer<Arg>::value && std::is_convertible<std::remove_const_t<std::remove_pointer_t<Arg>>*, CURRENT_ARGUMENT_TYPE>::value)
            || std::is_same<Arg, std::nullptr_t>::value)>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, Arg arg, Args... args)
    {
        setupArgumentsImpl<OperationType>(argSourceRegs, TrustedImmPtr(arg), args...);
    }

    // Special case DFG::RegisteredStructure because it's really annoying to deal with otherwise...
    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke, typename Arg, typename... Args>
    ALWAYS_INLINE std::enable_if_t<
        std::is_same<CURRENT_ARGUMENT_TYPE, Structure*>::value
        && std::is_same<Arg, DFG::RegisteredStructure>::value>
    setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs, Arg arg, Args... args)
    {
        setupArgumentsImpl<OperationType>(argSourceRegs, TrustedImmPtr(arg.get()), args...);
    }

#undef CURRENT_ARGUMENT_TYPE
#undef RESULT_TYPE

    // Base case; set up the argument registers.
    template<typename OperationType, unsigned numGPRArgs, unsigned numGPRSources, unsigned numFPRArgs, unsigned numFPRSources, unsigned extraPoke>
    ALWAYS_INLINE void setupArgumentsImpl(ArgCollection<numGPRArgs, numGPRSources, numFPRArgs, numFPRSources, extraPoke> argSourceRegs)
    {
        static_assert(FunctionTraits<OperationType>::arity == numGPRArgs + numFPRArgs, "One last sanity check");
        static_assert(FunctionTraits<OperationType>::cCallArity() == numGPRArgs + numFPRArgs + extraPoke, "Check the CCall arity");
        setupStubArgs<numGPRSources, GPRReg>(clampArrayToSize<numGPRSources, GPRReg>(argSourceRegs.gprDestinations), clampArrayToSize<numGPRSources, GPRReg>(argSourceRegs.gprSources));

        setupStubArgs<numFPRSources, FPRReg>(clampArrayToSize<numFPRSources, FPRReg>(argSourceRegs.fprDestinations), clampArrayToSize<numFPRSources, FPRReg>(argSourceRegs.fprSources));
    }

public:

#define FIRST_ARGUMENT_TYPE typename FunctionTraits<OperationType>::template ArgumentType<0>

    template<typename OperationType, typename... Args>
    ALWAYS_INLINE std::enable_if_t<std::is_same<FIRST_ARGUMENT_TYPE, ExecState*>::value> setupArguments(Args... args)
    {
#if USE(JSVALUE64)
        // This only really works for 64-bit since jsvalue regs mess things up for 32-bit...
        static_assert(FunctionTraits<OperationType>::cCallArity() == sizeof...(Args) + 1, "Basic sanity check");
#endif
        setupArgumentsImpl<OperationType, 0, 0, 0, 0, 0>(ArgCollection<0, 0, 0, 0, 0>(), GPRInfo::callFrameRegister, args...);
    }

    template<typename OperationType, typename... Args>
    ALWAYS_INLINE std::enable_if_t<!std::is_same<FIRST_ARGUMENT_TYPE, ExecState*>::value> setupArguments(Args... args)
    {
#if USE(JSVALUE64)
        // This only really works for 64-bit since jsvalue regs mess things up for 32-bit...
        static_assert(FunctionTraits<OperationType>::cCallArity() == sizeof...(Args), "Basic sanity check");
#endif
        setupArgumentsImpl<OperationType, 0, 0, 0, 0, 0>(ArgCollection<0, 0, 0, 0, 0>(), args...);
    }

#undef FIRST_ARGUMENT_TYPE

    void setupResults(GPRReg destA, GPRReg destB)
    {
        GPRReg srcA = GPRInfo::returnValueGPR;
        GPRReg srcB = GPRInfo::returnValueGPR2;

        if (destA == InvalidGPRReg)
            move(srcB, destB);
        else if (destB == InvalidGPRReg)
            move(srcA, destA);
        else if (srcB != destA) {
            // Handle the easy cases - two simple moves.
            move(srcA, destA);
            move(srcB, destB);
        } else if (srcA != destB) {
            // Handle the non-swap case - just put srcB in place first.
            move(srcB, destB);
            move(srcA, destA);
        } else
            swap(destA, destB);
    }
    
    void setupResults(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        move(GPRInfo::returnValueGPR, regs.gpr());
#else
        setupResults(regs.payloadGPR(), regs.tagGPR());
#endif
    }
    
    void jumpToExceptionHandler(VM& vm)
    {
        // genericUnwind() leaves the handler CallFrame* in vm->callFrameForCatch,
        // and the address of the handler in vm->targetMachinePCForThrow.
        loadPtr(&vm.targetMachinePCForThrow, GPRInfo::regT1);
        jump(GPRInfo::regT1, ExceptionHandlerPtrTag);
    }

    void prepareForTailCallSlow(GPRReg calleeGPR = InvalidGPRReg)
    {
        GPRReg temp1 = calleeGPR == GPRInfo::regT0 ? GPRInfo::regT3 : GPRInfo::regT0;
        GPRReg temp2 = calleeGPR == GPRInfo::regT1 ? GPRInfo::regT3 : GPRInfo::regT1;
        GPRReg temp3 = calleeGPR == GPRInfo::regT2 ? GPRInfo::regT3 : GPRInfo::regT2;

        GPRReg newFramePointer = temp1;
        GPRReg newFrameSizeGPR = temp2;
        {
            // The old frame size is its number of arguments (or number of
            // parameters in case of arity fixup), plus the frame header size,
            // aligned
            GPRReg oldFrameSizeGPR = temp2;
            {
                GPRReg argCountGPR = oldFrameSizeGPR;
                load32(Address(framePointerRegister, CallFrameSlot::argumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset), argCountGPR);

                {
                    GPRReg numParametersGPR = temp1;
                    {
                        GPRReg codeBlockGPR = numParametersGPR;
                        loadPtr(Address(framePointerRegister, CallFrameSlot::codeBlock * static_cast<int>(sizeof(Register))), codeBlockGPR);
                        load32(Address(codeBlockGPR, CodeBlock::offsetOfNumParameters()), numParametersGPR);
                    }

                    ASSERT(numParametersGPR != argCountGPR);
                    Jump argumentCountWasNotFixedUp = branch32(BelowOrEqual, numParametersGPR, argCountGPR);
                    move(numParametersGPR, argCountGPR);
                    argumentCountWasNotFixedUp.link(this);
                }

                add32(TrustedImm32(stackAlignmentRegisters() + CallFrame::headerSizeInRegisters - 1), argCountGPR, oldFrameSizeGPR);
                and32(TrustedImm32(-stackAlignmentRegisters()), oldFrameSizeGPR);
                // We assume < 2^28 arguments
                mul32(TrustedImm32(sizeof(Register)), oldFrameSizeGPR, oldFrameSizeGPR);
            }

            // The new frame pointer is at framePointer + oldFrameSize - newFrameSize
            ASSERT(newFramePointer != oldFrameSizeGPR);
            addPtr(framePointerRegister, oldFrameSizeGPR, newFramePointer);

            // The new frame size is just the number of arguments plus the
            // frame header size, aligned
            ASSERT(newFrameSizeGPR != newFramePointer);
            load32(Address(stackPointerRegister, CallFrameSlot::argumentCount * static_cast<int>(sizeof(Register)) + PayloadOffset - sizeof(CallerFrameAndPC)),
                newFrameSizeGPR);
            add32(TrustedImm32(stackAlignmentRegisters() + CallFrame::headerSizeInRegisters - 1), newFrameSizeGPR);
            and32(TrustedImm32(-stackAlignmentRegisters()), newFrameSizeGPR);
            // We assume < 2^28 arguments
            mul32(TrustedImm32(sizeof(Register)), newFrameSizeGPR, newFrameSizeGPR);
        }

        GPRReg tempGPR = temp3;
        ASSERT(tempGPR != newFramePointer && tempGPR != newFrameSizeGPR);

        // We don't need the current frame beyond this point. Masquerade as our
        // caller.
#if CPU(ARM) || CPU(ARM64)
        loadPtr(Address(framePointerRegister, CallFrame::returnPCOffset()), linkRegister);
        subPtr(TrustedImm32(2 * sizeof(void*)), newFrameSizeGPR);
#if USE(POINTER_PROFILING)
        addPtr(TrustedImm32(sizeof(CallerFrameAndPC)), MacroAssembler::framePointerRegister, tempGPR);
        untagPtr(linkRegister, tempGPR);
#endif
#elif CPU(MIPS)
        loadPtr(Address(framePointerRegister, sizeof(void*)), returnAddressRegister);
        subPtr(TrustedImm32(2 * sizeof(void*)), newFrameSizeGPR);
#elif CPU(X86) || CPU(X86_64)
        loadPtr(Address(framePointerRegister, sizeof(void*)), tempGPR);
        push(tempGPR);
        subPtr(TrustedImm32(sizeof(void*)), newFrameSizeGPR);
#else
        UNREACHABLE_FOR_PLATFORM();
#endif
        subPtr(newFrameSizeGPR, newFramePointer);
        loadPtr(Address(framePointerRegister), framePointerRegister);


        // We need to move the newFrameSizeGPR slots above the stack pointer by
        // newFramePointer registers. We use pointer-sized chunks.
        MacroAssembler::Label copyLoop(label());

        subPtr(TrustedImm32(sizeof(void*)), newFrameSizeGPR);
        loadPtr(BaseIndex(stackPointerRegister, newFrameSizeGPR, TimesOne), tempGPR);
        storePtr(tempGPR, BaseIndex(newFramePointer, newFrameSizeGPR, TimesOne));

        branchTest32(MacroAssembler::NonZero, newFrameSizeGPR).linkTo(copyLoop, this);

        // Ready for a jump!
        move(newFramePointer, stackPointerRegister);
    }
    
    // These operations clobber all volatile registers. They assume that there is room on the top of
    // stack to marshall call arguments.
    void logShadowChickenProloguePacket(GPRReg shadowPacket, GPRReg scratch1, GPRReg scope);
    void logShadowChickenTailPacket(GPRReg shadowPacket, JSValueRegs thisRegs, GPRReg scope, CodeBlock*, CallSiteIndex);
    // Leaves behind a pointer to the Packet we should write to in shadowPacket.
    void ensureShadowChickenPacket(VM&, GPRReg shadowPacket, GPRReg scratch1NonArgGPR, GPRReg scratch2);
};

} // namespace JSC

#endif // ENABLE(JIT)

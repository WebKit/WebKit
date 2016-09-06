/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ThrowScope.h"

#include "JSCInlines.h"
#include "VM.h"

namespace JSC {
    
#if ENABLE(THROW_SCOPE_VERIFICATION)

namespace {

// Logs all ThrowScope activity to help debug the source of a verification failure.
bool traceOn = false;

// A more verbose logging option to dump the C++ stack trace at strategic points to aid debugging.
bool traceWithStackTraces = false;

// Disabled temporarily until all known verification failures are fixed.
bool verificationOn = false;

unsigned traceCount = 0;
};

/*
    ThrowScope verification works to simulate exception throws and catch cases where
    exception checks are missing. This is how it works:

 1. The VM has a m_needExceptionCheck bit that indicates where an exception check is
    needed. You can think of the m_needExceptionCheck bit being set as a simulated
    throw.

 2. Every throw site must declare a ThrowScope instance using DECLARE_THROW_SCOPE at
    the top of its function (as early as possible) e.g.
 
        void foo(...)
        {
            auto scope = DECLARE_THROW_SCOPE(vm);
            throwException(exec, scope, ...);
        }

    Note: VM::throwException() methods are private, and only calleableby the ThrowScope
    friend class. All throws must go through a ThrowScope. Hence, we are guaranteed that
    any function that can throw will have a ThrowScope.

    Note: by convention, every throw helper function must take a ThrowScope argument
    instead of instantiating its own ThrowScope.  This allows the throw to be attributed
    to the client code rather than the throw helper itself.

 3. Verification of needed exception checks

    a. On construction, each ThrowScope will verify that VM::m_needExceptionCheck is
       not set.
 
       This ensures that the caller of the current function has checked for exceptions
       where needed before doing more work which led to calling the current function.

    b. On destruction, each ThrowScope will verify that VM::m_needExceptionCheck is
       not set. This verification will be skipped if the ThrowScope has been released
       (see (5) below).

       This ensures that the function that owns this ThrowScope is not missing any
       exception checks before returning.
 
    c. When throwing an exception, the ThrowScope will verify that VM::m_needExceptionCheck
       is not set, unless it's been ask to rethrow the same Exception object.

 4. Simulated throws

    Throws are simulated by setting the m_needExceptionCheck bit.

    The bit will only be set in the ThrowScope destructor except when the ThrowScope
    detects the caller is a LLInt or JIT function. LLInt or JIT functions will always
    check for exceptions after a host C++ function returns to it. However, they will
    not clear the m_needExceptionCheck bit.

    Hence, if the ThrowScope destructor detects the caller is a LLInt or JIT function,
    it will just skip the setting of the bit.

    Note: there is no need, and it is incorrect to set the m_needExceptionCheck bit
    in the throwException methods. This is because, in practice, we always return
    immediately after throwing an exception. It doesn't make sense to set the bit in
    the throw just to have to clear it immediately after before we do verification in
    the ThrowScope destructor.

 5. Using ThrowScope::release()

    ThrowScope::release() should only be used at the bottom of a function if:
 
    a. This function is going to let its caller check and handle the exception.
 
        void foo(...)
        {
            auto scope = DECLARE_THROW_SCOPE(vm);
            auto result = goo(); // may throw.

            ... // Cleanup code that will are not affected by a pending exceptions.

            scope.release(); // tell the ThrowScope that the caller will handle the exception.
            return result;
        }
 
    b. This function is going to do a tail call that may throw.

        void foo(...)
        {
            auto scope = DECLARE_THROW_SCOPE(vm);
            ...
            scope.release(); // tell the ThrowScope that the caller will handle the exception.
            return goo(); // may throw.
        }
 
    ThrowScope::release() should not be used in the code paths that branch. For example:
 
        void foo(...)
        {
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto result = goo1(); // may throw.
            scope.release(); // <=================== the WRONG way !!!
            if (result)
                return;
 
            result = goo2(); // may throw.
            ...
            return result;
        }
 
    The above will result in a verification failure in goo2()'s ThrowScope.  The proper way
    to fix this verification is to do wither (6) or (7) below.
 
  6. Checking exceptions with ThrowScope::exception()
 
     ThrowScope::exception() returns the thrown Exception object if there is one pending.
     Else it returns nullptr.
 
     It also clears the m_needExceptionCheck bit thereby indicating that we've satisifed
     the needed exception check.
 
     This is how we do it:
 
        void foo(...)
        {
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto result = goo1(); // may throw.
            if (scope.exception())
                return;

            result = goo2(); // may throw.
            ...
            return result;
        }
 
    But sometimes, for optimization reasons, we may choose to test the result of the callee
    function instead doing a load of the VM exception value. See (7) below.
 
 7. Checking exception by checking callee result
 
    This approach should only be applied when it makes a difference to performance.
    If we need to do this, we should add an ASSERT() that invokes ThrowScope::exception()
    and verify the result. Since ThrowScope verification is only done on DEBUG builds,
    this ASSERT will satisfy the verification requirements while not impacting performance.
 
    This is how we do it:

        void foo(...)
        {
            auto scope = DECLARE_THROW_SCOPE(vm);

            bool failed = goo1(); // may throw.
            ASSERT(!!scope.exception() == failed)
            if (failed)
                return;

            result = goo2(); // may throw.
            ...
            return result;
        }
 
 8. Debugging verification failures.

    a. When verification fails, you will see a helpful message followed by an assertion failure.
       For example:
 
    FAILED exception check verification:
        Exception thrown from ThrowScope [2] Exit: setUpCall @ /Volumes/Data/ws6/OpenSource/Source/JavaScriptCore/llint/LLIntSlowPaths.cpp:1245
        is unchecked in ThrowScope [1]: varargsSetup @ /Volumes/Data/ws6/OpenSource/Source/JavaScriptCore/llint/LLIntSlowPaths.cpp:1398

       The message tells you that failure was detected at in varargsSetup() @ LLIntSlowPaths.cpp
       line 1398, and that the missing exception check should have happened somewhere between
       the call to setUpCall() @ LLIntSlowPaths.cpp line 1245 and it.

       If that is insufficient information, you can ...

    b. Turn on ThrowScope tracing
 
       Just set traceOn=true at the top of ThrowScope.cpp, and rebuild. Thereafter, you should
       see a trace of ThrowScopes being entered and exited as well as their depth e.g.

    ThrowScope [1] Enter: llint_slow_path_jfalse @ /Volumes/Data/ws6/OpenSource/Source/JavaScriptCore/llint/LLIntSlowPaths.cpp:1032
    ThrowScope [1] Exit: llint_slow_path_jfalse @ /Volumes/Data/ws6/OpenSource/Source/JavaScriptCore/llint/LLIntSlowPaths.cpp:1032

       You will also see traces of simulated throws e.g.

    ThrowScope [2] Throw from: setUpCall @ /Volumes/Data/ws6/OpenSource/Source/JavaScriptCore/llint/LLIntSlowPaths.cpp:1245

       If that is insufficient information, you can ...

    c. Turn on ThrowScope stack dumps

       Just set traceWithStackTraces=true at the top of ThrowScope.cpp, and rebuild.
       Thereafter, you should see a stack traces at various relevant ThrowScope events.

    d. Using throwScopePrintIfNeedCheck()

       If you have isolated the missing exception check to a function that is large but
       is unsure which statement can throw and is missing the check, you can sprinkle
       the function with calls to throwScopePrintIfNeedCheck().

       throwScopePrintIfNeedCheck() will log a line "Need exception check at ..." that
       inlcudes the file and line number only when it see the m_needExceptionCheck set.
       This will tell you which statement simulated the throw that is not being checked
       i.e. the one that preceded the throwScopePrintIfNeedCheck() that printed a line.
*/

ThrowScope::ThrowScope(VM& vm, ThrowScopeLocation location)
    : m_vm(vm)
    , m_previousScope(vm.m_topThrowScope)
    , m_location(location)
    , m_depth(m_previousScope ? m_previousScope->m_depth + 1 : 0)
{
    m_vm.m_topThrowScope = this;

    if (traceOn) {
        dataLog("<", traceCount++, "> ThrowScope [", m_depth, "] Enter: ", location.functionName, " @ ", location.file, ":", location.line);
        if (m_vm.m_needExceptionCheck)
            dataLog(", needs check");
        dataLog("\n");

        if (traceWithStackTraces)
            WTFReportBacktrace();
    }

    verifyExceptionCheckNeedIsSatisfied(Site::ScopeEntry);
}

ThrowScope::~ThrowScope()
{
    RELEASE_ASSERT(m_vm.m_topThrowScope);

    if (!m_isReleased)
        verifyExceptionCheckNeedIsSatisfied(Site::ScopeExit);
    else {
        // If we released the scope, that means we're letting our callers do the
        // exception check. However, because our caller may be a LLInt or JIT
        // function (which always checks for exceptions but won't clear the
        // m_needExceptionCheck bit), we should clear m_needExceptionCheck here
        // and let code below decide if we need to simulate a re-throw.
        m_vm.m_needExceptionCheck = false;
    }

    bool willBeHandleByLLIntOrJIT = false;
    void* previousScope = m_previousScope;
    void* topCallFrame = m_vm.topCallFrame;
    
    // If the topCallFrame was pushed on the stack after the previousScope was instantiated,
    // then this throwScope will be returning to LLINT or JIT code that always do an exception
    // check. In that case, skip the simulated throw because the LLInt and JIT will be
    // checking for the exception their own way instead of calling ThrowScope::exception().
    if (topCallFrame && previousScope > topCallFrame)
        willBeHandleByLLIntOrJIT = true;
    
    if (!willBeHandleByLLIntOrJIT)
        simulateThrow();

    if (traceOn) {
        dataLog("<", traceCount++, "> ThrowScope [", m_depth, "] Exit: ", m_location.functionName, " @ ", m_location.file, ":", m_location.line);
        if (!willBeHandleByLLIntOrJIT)
            dataLog(", with rethrow");
        if (m_vm.m_needExceptionCheck)
            dataLog(", needs check");
        dataLog("\n");

        if (traceWithStackTraces)
            WTFReportBacktrace();
    }

    m_vm.m_topThrowScope = m_previousScope;
}

void ThrowScope::throwException(ExecState* exec, Exception* exception)
{
    if (m_vm.exception() && m_vm.exception() != exception)
        verifyExceptionCheckNeedIsSatisfied(Site::Throw);
    
    m_vm.throwException(exec, exception);
}

JSValue ThrowScope::throwException(ExecState* exec, JSValue error)
{
    if (!error.isCell() || !jsDynamicCast<Exception*>(error.asCell()))
        verifyExceptionCheckNeedIsSatisfied(Site::Throw);
    
    return m_vm.throwException(exec, error);
}

JSObject* ThrowScope::throwException(ExecState* exec, JSObject* obj)
{
    if (!jsDynamicCast<Exception*>(obj))
        verifyExceptionCheckNeedIsSatisfied(Site::Throw);
    
    return m_vm.throwException(exec, obj);
}

void ThrowScope::printIfNeedCheck(const char* functionName, const char* file, unsigned line)
{
    if (m_vm.m_needExceptionCheck)
        dataLog("<", traceCount++, "> Need exception check at ", functionName, " @ ", file, ":", line, "\n");
}

void ThrowScope::simulateThrow()
{
    RELEASE_ASSERT(m_vm.m_topThrowScope);
    m_vm.m_simulatedThrowPointLocation = m_location;
    m_vm.m_simulatedThrowPointDepth = m_depth;
    m_vm.m_needExceptionCheck = true;

    if (traceOn) {
        dataLog("<", traceCount++, "> ThrowScope [", m_depth, "] Throw from: ", m_location.functionName, " @ ", m_location.file, ":", m_location.line, "\n");
        if (traceWithStackTraces)
            WTFReportBacktrace();
    }
}

void ThrowScope::verifyExceptionCheckNeedIsSatisfied(ThrowScope::Site site)
{
    if (!verificationOn)
        return;

    if (UNLIKELY(m_vm.m_needExceptionCheck)) {
        auto failDepth = m_vm.m_simulatedThrowPointDepth;
        auto& failLocation = m_vm.m_simulatedThrowPointLocation;

        auto siteName = [] (Site site) -> const char* {
            switch (site) {
            case Site::ScopeEntry:
                return "Entry";
            case Site::ScopeExit:
                return "Exit";
            case Site::Throw:
                return "Throw";
            }
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        };

        dataLog(
            "FAILED exception check verification:\n"
            "    Exception thrown from ThrowScope [", failDepth, "] ", siteName(site), ": ", failLocation.functionName, " @ ", failLocation.file, ":", failLocation.line, "\n"
            "    is unchecked in ThrowScope [", m_depth, "]: ", m_location.functionName, " @ ", m_location.file, ":", m_location.line, "\n"
            "\n");

        RELEASE_ASSERT(!m_vm.m_needExceptionCheck);
    }
}

#endif // ENABLE(THROW_SCOPE_VERIFICATION)
    
} // namespace JSC

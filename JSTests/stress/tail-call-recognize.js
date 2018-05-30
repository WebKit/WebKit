function callerMustBeRun() {
    if (!Object.is(callerMustBeRun.caller, runTests))
        throw new Error("Wrong caller, expected run but got ", callerMustBeRun.caller);
}

function callerMustBeStrict() {
    var errorThrown = false;
    try {
        callerMustBeStrict.caller;
    } catch (e) {
        errorThrown = true;
    }
    if (!errorThrown)
        throw Error("Wrong caller, expected strict caller but got ", callerMustBeStrict.caller);
}

function runTests() {
    // Statement tests
    (function simpleTailCall() {
        "use strict";
        return callerMustBeRun();
    })();

    (function noTailCallInTry() {
        "use strict";
        try {
            return callerMustBeStrict();
        } catch (e) {
            throw e;
        }
    })();

    (function tailCallInCatch() {
        "use strict";
        try { } catch (e) { return callerMustBeRun(); }
    })();

    (function tailCallInFinally() {
        "use strict";
        try { } finally { return callerMustBeRun(); }
    })();

    (function tailCallInFinallyWithCatch() {
        "use strict";
        try { } catch (e) { } finally { return callerMustBeRun(); }
    })();

    (function tailCallInFinallyWithCatchTaken() {
        "use strict";
        try { throw null; } catch (e) { } finally { return callerMustBeRun(); }
    })();

    (function noTailCallInCatchIfFinally() {
        "use strict";
        try { throw null; } catch (e) { return callerMustBeStrict(); } finally { }
    })();

    (function tailCallInFor() {
        "use strict";
        for (var i = 0; i < 10; ++i)
            return callerMustBeRun();
    })();

    (function tailCallInWhile() {
        "use strict";
        while (true)
            return callerMustBeRun();
    })();

    (function tailCallInDoWhile() {
        "use strict";
        do
            return callerMustBeRun();
        while (true);
    })();

    (function noTailCallInForIn() {
        "use strict";
        for (var x in [1, 2])
            return callerMustBeStrict();
    })();

    (function noTailCallInForOf() {
        "use strict";
        for (var x of [1, 2])
            return callerMustBeStrict();
    })();

    (function tailCallInIf() {
        "use strict";
        if (true)
            return callerMustBeRun();
    })();

    (function tailCallInElse() {
        "use strict";
        if (false) throw new Error("WTF");
        else return callerMustBeRun();
    })();

    (function tailCallInSwitchCase() {
        "use strict";
        switch (0) {
        case 0: return callerMustBeRun();
        }
    })();

    (function tailCallInSwitchDefault() {
        "use strict";
        switch (0) {
        default: return callerMustBeRun();
        }
    })();

    (function tailCallWithLabel() {
        "use strict";
        dummy: return callerMustBeRun();
    })();

    (function tailCallInTaggedTemplateString() {
        "use strict";
        return callerMustBeRun`test`;
    })();

    // Expression tests, we don't enumerate all the cases where there
    // *shouldn't* be a tail call

    (function tailCallComma() {
        "use strict";
        return callerMustBeStrict(), callerMustBeRun();
    })();

    (function tailCallTernaryLeft() {
        "use strict";
        return true ? callerMustBeRun() : unreachable();
    })();

    (function tailCallTernaryRight() {
        "use strict";
        return false ? unreachable() : callerMustBeRun();
    })();

    (function tailCallLogicalAnd() {
        "use strict";
        return true && callerMustBeRun();
    })();

    (function tailCallLogicalOr() {
        "use strict";
        return false || callerMustBeRun();
    })();

    (function memberTailCall() {
        "use strict";
        return { f: callerMustBeRun }.f();
    })();

    (function bindTailCall() {
        "use strict";
        return callerMustBeRun.bind()();
    })();

    // Function.prototype tests

    (function applyTailCall() {
        "use strict";
        return callerMustBeRun.apply();
    })();

    (function callTailCall() {
        "use strict";
        return callerMustBeRun.call();
    })();

    // No tail call for constructors
    (function noTailConstruct() {
        "use strict";
        return new callerMustBeStrict();
    })();
}

for (var i = 0; i < 10000; ++i)
    runTests();

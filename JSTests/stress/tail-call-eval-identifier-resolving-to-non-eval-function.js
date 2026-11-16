//@ defaultNoSamplingProfilerRun

// A call whose callee is spelled "eval" is only a direct eval when the identifier
// actually resolves to %eval%. Otherwise it is an ordinary call, so in a strict
// function it must be a tail call.

var iterations = 100000;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

(function functionScope() {
    var callCount = 0;
    function f(n) {
        "use strict";
        if (n === 0) {
            callCount += 1;
            return "done";
        }
        return eval(n - 1);
    }
    var eval = f;
    shouldBe(f(iterations), "done");
    shouldBe(callCount, 1);
})();

(function functionScopeDynamic() {
    var callCount = 0;
    function f(n) {
        "use strict";
        if (n === 0) {
            callCount += 1;
            return "done";
        }
        return eval(n - 1);
    }
    eval("var eval = f;");
    shouldBe(f(iterations), "done");
    shouldBe(callCount, 1);
})();

(function withScope() {
    var callCount = 0;
    var f, scope = {};
    with (scope) {
        f = function (n) {
            "use strict";
            if (n === 0) {
                callCount += 1;
                return "done";
            }
            return eval(n - 1);
        };
    }
    scope.eval = f;
    shouldBe(f(iterations), "done");
    shouldBe(callCount, 1);
})();

(function spreadArguments() {
    var callCount = 0;
    function f(n) {
        "use strict";
        if (n === 0) {
            callCount += 1;
            return "done";
        }
        return eval(...[n - 1]);
    }
    var eval = f;
    shouldBe(f(iterations), "done");
    shouldBe(callCount, 1);
})();

// A real direct eval in a tail position keeps direct eval semantics: it sees the
// caller's bindings and its own declarations do not escape into the caller.
(function realDirectEvalInTailPosition() {
    "use strict";
    var x = 42;
    function f() {
        "use strict";
        var y = "inner";
        return eval("[x, y, typeof z]");
    }
    shouldBe(f().join(), "42,inner,undefined");
})();

// An async generator body holds no tail positions, so "return eval(0)" must stay an ordinary
// call whose result the body then awaits.
(function asyncGeneratorScope() {
    var eval = function () { return Promise.resolve(42); };
    async function* g() {
        "use strict";
        return eval(0);
    }
    var result;
    g().next().then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.done, true);
    shouldBe(result.value, 42);
})();

// Global scope, run last because it clobbers the global eval binding.
var callCount = 0;
function globalF(n) {
    "use strict";
    if (n === 0) {
        callCount += 1;
        return "done";
    }
    return eval(n - 1);
}
eval = globalF;
shouldBe(globalF(iterations), "done");
shouldBe(callCount, 1);

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

(function testBasicUnusedCatch() {
    let executed = false;
    try {
        throw new Error("test");
    } catch (e) {
        executed = true;
    }
    shouldBe(executed, true, "testBasicUnusedCatch");
})();

(function testUnusedCatchWithReturn() {
    function f() {
        try {
            throw new Error("test");
        } catch (e) {
            return "caught";
        }
        return "not caught";
    }
    shouldBe(f(), "caught", "testUnusedCatchWithReturn");
})();

(function testUnusedCatchWithFinally() {
    let catchExecuted = false;
    let finallyExecuted = false;
    try {
        throw new Error("test");
    } catch (e) {
        catchExecuted = true;
    } finally {
        finallyExecuted = true;
    }
    shouldBe(catchExecuted, true, "testUnusedCatchWithFinally - catch");
    shouldBe(finallyExecuted, true, "testUnusedCatchWithFinally - finally");
})();

(function testUsedCatchVariable() {
    let message = null;
    try {
        throw new Error("expected message");
    } catch (e) {
        message = e.message;
    }
    shouldBe(message, "expected message", "testUsedCatchVariable");
})();

(function testCatchVariableInClosure() {
    let getter = null;
    try {
        throw "captured value";
    } catch (e) {
        getter = function() { return e; };
    }
    shouldBe(getter(), "captured value", "testCatchVariableInClosure");
})();

(function testCatchVariableWithEval() {
    let result = null;
    try {
        throw "eval test";
    } catch (e) {
        result = eval("e");
    }
    shouldBe(result, "eval test", "testCatchVariableWithEval");
})();

(function testCatchVariableWithEvalExpression() {
    let result = null;
    try {
        throw 42;
    } catch (e) {
        result = eval("e + 8");
    }
    shouldBe(result, 50, "testCatchVariableWithEvalExpression");
})();

(function testCatchVariableWithEvalInner() {
    let result = null;
    try {
        throw 42;
    } catch (e) {
        function foo() {
            result = eval("e + 8");
        }
        foo();
    }
    shouldBe(result, 50, "testCatchVariableWithEvalInner");
})();

(function testOuterVariableNotAffected() {
    let e = "outer";
    try {
        throw new Error("inner");
    } catch (e) {
    }
    shouldBe(e, "outer", "testOuterVariableNotAffected");
})();

(function testOuterVariableAccessible() {
    let outer = "accessible";
    let result = null;
    try {
        throw new Error("test");
    } catch (e) {
        result = outer;
    }
    shouldBe(result, "accessible", "testOuterVariableAccessible");
})();

(function testNestedTryCatch() {
    let innerCaught = false;
    try {
        try {
            throw new Error("inner");
        } catch (e) {
            innerCaught = true;
            throw new Error("rethrow");
        }
    } catch (e) {
    }
    shouldBe(innerCaught, true, "testNestedTryCatch");
})();

(function testNestedTryCatchSameName() {
    let values = [];
    try {
        try {
            throw "first";
        } catch (e) {
            values.push(e);
            throw "second";
        }
    } catch (e) {
        values.push(e);
    }
    shouldBe(values[0], "first", "testNestedTryCatchSameName - first");
    shouldBe(values[1], "second", "testNestedTryCatchSameName - second");
})();

(function testUnusedCatchInLoop() {
    let count = 0;
    for (let i = 0; i < 10000; i++) {
        try {
            throw new Error("loop");
        } catch (e) {
            count++;
        }
    }
    shouldBe(count, 10000, "testUnusedCatchInLoop");
})();

(function testUsedCatchInLoop() {
    let sum = 0;
    for (let i = 0; i < 1000; i++) {
        try {
            throw i;
        } catch (e) {
            sum += e;
        }
    }
    shouldBe(sum, 499500, "testUsedCatchInLoop");
})();

(function testMixedCatchInLoop() {
    let usedSum = 0;
    let unusedCount = 0;
    for (let i = 0; i < 1000; i++) {
        if (i % 2 === 0) {
            try {
                throw i;
            } catch (e) {
                usedSum += e;
            }
        } else {
            try {
                throw i;
            } catch (e) {
                unusedCount++;
            }
        }
    }
    shouldBe(usedSum, 249500, "testMixedCatchInLoop - usedSum");
    shouldBe(unusedCount, 500, "testMixedCatchInLoop - unusedCount");
})();

(function testUnderscoreNaming() {
    let executed = false;
    try {
        throw new Error("test");
    } catch (_) {
        executed = true;
    }
    shouldBe(executed, true, "testUnderscoreNaming");
})();

(function testUnderscoreUsed() {
    let result = null;
    try {
        throw "underscore value";
    } catch (_) {
        result = _;
    }
    shouldBe(result, "underscore value", "testUnderscoreUsed");
})();

(function testThrowNonError() {
    let results = [];
    try { throw null; } catch (e) { results.push("null"); }
    try { throw undefined; } catch (e) { results.push("undefined"); }
    try { throw 0; } catch (e) { results.push("zero"); }
    try { throw ""; } catch (e) { results.push("empty string"); }
    try { throw false; } catch (e) { results.push("false"); }
    shouldBe(results.length, 5, "testThrowNonError");
})();

(function testCatchModifiesOuter() {
    let value = "before";
    try {
        throw new Error("test");
    } catch (e) {
        value = "after";
    }
    shouldBe(value, "after", "testCatchModifiesOuter");
})();

(function testSequentialCatches() {
    let count = 0;
    try { throw 1; } catch (a) { count++; }
    try { throw 2; } catch (b) { count++; }
    try { throw 3; } catch (c) { count++; }
    shouldBe(count, 3, "testSequentialCatches");
})();

(function testStrictModeUnused() {
    "use strict";
    let executed = false;
    try {
        throw new Error("strict");
    } catch (e) {
        executed = true;
    }
    shouldBe(executed, true, "testStrictModeUnused");
})();

(function testStrictModeWithEval() {
    "use strict";
    let result = null;
    try {
        throw "strict eval";
    } catch (e) {
        result = eval("e");
    }
    shouldBe(result, "strict eval", "testStrictModeWithEval");
})();

(async function testAsyncUnusedCatch() {
    let executed = false;
    try {
        throw new Error("async");
    } catch (e) {
        executed = true;
    }
    shouldBe(executed, true, "testAsyncUnusedCatch");
})();

(async function testAsyncUsedCatch() {
    let message = null;
    try {
        throw new Error("async message");
    } catch (e) {
        message = e.message;
    }
    shouldBe(message, "async message", "testAsyncUsedCatch");
})();

(function testGeneratorUnusedCatch() {
    function* gen() {
        try {
            throw new Error("gen");
        } catch (e) {
            yield "caught";
        }
    }
    let g = gen();
    shouldBe(g.next().value, "caught", "testGeneratorUnusedCatch");
})();

(function testGeneratorUsedCatch() {
    function* gen() {
        try {
            throw "gen value";
        } catch (e) {
            yield e;
        }
    }
    let g = gen();
    shouldBe(g.next().value, "gen value", "testGeneratorUsedCatch");
})();

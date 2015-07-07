// Case 1: Throw and print stack:
testId++;
    try { throw new Error(); } catch (e) { printStack(e.stack); }

// Case 2: Same program as Case 1 but indented.
testId++;
        try { throw new Error(); } catch (e) { printStack(e.stack); }

// Case 3: Same program indented on the same line.
testId++;
try { throw new Error(); } catch (e) { printStack(e.stack); }   try { throw new Error(); } catch (e) { printStack(e.stack); }

// Case 4: Stack with 2 function frames.
testId++;
try {
    function doThrow4b() { throw new Error(); }
    doThrow4b();
} catch(e) {
    printStack(e.stack);
}

// Case 5: Function wrapping a Function.
testId++;
function doThrow5b() { try { function innerFunc() { throw new Error(); } innerFunc(); } catch (e) { printStack(e.stack); }}; doThrow5b();

// Case 6: Same inner function body as Case 5.
testId++;
function doThrow6b() { try { function innerFunc() { throw new Error(); } innerFunc(); } catch (e) { printStack(e.stack); }}; doThrow6b();

// Case 7: Case 1 redone with a Function Expression.
testId++;
    try { (function () { throw new Error(); })(); } catch (e) { printStack(e.stack); }

// Case 8: Case 2 redone with a Function Expression.
testId++;
        try { (function () { throw new Error(); })(); } catch (e) { printStack(e.stack); }

// Case 9: Case 3 redone with a Function Expression.
testId++;
try { (function () { throw new Error(); })(); } catch (e) { printStack(e.stack); }   try { (function () { throw new Error(); })(); } catch (e) { printStack(e.stack); }

// Case 10: Function Expression as multiple lines.
testId++;
try {
    (function () {
        throw new Error();
     })();
} catch(e) {
    printStack(e.stack);
}

// Case 11: Case 4 redone with a Function wrapping Function Expression.
testId++;
try {
    function doThrow11b() {
        (function () { throw new Error(); })();
    }
    doThrow11b();
} catch(e) {
    printStack(e.stack);
}

// Case 12: A Function Expression wrapping a Function Expression.
testId++;
try { (function () {(function () { throw new Error(); })();})(); } catch (e) { printStack(e.stack); }

// Case 13: Same function body as Case 12.
testId++;
try { (function () {(function () { throw new Error(); })();})(); } catch (e) { printStack(e.stack); }

// Case 14: Function Expression in a Function Expression in a Function.
testId++;
try { function doThrow14b() {(function () { (function () { throw new Error(); })();})();} doThrow14b(); } catch (e) { printStack(e.stack); }

// Case 15: Throw in an Eval.
testId++;
eval("try { throw new Error(); } catch(e) { printStack(e.stack); }");

// Case 16: Multiple lines in an Eval.
testId++;
eval("\n" +
"try {\n" +
"    function doThrow15b() { throw new Error(); }\n" +
"    doThrow15b();\n" +
"} catch(e) {\n" +
"    printStack(e.stack);\n" +
"}\n" +
"");

// Case 17: Function Expression in an Eval.
testId++;
eval("try { (function () { throw new Error(); })(); } catch(e) { printStack(e.stack); }");

// Case 18: Multiple lines with a Function Expression in an Eval.
testId++;
eval("\n" +
"try {\n" +
"    (function () { throw new Error(); })();\n" +
"} catch(e) {\n" +
"    printStack(e.stack);\n" +
"}\n" +
"");

// Case 19: Binary op with type coersion on strcat.
testId++;
try {
    testObj19b = {
        toString: function() {
            var result = ("Hello " + "World") + this;
            b19 = 5;
            return result;
        },
        run: function() {
            return testObj19b.toString();
        }
    };
    testObj19b.run();
} catch(e) {
    printStack(e.stack);
}

// Case 20: BinaryOp with type coersion on comparison.
testId++;
try {
    function test20b() {
        var f = function g() {
            if (this != 10) f();
        };
        var a = f();
    }

    test20b();
} catch(e) {
    printStack(e.stack);
}

// Case 21: Regression test from https://bugs.webkit.org/show_bug.cgi?id=118662
testId++;
try {
    function toFuzz21b() {
        if (PriorityQueue.prototype.doSort() instanceof (this ^= function() {
        })) return 2; 
    }
    toFuzz21b();
} catch(e) {
    printStack(e.stack);
}

// Case 22: Regression test from https://bugs.webkit.org/show_bug.cgi?id=118664
testId++;
try {
    function toFuzz22b() {
        var conf = new ConfigObject({})
        for (conf in str1.localeCompare) {
        }
    }
    toFuzz22b();
} catch(e) {
    printStack(e.stack);
}

successfullyParsed = true;

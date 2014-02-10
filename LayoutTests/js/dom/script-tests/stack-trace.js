if (!this.alert) {
    debug = print;
    description = print;
}

description(
'This test checks stack trace corectness in special cases.'
);

function stackTraceLineFor(stackTrace, frameIndex) {
    var i = frameIndex;
    var indexOfAt = stackTrace[i].indexOf('@')
    var indexOfLastSlash = stackTrace[i].lastIndexOf('/');
    if (indexOfLastSlash == -1)
        indexOfLastSlash = indexOfAt
    var functionName = stackTrace[i].substring(0, indexOfAt);
    var fileName = stackTrace[i].substring(indexOfLastSlash + 1);
    return functionName + " at " + fileName;
}

function printStack(stackTrace) {
    debug("--> Stack Trace:")
    stackTrace = stackTrace.split("\n");
    var length = Math.min(stackTrace.length, 100);
    for (var i = 0; i < length; i++)
        debug("    " + i + "   " + stackTraceLineFor(stackTrace, i));
    debug('');
}

function dumpPattern(pattern) {
    for (var i = 0; i < pattern.length; i++)
        debug("    " + i + "   " + pattern[i]);
}

function matchesPatternAtLine(pattern, patternIndex, traceLine) {
    var patternLine = pattern[patternIndex];
    return traceLine.slice(0, patternLine.length) == patternLine;
}

function matchPattern(pattern, traceLine) {
    for (var i = 0; i < pattern.length; i++) {
        if (matchesPatternAtLine(pattern, i, traceLine))
            return i;
    }
    return -1;
}

function checkStackForPattern(stackTrace, pattern) {
    stackTrace = stackTrace.split("\n");
    var length = Math.min(stackTrace.length, 100);

    // Get the match in the pattern for the first line:
    var firstStackTraceLine = stackTraceLineFor(stackTrace, 0);
    var patternIndex = matchPattern(pattern, firstStackTraceLine);
    if (patternIndex < 0) {
        debug("--> Stack Trace FAILED to match pattern:")
        dumpPattern(pattern);
        debug('');
        return;
    }

    for (var i = 1; i < length; i++) {
        patternIndex = ++patternIndex % pattern.length;
        var traceLine = stackTraceLineFor(stackTrace, i);
        if (!matchesPatternAtLine(pattern, patternIndex, traceLine)) {
            debug("--> Stack Trace FAILED to match pattern:")
            dumpPattern(pattern);
            debug('');
            return;
        }
    }

    debug("--> Stack Trace matches pattern:")
    dumpPattern(pattern);
    debug('');
}

function hostThrower() { Element.prototype.appendChild.call({ }, [{ }]);  }
function callbacker(f) { [0].map(f); }
function outer(errorName) { inner(errorName); }
function inner(errorName) { throw new Error("Error in " + errorName); }
function evaler(code) { eval(code); }
function normalOuter() { normalInner(); }
function normalInner() { if(thisVarDoesntExist) failIfTrue("shouldFailBeforeThis") };
function scripterInner() { htmlInner(); }
function scripterOuter() { htmlOuter(); }
                                                                       // Expected functions in stack trace
// Normal Case
try { normalOuter() } catch (e) { printStack(e.stack) }                     // normalOuter -> normalInner

// Eval Case
try { evaler("inner('inner eval');"); } catch (e) { printStack(e.stack) }   // evaler -> eval -> inner
try { evaler("outer('outer eval');"); } catch (e) { printStack(e.stack) }   // evaler -> eval -> outer -> inner

// Function Callback Case
try { callbacker(inner('inner map')); } catch (e) { printStack(e.stack); }   // callbacker -> map -> inner
try { callbacker(outer('outer map')); } catch (e) { printStack(e.stack); }   // callbacker -> map -> outer -> inner

// Host Code Case
try { hostThrower(); } catch (e) { printStack(e.stack); }                    // hostThrower

try { scripterInner(); } catch (e) { printStack(e.stack) }                   // program -> scripter -> inner
try { scripterOuter(); } catch (e) { printStack(e.stack) }                   // program -> scripter -> outer -> inner

function selfRecursive1() { selfRecursive1();
}


try { selfRecursive1(); } catch (e) { printStack(e.stack) }                   // selfRecursive1 -> selfRecursive1 -> selfRecursive1 -> selfRecursive1 ...

function selfRecursive2() {
    // A little work to make the DFG kick in
    for (var i = 0; i < 10; i++) {
        if (i == 9)
            selfRecursive2(); 
    }
}

try { selfRecursive2(); } catch (e) { printStack(e.stack) }                   // selfRecursive2 -> selfRecursive2 -> selfRecursive2 -> selfRecursive2 ...

function selfRecursive3() {
    eval("selfRecursive3()");
}

try {
    selfRecursive3();
} catch (e) {
    var pattern = [
        " at eval code",
        "eval at [native code]",
        "selfRecursive3 at stack-trace.js"
    ];
    checkStackForPattern(e.stack, pattern);
}

var callCount = 0;

function throwError() {
    throw {};
}

var object = {
    get getter1() {
        var o = {
            valueOf: function() {
                throwError()
            }
        };
        +o;
    },
    get getter2() {
        var o = {
            valueOf: throwError
        };
        +o;
    },
    get getter3() {
        var o1 = {
            valueOf: throwError
        };
        var o2 = {
            valueOf: function () {
                throwError();
            }
        };
        if (callCount == 9998)
            +o1;
        if (callCount == 9999)
            +o2;
    },
    nonInlineable : function () {
        if (0) return [arguments, function(){}];
        ++callCount;
        if (callCount == 1) {
            this.getter1;
        } else if (callCount == 2) {
            this.getter2;
        } else {
            this.getter3;
        }
    },
    inlineable : function () {
        this.nonInlineable();
    }
}

function yetAnotherInlinedCall(o) {
    o.inlineable();
}

function makeInlinableCall(o) {
    for (var i = 0; i < 10000; i++) {
        new yetAnotherInlinedCall(o);
    }
}
for (var k = 0; k < 4; k++) {
    try {
        function g() {
            var j = 0;
            for (var i = 0; i < 1000; i++) {
                j++;
                makeInlinableCall(object);
            }
        }
        [1].map(g);
    } catch (e) {
        printStack(e.stack);
    }
}

function h() {
    if (callCount++ == 1000)
        throw {};
    if (callCount > 1000) {
        [].map.apply(undefined, throwError);
    }
}

function mapTest(a) {
    a.map(h);
}

function mapTestDriver() {
    var a = [1,2,3];
    for (var i = 0; i < 2000; i++)
        mapTest(a);
}

try {
    callCount = 0;
    mapTestDriver()
} catch(e) {
    printStack(e.stack);
}

try {
    mapTestDriver()
} catch(e) {
    printStack(e.stack);
}

var dfgFunctionShouldThrow = false;
function dfgFunction() { 
    if (dfgFunctionShouldThrow) { 
        dfgFunctionShouldThrow = false; 
        throwError();
    }
}

for (var k = 0; k < 1000; k++)
    dfgFunction();

try {
    dfgFunctionShouldThrow = true;
    [1,2,3,4].map(dfgFunction);
} catch (e) {
    printStack(e.stack);
}

try { 
    var o = { };
    o.__defineGetter__("g", dfgFunction);
    function f(o) {
        o.g;
    }
    for (var k = 0; k < 1000; k++)
        f(o);
    
    dfgFunctionShouldThrow = true;
    f(o);

} catch (e) {
    printStack(e.stack);
}

var someValue = null;

function callNonCallable() {
    someValue();
}

for (var i = 0; i < 100; i++) {
    try {
        callNonCallable();
    } catch (e) {
    }
}

function dfgTest(f) {
    dfgCount = 0;
    while (dfgCount++ < 1000) {
        try {
            f();
        } catch (e) {
            printStack(e.stack)
            return;
        }
    }
}

function inlineableThrow() {
    if (dfgCount > 500) throw {};
}

var dfgThing = {
    get willThrow() {
        if (dfgCount > 500)
            throw {};
    },
    get willThrowEventually() {
        inlineableThrow();
    },
    willThrowFunc: function () { if (dfgCount > 500) throw {}; },
    willThrowEventuallyFunc: function () { inlineableThrow(); }
}
dfgThing.__defineGetter__("hostWillThrow", hostThrower);

function dfg1() {
    dfgThing.willThrow
}

function dfg2() {
    dfg1();
}

function dfg3() {
    dfg2();
}

function dfg4() {
    dfgThing.willThrowFunc();
}

function dfg5() {
    dfg4();
}

function dfg6() {
    dfg5();
}

function dfg7() {
    dfgThing.willThrowEventually
}

function dfg8() {
    dfg7();
}

function dfg9() {
    dfg8();
}

function dfga() {
    dfgThing.willThrowEventuallyFunc();
}

function dfgb() {
    dfga();
}

function dfgc() {
    dfgb();
}

dfgTest(dfg3)
dfgTest(dfg6)
dfgTest(dfg9)
dfgTest(dfgc)

successfullyParsed = true;

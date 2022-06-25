//@ skip if $buildType == "debug"
//@ requireOptions("--exceptionStackTraceLimit=0", "--defaultErrorStackTraceLimit=0", "--forceRAMSize=1000000", "--forceDebuggerBytecodeGeneration=1", "--useZombieMode=1", "--jitPolicyScale=0", "--collectContinuously=1", "--useConcurrentJIT=0")

function assert(b) {
    if (!b)
        throw new Error('aa');
}

var exception;
try {
    let target = function (x, y) {
        const actual = '' + x;
        target(x);
    };
    let handler = {
        apply: function (theTarget, thisArg, argArray) {
            return theTarget.apply([], argArray);
        }
    };
    let proxy = new Proxy(target, handler);
    assert(proxy(new String("10"), new String("20")) === 'foo');
} catch(e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";

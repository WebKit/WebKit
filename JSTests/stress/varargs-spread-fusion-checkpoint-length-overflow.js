//@ skip if $buildType == "debug" or $memoryLimited
//@ runDefault("--useConcurrentJIT=0")

// The bug needs the DFG to inline the call, so the caller must stay under maximumInliningCallerBytecodeCost.
// The spreads share one copy-on-write butterfly, so 2^22 elements each reach 2^32 arguments cheaply.

function callee() { return arguments.length; }

var caller = new Function("a", "return callee(" + Array(1024).fill("...a").join(", ") + ");");
noInline(caller);

for (var i = 0; i < testLoopCount; ++i)
    caller([]);

// Literals in eval code are not copy-on-write, so build this one as function code.
var big = new Function("return [" + Array(1 << 22).fill('"x"').join(",") + "];")();

var error = null;
try {
    caller(big);
} catch (e) {
    error = e;
}
if (!(error instanceof RangeError))
    throw new Error("Expected RangeError, got " + error);

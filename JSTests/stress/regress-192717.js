//@ skip if $memoryLimited or $buildType == "debug"
//@ runDefault("--useLLInt=false", "--forceCodeBlockToJettisonDueToOldAge=true", "--maxPerThreadStackUsage=200000", "--exceptionStackTraceLimit=1", "--defaultErrorStackTraceLimit=1")

let foo = 'let a';
for (let i = 0; i < 400000; i++)
  foo += ',a' + i;

var exception;
try {
    new Function(foo)();
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";

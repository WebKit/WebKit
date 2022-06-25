//@ requireOptions("--maxPerThreadStackUsage=153600", "--exceptionStackTraceLimit=0", "--defaultErrorStackTraceLimit=0")

function foo() {
  JSON.stringify();
  foo();
}

try {
    foo();
} catch (e) {
    if (e != "RangeError: Maximum call stack size exceeded.")
        throw e;
}

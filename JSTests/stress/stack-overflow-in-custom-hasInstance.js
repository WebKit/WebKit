//@ requireOptions("--maxPerThreadStackUsage=147456", "--exceptionStackTraceLimit=1", "--defaultErrorStackTraceLimit=1")

function f() {}

var fn = f;
for (var i = 0; i < 100000; ++i) {
    fn = fn.bind();

    // Ensure we don't fallback to @@hasInstance from %FunctionPrototype%.
    Object.defineProperty(fn, Symbol.hasInstance, {
        value: undefined, writable: true, enumerable: true, writable: true
    });

    // Prevent generating overlong names of the form "bound bound bound [...] f".
    Object.defineProperty(fn, "name", {
        value: "", writable: true, enumerable: true, writable: true
    });
}

var exception;
try {
    ({}) instanceof fn;
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";

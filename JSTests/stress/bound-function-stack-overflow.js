// This test verifies that we check for out of stack errors from recursively bound functions.
// It should exit without any output.

let expectedException = "RangeError: Maximum call stack size exceeded.";
let actualException = false;

function foo()
{
}

for (var i = 0; i < 6000; ++i) {
    foo = foo.bind(undefined, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
    Object.defineProperty(foo, "name", { value: "bar", writable: true, enumerable: true, writable: true });
}

try {
    foo("x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", 
        "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x", "x");
} catch (e) {
    actualException = e;
}

if (!actualException)
    throw "Expected \"" + expectedException + "\" exception, but no exceptoion was thrown";
else if (actualException != expectedException)
    throw "Expected \"" + expectedException + "\", but got \"" + actualException +"\"";

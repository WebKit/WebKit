//@ requireOptions("--exceptionStackTraceLimit=0", "--defaultErrorStackTraceLimit=0")

let arr0 = [];
var afterFirstCatch = false;

function foo(arg0) {
    var exception;
    let arr1 = [];
    arg0.__proto__ = arr1;
    try {
        foo(arr1);
    } catch (e) {
        // This afterFirstCatch tracking is just to facilitate being able to end this
        // test quickly without having to run the for-in loop below on the entire return
        // path.
        if (afterFirstCatch)
            throw e;
        afterFirstCatch = true;
        exception = e;
    }
    for (let q in arr0) { }
    if (afterFirstCatch)
        throw exception; // We're done with the test. Let's end this quickly.
}

try {
    foo(arr0);
} catch (e) {
    if (e != "RangeError: Maximum call stack size exceeded.")
        throw e;
}

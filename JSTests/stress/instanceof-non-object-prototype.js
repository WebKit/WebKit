let base = "sting";
let constructor = function() { };
constructor.prototype = 42;

function test(a, b) {
    return a instanceof b;
}
noInline(test);

for (let i = 0; i < 10000; i++) {
    let exception;
    try {
        var result = test(base, constructor);
    } catch (e) {
        exception = e;
    }
    if (exception)
        throw new Error("Threw an exception: " + exception);
    if (result !== false)
        throw new Error("instanceof returned: " + result);
}

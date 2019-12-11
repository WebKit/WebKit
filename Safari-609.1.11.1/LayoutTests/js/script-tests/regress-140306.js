description(
"Regression test for https://webkit.org/b/140306. This test should run without any exceptions."
);

testArgs = [ 1, "Second", new Number(3) ];

function checkArgs(a0, a1, a2) {
    if (a0 !== testArgs[0])
        throw "Value of declared arg a0 is wrong.  Should be: " + testArgs[0] + ", was: " + a0;

    if (a1 !== testArgs[1])
        throw "Value of declared arg a1 is wrong.  Should be: " + testArgs[1] + ", was: " + a1;

    if (a2 !== testArgs[2])
        throw "Value of declared arg a2 is wrong.  Should be: " + testArgs[2] + ", was: " + a2;

    if (arguments.length != 3)
        throw "Length of arguments is wrong.  Should be: 3, was: " + arguments.length;

    for (var i = 0; i < arguments.length; i++) {
        if (arguments[i] !== testArgs[i])
            throw "Value of arguments[" + i + "] is wrong.  Should be: " + testArgs[i] + ", was: " + arguments[i];
    }
}

function applyToArgs() {
    arguments = testArgs;

    checkArgs.apply(this, arguments)

    try { } catch (e) { throw e; }  // To force the creation of an activation object
}

applyToArgs(42);

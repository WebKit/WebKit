// Try a few ways to exhaust machine stack.

function f()
{
    arguments.callee.call();
}

function g()
{
    eval("g()");
}

try {
    f();
} catch (ex) {
    try {
        g();
    } catch (ex) {
        postMessage(ex.toString());
    }
}

// WARNING: some tests blindly set breakpoints in this file by line number.
// So, if you modify the code, make sure to adjust any createSourceCodeLocation
// calls from tests in the ../ directory. Callsites should include a description
// of the function/statement to set a breakpoint at, so that it's easy to fix them.

function callFunction(fn)
{
    if (!(fn instanceof Function))
        return;

    var argsArray = Array.prototype.slice.call(arguments);
    Array.prototype.splice.call(argsArray, 0, 1);
    fn.call(this, argsArray);
}

function hook()
{
    return 42;
}

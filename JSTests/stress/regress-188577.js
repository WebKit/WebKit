//@ requireOptions("--maxPerThreadStackUsage=262144")

var exception;
try {
    var i = 25000;
    var args = [];
    var v3;
    while (i--)
        args[i] = "a";
    var argsList = args.join();
    setter = Function(argsList, "");
    Object.defineProperty(args, '0', {set: setter});
    args.sort();

} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";

//@ requireOptions("--maxPerThreadStackUsage=262144")

var exception;
try {
    var args = new Array(25000).fill("a");
    Object.defineProperty(args, "0", {
        set: new Function(args.join(), ""),
    });
    args.unshift(1);
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Maximum call stack size exceeded.")
    throw "FAILED";

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var args = [];
for (var i = 0; i < 20; i++) {
  args[i] = args.toLocaleString() + "x" + i;
}
var min = new Function(args, "return Math.min(" + args.join(",") + ");");
shouldThrow(() => {
    min(0);
}, `RangeError: Maximum call stack size exceeded.`);

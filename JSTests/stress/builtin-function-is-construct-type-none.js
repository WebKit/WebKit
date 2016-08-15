function shouldThrow(func, message) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown.");
    if (String(error) !== message)
        throw new Error("bad error: " + String(error));
}

for (var i = 0; i < 10000; ++i) {
    shouldThrow(function () {
        new Array.prototype.forEach(function () { });
    }, "TypeError: function is not a constructor (evaluating 'new Array.prototype.forEach(function () { })')");
}

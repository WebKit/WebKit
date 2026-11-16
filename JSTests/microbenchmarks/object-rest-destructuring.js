(function() {
    var obj = { a: 0, b: 0, c: 0, d: 1, e: 0, f: 0, g: 0, h: 0, i: 0, j: 0 };
    var acc = 0;

    for (var i = 0; i < 1e5; i++) {
        var { a, b, c, ...rest } = obj;
        acc += rest.d;
    }

    if (acc !== 1e5)
        throw new Error("Bad assertion!");
})();

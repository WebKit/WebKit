(function() {
    var obj = { a: 1, b: 2, c: 3, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0, j: 0 };
    var acc = 0;

    for (var i = 0; i < 1e5; i++)
        acc += ({ a: 0, b: 0, c: 0, ...obj }).a;

    if (acc !== 1e5)
        throw new Error("Bad assertion!");
})();

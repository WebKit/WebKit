(function() {
    var obj = { a: 1, b: 2, c: 3, d: 4, e: 5 };
    var acc = 0;

    for (var i = 0; i < 1e6; i++)
        acc += ({ ...obj }).a;

    if (acc !== 1e6)
        throw new Error("Bad assertion!");
})();

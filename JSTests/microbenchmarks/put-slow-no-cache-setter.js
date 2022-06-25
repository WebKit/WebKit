(function() {
    var foo = 0;
    var base = {
        set foo0(v) { foo = v; },
        set foo1(v) { foo = v; },
        set foo2(v) { foo = v; },
        set foo3(v) { foo = v; },
        set foo4(v) { foo = v; },
    };

    for (var j = 0; j < 3e5; j++)
        base[`foo${j % 5}`] = j;

    if (foo !== j - 1)
        throw new Error("Bad assertion!");
})();

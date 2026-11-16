(function(key1, key2, key3) {
    var obj = { a: 0, b: 0, c: 0, d: 1, e: 0, f: 0, g: 0, h: 0, i: 0, j: 0 };
    var acc = 0;

    for (var i = 0; i < 1e5; i++) {
        var { [key1]: a, [key2]: b, [key3]: c, ...rest } = obj;
        acc += rest.d;
    }

    if (acc !== 1e5)
        throw new Error("Bad assertion!");
})('a', 'b', 'c');

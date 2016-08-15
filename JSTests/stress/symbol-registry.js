function test(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

(function () {
    var hello = Symbol("Hello");
    var proto = Symbol("__proto__");

    for (var sym of [ hello, proto, Symbol.iterator ]) {
        var key = Symbol.keyFor(sym);
        test(key, undefined);
        // twice
        var key = Symbol.keyFor(sym);
        test(key, undefined);
    }
}());

(function () {
    var keys = [
        "Hello",
        "__proto__",
        "Symbol.iterator",
        '',
        null,
        undefined,
        42,
        20.5,
        -42,
        -20.5,
        true,
        false,
        {},
        function () {},
        [],
    ];
    for (var key of keys) {
        var sym = Symbol.for(key);
        test(typeof sym, "symbol");
        test(sym.toString(), "Symbol(" + String(key) + ")");

        var sym2 = Symbol.for(key);
        test(sym === sym2, true);

        var key = Symbol.keyFor(sym);
        test(key, key);
        var key = Symbol.keyFor(sym2);
        test(key, key);
    }
}());

(function () {
    var error = null;
    try {
        var key = {
            toString() {
                throw new Error('toString');
            }
        };
        Symbol.for(key);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('not thrown');
    if (String(error) !== 'Error: toString')
        throw new Error('bad error: ' + String(error));
}());

(function () {
    var elements = [
        null,
        undefined,
        42,
        20.5,
        true,
        false,
        'string',
        {},
        function () {},
        [],
    ];
    for (var item of elements) {
        var error = null;
        try {
            Symbol.keyFor(item);
        } catch (e) {
            error = e;
        }
        if (!error)
            throw new Error('not thrown');
        if (String(error) !== 'TypeError: Symbol.keyFor requires that the first argument be a symbol')
            throw new Error('bad error: ' + String(error));
    }
}());

(function () {
    for (var i = 0; i < 10000; ++i)
        Symbol.for(i);
    gc();
}());

(function () {
    for (var i = 0; i < 100; ++i) {
        var symbol = Symbol.for(i);
        test(String(symbol), "Symbol(" + i + ")");
        test(symbol, Symbol.for(i));
        gc();
    }
    gc();
}());

(function () {
    var symbols = [];
    for (var i = 0; i < 100; ++i) {
        var symbol = Symbol.for(i);
        symbols.push(symbol);
    }

    for (var i = 0; i < 100; ++i)
        test(Symbol.for(i), symbols[i]);

    for (var i = 0; i < 100; ++i)
        test(Symbol.keyFor(Symbol(i)), undefined);
}());

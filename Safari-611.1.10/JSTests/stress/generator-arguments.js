function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

(function () {
    function *g1(a, b, c)
    {
        yield arguments;
        yield arguments;
    }

    var g = g1(0, 1, 2);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);

    function *g2(a, b, c)
    {
        yield arguments;
        yield arguments;
        a = yield a;
        yield arguments;
        b = yield b;
        yield arguments;
        c = yield c;
        yield arguments;
    }
    var g = g2(0, 1, 2);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 0);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":42,"1":1,"2":2}`);
    shouldBe(g.next().value, 1);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":42,"1":42,"2":2}`);
    shouldBe(g.next().value, 2);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":42,"1":42,"2":42}`);
}());

(function () {
    function *g1(a, b, c)
    {
        "use strict";
        yield arguments;
        yield arguments;
    }

    var g = g1(0, 1, 2);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);

    function *g2(a, b, c)
    {
        "use strict";
        yield arguments;
        yield arguments;
        a = yield a;
        yield arguments;
        b = yield b;
        yield arguments;
        c = yield c;
        yield arguments;
    }
    var g = g2(0, 1, 2);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 0);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 1);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 2);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":0,"1":1,"2":2}`);
}());

(function () {
    "use strict";
    function *g1(a, b, c)
    {
        yield arguments;
        yield arguments;
    }

    var g = g1(0, 1, 2);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);

    function *g2(a, b, c)
    {
        yield arguments;
        yield arguments;
        a = yield a;
        yield arguments;
        b = yield b;
        yield arguments;
        c = yield c;
        yield arguments;
    }
    var g = g2(0, 1, 2);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(JSON.stringify(g.next().value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 0);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 1);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":0,"1":1,"2":2}`);
    shouldBe(g.next().value, 2);
    shouldBe(JSON.stringify(g.next(42).value), `{"0":0,"1":1,"2":2}`);
}());

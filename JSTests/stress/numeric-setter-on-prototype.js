function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

class Trace {
    constructor()
    {
        this.__count = 0;
    }

    trace()
    {
        this.__count++;
    }

    get count()
    {
        return this.__count;
    }
}

for (var i = 0; i < 10000; ++i) {
    var t0 = new Trace();

    shouldBe(t0.count, 0);
    var z = {__proto__: {set 3(x) { t0.trace() }}};
    shouldBe(t0.count, 0);
    z[3] = 7;
    shouldBe(t0.count, 1);
}

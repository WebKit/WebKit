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
    var t3 = new Trace();

    var object = { 2: 2, 4: 4 };
    shouldBe(t3.count, 0);
    var a = {__proto__: object };
    shouldBe(t3.count, 0);
    Object.defineProperty(object, 3, {
        set: function (x) { t3.trace() }
    });
    a[3] = 7;
    shouldBe(t3.count, 1);
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {};
object[42] = 42;
object[43] = function tag() { return 42; };

shouldBe(object['43']`Hello`, 42);


class Hello {
    constructor()
    {
        this['44'] = 42;
        shouldBe(this['42'], 42);
        shouldBe(this['43'](), 42);
        shouldBe(this['44'], 42);
    }

    get 42()
    {
        return 42;
    }

    43()
    {
        return 42;
    }
}

class Derived extends Hello {
    constructor()
    {
        super();
        shouldBe(super['42'], 42);
        shouldBe(super['43'](), 42);
        shouldBe(this['44']++, 42);
        shouldBe(++this['44'], 44);
    }
}

var derived = new Derived();

var test = { 42: '' };

for (test['42'] in { a: 'a' })
    shouldBe(test['42'], 'a');
shouldBe(test['42'], 'a');

for (test['42'] of [ 'b' ])
    shouldBe(test['42'], 'b');
shouldBe(test['42'], 'b');

{
    let { '42': a } = { '42': '42' };
    shouldBe(a, '42');
}

{
    let object = { 42: 42 };
    let objectAlias = object;
    object['42'] = (object = 30);
    shouldBe(objectAlias['42'], 30);
}

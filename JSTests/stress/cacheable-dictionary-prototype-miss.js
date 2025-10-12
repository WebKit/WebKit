function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

class Base {
    constructor() {
    }

    test() { return 42; }
}

class Middle extends Base {
}

class Derived0 extends Middle {
    constructor() {
        super();
    }
}

class Derived1 extends Middle {
    constructor() {
        super();
    }

    ok() { }
}

class Derived2 extends Middle {
    constructor() {
        super();
    }

    test() { return 42; }
}

let array = [];
for (let i = 0; i < 10; ++i)
    array.push(new Derived0);
for (let i = 0; i < 10; ++i)
    array.push(new Derived1);
for (let i = 0; i < 10; ++i)
    array.push(new Derived2);

$vm.toCacheableDictionary(Middle.prototype);

function test(derived)
{
    return derived.test();
}
noInline(test);

for (let i = 0; i < 100; ++i) {
    for (let derived of array)
        shouldBe(test(derived), 42);
}

$vm.toCacheableDictionary(Middle.prototype);
Middle.prototype.test = function () { return 20; }
Derived2.prototype.test = function () { return 20; }

for (let i = 0; i < 1e3; ++i) {
    for (let derived of array)
        shouldBe(test(derived), 20);
}

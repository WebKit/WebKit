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

class Derived0 extends Base {
    constructor() {
        super();
    }
}

class Derived1 extends Base {
    constructor() {
        super();
    }

    ok() { }
}

class Derived2 extends Base {
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

$vm.toCacheableDictionary(Base.prototype);

function test(derived)
{
    return derived.test();
}
noInline(test);

for (let i = 0; i < 1e3; ++i) {
    for (let derived of array)
        shouldBe(test(derived), 42);
}

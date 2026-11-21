function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error('bad error: ' + error);
}

let first = {
    value: 'first',
    method() {
        return 'first';
    }
};

let second = {
    value: 'second',
    method() {
        return 'second';
    }
};

function switchPrototype(key) {
    Object.setPrototypeOf(object, second);
    return key;
}

let object = {
    __proto__: first,
    get() {
        return super[switchPrototype('value')];
    },
    call() {
        return super[switchPrototype('method')]();
    }
};

function test() {
    Object.setPrototypeOf(object, first);
    shouldBe(object.get(), 'second');
    Object.setPrototypeOf(object, first);
    shouldBe(object.call(), 'second');
}

for (let i = 0; i < testLoopCount; i++)
    test();

let count = 0;

function key() {
    count++;
    return 'value';
}

let iterable = {
    [Symbol.iterator]() {
        return {
            next() {
                count++;
                return { done: true };
            }
        };
    }
};

let source = {
    get value() {
        count++;
        return 0;
    }
};

class Base {
    constructor() {
        count++;
    }
}

let constructors = [
    class extends Base { constructor() { super[key()] = 0; } },
    class extends Base { constructor() { super[super()] = 0; } },
    class extends Base { constructor() { super[key()] += 0; } },
    class extends Base { constructor() { super[key()] ||= 0; } },
    class extends Base { constructor() { ++super[key()]; } },
    class extends Base { constructor() { let result = super[key()]--; } },
    class extends Base { constructor() { delete super[key()]; } },
    class extends Base { constructor() { super[key()]`x`; } },
    class extends Base { constructor() { [super[key()]] = [0]; } },
    class extends Base { constructor() { ({ value: super[key()] } = { value: 0 }); } },
    class extends Base { constructor() { ({ ...super[key()] } = { value: 0 }); } },
    class extends Base { constructor() { for (super[key()] of [0]); } },
    class extends Base { constructor() { for (super[key()] in { value: 0 }); } },
    class extends Base { constructor() { super.value = key(); } },
    class extends Base { constructor() { [super.value] = iterable; } },
    class extends Base { constructor() { ({ value: super.value } = source); } },
    class extends Base { constructor() { ({ ...super.value } = source); } },
    class extends Base { constructor() { (() => { super[key()] = 0; })(); } },
    class extends Base { constructor() { eval('super[key()] = 0'); } },
];

for (let constructor of constructors) {
    shouldThrow(() => new constructor, ReferenceError);
    shouldBe(count, 0);
}

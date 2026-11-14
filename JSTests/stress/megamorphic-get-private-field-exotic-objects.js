function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof TypeError))
        throw new Error('expected TypeError but got: ' + error);
}

function trap() { throw new Error('trap must not be called'); }
const handler = {
    get: trap,
    getOwnPropertyDescriptor: trap,
    has: trap,
    getPrototypeOf: trap,
};

class Stamper extends (function (object) { return object; }) {
    #state;

    constructor(object, state) {
        super(object);
        this.#state = state;
    }

    static get(object) {
        return object.#state;
    }
}
noInline(Stamper.get);

let objects = [];
for (let i = 0; i < 16; ++i)
    objects.push(new Stamper({ ['extra' + i]: i }, i));

function check(list) {
    for (let i = 0; i < testLoopCount; ++i) {
        let index = i % list.length;
        shouldBe(Stamper.get(list[index]), index);
    }
}

check(objects);

let exotics = [
    globalThis,
    [1, 2, 3],
    function () { },
    new String("test"),
    new Uint8Array(8),
    /regexp/,
    new Proxy({}, handler),
];
for (let exotic of exotics)
    objects.push(new Stamper(exotic, objects.length));
check(objects);

let proxy = new Proxy(objects[0], handler);
shouldThrowTypeError(() => Stamper.get(proxy));
check(objects);

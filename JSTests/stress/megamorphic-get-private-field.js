//@ defaultRun
//@ runNoCJITNoAccessInlining
//@ runFTLNoCJITNoAccessInlining

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

class Base {
    #state;

    constructor(state) {
        this.#state = state;
    }

    getState() {
        return this.#state;
    }

    setState(state) {
        this.#state = state;
    }
}
noInline(Base.prototype.getState);

class Other {
    #state;

    constructor(state) {
        this.#state = state;
    }
}

let subs = [];
for (let i = 0; i < 64; ++i) {
    class Sub extends Base {
        constructor(state) {
            super(state);
            this['extra' + i] = i;
        }
    }
    subs.push(new Sub(i));
}

class Stamper extends (function (object) { return object; }) {
    #field;

    constructor(object, value) {
        super(object);
        this.#field = value;
    }

    static get(object) {
        return object.#field;
    }

    static set(object, value) {
        object.#field = value;
    }
}
noInline(Stamper.get);

let stamped = [];
for (let i = 0; i < 64; ++i) {
    let object = {};
    for (let j = 0; j < i; ++j)
        object['before' + j] = j;
    stamped.push(new Stamper(object, i));
}

let getState = Base.prototype.getState;
let get = Stamper.get;
let invalidReceivers = [
    {},
    new Other(42),
    Object.create(subs[0]),
    Object.create(stamped[0]),
    null,
    undefined,
    1,
    'string',
    Symbol('symbol'),
];

function shouldThrowForInvalidReceivers() {
    for (let receiver of invalidReceivers) {
        shouldThrowTypeError(() => getState.call(receiver));
        shouldThrowTypeError(() => get(receiver));
    }
    shouldThrowTypeError(() => getState.call(stamped[0]));
    shouldThrowTypeError(() => get(subs[0]));
}

// Only a few structures at first, so that optimizing tiers see a single private name. They emit by-id ICs for it if
// access inlining is disabled.
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(getState.call(subs[i & 3]), i & 3);
    shouldBe(get(stamped[i & 3]), i & 3);
}

for (let i = 0; i < testLoopCount; ++i) {
    let index = i & 63;
    shouldBe(getState.call(subs[index]), index);
    shouldBe(get(stamped[index]), index);
    if (!(i & 0xff))
        shouldThrowForInvalidReceivers();
}

for (let i = 0; i < testLoopCount; ++i) {
    let index = i & 63;
    subs[index].setState(i);
    Stamper.set(stamped[index], -i);
    shouldBe(getState.call(subs[index]), i);
    shouldBe(get(stamped[index]), -i);
}

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
}
noInline(Base.prototype.getState);

class Other {
    #state;

    constructor(state) {
        this.#state = state;
    }
}

let objects = [];
for (let i = 0; i < 64; ++i) {
    class Sub extends Base {
        constructor(state) {
            super(state);
            this['extra' + i] = i;
        }
    }
    objects.push(new Sub(i));
}

let getState = Base.prototype.getState;
let plainObject = {};
let otherPrivateField = new Other(42);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(objects[i & 63].getState(), i & 63);
    if (!(i & 0xff)) {
        // Reading a private field from an object which does not have it must throw,
        // even after the IC goes megamorphic.
        shouldThrowTypeError(() => getState.call(plainObject));
        shouldThrowTypeError(() => getState.call(otherPrivateField));
        shouldThrowTypeError(() => getState.call(null));
        shouldThrowTypeError(() => getState.call(1));
        shouldThrowTypeError(() => getState.call('string'));
    }
}

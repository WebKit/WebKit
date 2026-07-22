function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// Private field reads must not trigger proxy traps, even via the megamorphic paths.
let traps = 0;
const handler = {
    get() { ++traps; return undefined; },
    getOwnPropertyDescriptor() { ++traps; return undefined; },
    has() { ++traps; return false; },
};

class MaybeProxy {
    constructor(useProxy) {
        if (useProxy)
            return new Proxy({}, handler);
    }
}

class Base extends MaybeProxy {
    #state;

    constructor(state, useProxy) {
        super(useProxy);
        this.#state = state;
    }

    getState() {
        return this.#state;
    }
}
noInline(Base.prototype.getState);

let objects = [];
for (let i = 0; i < 16; ++i) {
    class Sub extends Base {
        constructor(state, useProxy) {
            super(state, useProxy);
            if (!useProxy)
                this['extra' + i] = i;
        }
    }
    // Odd indices get a proxy receiver with the private field stamped directly on the proxy.
    objects.push(new Sub(i, !!(i & 1)));
}

// Proxy receivers are not linked to Sub.prototype, so call the getter directly. This keeps
// the only proxy interaction the private field read itself, which must not hit any trap.
let getState = Base.prototype.getState;
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(getState.call(objects[i & 15]), i & 15);

shouldBe(traps, 0);

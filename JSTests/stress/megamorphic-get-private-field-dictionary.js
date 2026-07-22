function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
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

let objects = [];
for (let i = 0; i < 16; ++i) {
    class Sub extends Base {
        constructor(state) {
            super(state);
            this['extra' + i] = i;
        }
    }
    objects.push(new Sub(i));
}

// Warm the IC site up to megamorphic.
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(objects[i & 15].getState(), i & 15);

// Turn some receivers into dictionary structures with heavy property churn, and freeze others.
for (let i = 0; i < 8; ++i) {
    let object = objects[i];
    for (let j = 0; j < 100; ++j)
        object['churn' + j] = j;
    for (let j = 0; j < 100; ++j)
        delete object['churn' + j];
}
for (let i = 8; i < 12; ++i)
    Object.freeze(objects[i]);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(objects[i & 15].getState(), i & 15);

// Keep mutating dictionary receivers between reads so cached offsets must stay coherent.
for (let i = 0; i < testLoopCount; ++i) {
    let object = objects[i & 7];
    object['mutate' + (i & 31)] = i;
    shouldBe(object.getState(), i & 7);
    delete object['mutate' + (i & 31)];
    shouldBe(object.getState(), i & 7);
}

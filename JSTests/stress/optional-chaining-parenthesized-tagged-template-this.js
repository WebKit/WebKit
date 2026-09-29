function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

const log = [];
const arg = () => { log.push('arg'); return 1; };
const key = () => { log.push('key'); return 'b'; };

class Private {
    #method(strings) { return this; }
    static call(o) { return (o?.#method)`x`; }
}

const receiver = {
    b(strings) {
        "use strict";
        return this;
    },
    c: {
        d(strings) {
            "use strict";
            return this;
        }
    },
    get getter() {
        const self = this;
        return function () {
            "use strict";
            return [self, this];
        };
    },
    sum(strings, ...values) {
        return this === receiver ? strings.raw.join('|') + values.join() : undefined;
    },
};

class Field {
    x = (receiver?.b)`x`;
    static y = (receiver?.b)`x`;
}

function testTailPosition(base) {
    "use strict";
    return (base?.b)`x`;
}

function testConditionContext(base) {
    if ((base?.b)`x`)
        return true;
    return false;
}

function test() {
    log.length = 0;

    shouldBe((receiver?.b)`x`, receiver);
    shouldBe((receiver?.['b'])`x`, receiver);
    shouldBe((receiver?.[key()])`x${arg()}`, receiver);
    shouldBe(log.join(), 'key,arg');
    shouldBe((receiver?.c.d)`x`, receiver.c);
    shouldBe((receiver?.c?.d)`x`, receiver.c);
    shouldBe((receiver.c?.d)`x`, receiver.c);
    shouldBe((receiver?.c['d'])`x`, receiver.c);
    shouldBe(((receiver?.b))`x`, receiver);
    shouldBe((receiver?.sum)`a${1}b${2}c`, 'a|b|c1,2');

    const [self, thisValue] = (receiver?.getter)`x`;
    shouldBe(self, receiver);
    shouldBe(thisValue, receiver);

    const object = new Private;
    shouldBe(Private.call(object), object);

    shouldBe(testTailPosition(receiver), receiver);
    shouldBe(testConditionContext(receiver), true);

    shouldBe(new Field().x, receiver);
    shouldBe(Field.y, receiver);
}

for (let i = 0; i < testLoopCount; ++i)
    test();

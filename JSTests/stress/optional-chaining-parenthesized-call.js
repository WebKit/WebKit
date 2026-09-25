function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

const log = [];
const arg = () => { log.push('arg'); return 1; };
const key = () => { log.push('key'); return 'b'; };

function shouldThrowTypeError(func, expectedLog) {
    log.length = 0;
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error(`Expected TypeError but got ${error}`);
    shouldBe(log.join(), expectedLog);
}

function shouldThrowReferenceError(source) {
    let error;
    try {
        eval(source);
    } catch (e) {
        error = e;
    }

    if (!(error instanceof ReferenceError))
        throw new Error(`Expected ReferenceError from ${source} but got ${error}`);
}

class Private {
    #method() { return this; }
    static call(o) { return (o?.#method)(); }
}

function f() { return this; }

const receiver = {
    b(x) { return this === receiver ? x : undefined; },
    c: { d() { return this; } },
    ownProperty: 1,
};

class Field {
    x = (receiver?.b)(8);
    static y = (receiver?.b)(9);
}

class NullishField {
    static base = null;
    x = (NullishField.base?.b)(arg());
}

function testTailPosition(base) {
    "use strict";
    return (base?.b)(1);
}

function testConditionContext(base) {
    if ((base?.b)(1))
        return true;
    return false;
}

function testNullish(base) {
    shouldThrowTypeError(() => (base?.b)(arg()), 'arg');
    shouldThrowTypeError(() => (base?.b.c.d)(arg()), 'arg');
    shouldThrowTypeError(() => (base?.b?.c)(arg()), 'arg');
    shouldThrowTypeError(() => (base?.['b'])(arg()), 'arg');
    shouldThrowTypeError(() => (base?.[key()])(arg()), 'arg');
    shouldThrowTypeError(() => (base?.call)(arg()), 'arg');
    shouldThrowTypeError(() => (base?.b)(arg())?.c, 'arg');
    shouldThrowTypeError(() => (base?.b)(...[arg()]), 'arg');
    NullishField.base = base;
    shouldThrowTypeError(() => new NullishField, 'arg');
    shouldThrowTypeError(() => Private.call(base), '');
    shouldThrowTypeError(() => testTailPosition(base), '');
    shouldThrowTypeError(() => testConditionContext(base), '');

    log.length = 0;
    shouldBe((base?.b)?.(arg()), undefined);
    shouldBe(base?.b(arg()), undefined);
    shouldBe(base?.b?.(arg()), undefined);
    shouldBe(base?.[key()](arg()), undefined);
    shouldBe(log.length, 0);
}

function testNonNullish() {
    const thisValue = { thisValue: true };

    shouldBe((receiver?.b)(2), 2);
    shouldBe((receiver?.['b'])(3), 3);
    shouldBe((receiver?.b)(...[5]), 5);
    shouldBe((receiver?.b)?.(6), 6);
    shouldBe((receiver?.['b'])?.(7), 7);
    shouldBe(new Field().x, 8);
    shouldBe(Field.y, 9);
    let clobbered = receiver;
    shouldBe((clobbered?.[(clobbered = null, 'b')])(4), 4);
    shouldBe((receiver?.c?.d)(), receiver.c);
    shouldBe((receiver?.c.d)(), receiver.c);
    shouldBe((receiver?.c).d(), receiver.c);
    shouldBe((f?.call)(thisValue), thisValue);
    shouldBe((receiver?.hasOwnProperty)('ownProperty'), true);
    const privateInstance = new Private;
    shouldBe(Private.call(privateInstance), privateInstance);
    shouldBe(testTailPosition(receiver), 1);
    shouldBe(testConditionContext({ b() { return true; } }), true);
    shouldBe(testConditionContext({ b() { return false; } }), false);
    shouldThrowTypeError(() => (receiver?.missing)(arg()), 'arg');
}

function testAssignmentTarget() {
    shouldThrowReferenceError('(receiver?.b)(1) = 1');
    shouldThrowReferenceError('(receiver?.b)(1)++');
    shouldThrowReferenceError('for ((receiver?.b)(1) in { x: 1 }) ;');
}

testAssignmentTarget();

for (let i = 0; i < testLoopCount; ++i) {
    testNullish(i % 2 ? null : undefined);
    testNonNullish();
}

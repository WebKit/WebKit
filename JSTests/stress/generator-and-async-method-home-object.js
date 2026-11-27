function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

class Base {
    get x() { return 42; }
    static get y() { return 43; }
}

class Derived extends Base {
    *noSuper() { yield 1; }
    *direct() { yield super.x; }
    *arrow() { yield (() => super.x)(); }
    *evalDirect() { yield eval("super.x"); }
    *evalInArrow() { yield (() => eval("super.x"))(); }
    *nested() { yield* this.direct(); }
    *afterYield() { yield 0; yield super.x; }
    *evalInBlock() { { yield eval("super.x"); } }
    *computedKey() { class C { [super.x]() { } } yield Object.hasOwn(C.prototype, "42"); }
    *asyncArrowInBody() { yield (async () => super.x)(); }
    *superInParameter(a = super.x) { yield a; }
    static *staticDirect() { yield super.y; }
    static async staticAsync() { await 0; return super.y; }
    async asyncNoSuper() { await 0; return 1; }
    async asyncDirect() { await 0; return super.x; }
    async asyncArrow() { await 0; return (() => super.x)(); }
    async asyncEval() { await 0; return eval("super.x"); }
    async *asyncGenNoSuper() { yield 1; }
    async *asyncGenDirect() { await 0; yield super.x; }
    async *asyncGenArrow() { yield (() => super.x)(); }
    async *asyncGenEval() { await 0; yield eval("super.x"); }
}

let object = {
    __proto__: { z: 44 },
    *noSuper() { yield 1; },
    *direct() { yield super.z; },
    *arrow() { yield (() => super.z)(); },
    async asyncDirect() { await 0; return super.z; },
    async *asyncGenDirect() { yield super.z; },
};

let errors = [];
function check(promise, expected) {
    promise.then((value) => {
        if (value !== expected)
            errors.push(new Error('bad async value: ' + value + ' expected: ' + expected));
    }, (error) => errors.push(error));
}

let d = new Derived;
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(d.noSuper().next().value, 1);
    shouldBe(d.direct().next().value, 42);
    shouldBe(d.arrow().next().value, 42);
    shouldBe(d.evalDirect().next().value, 42);
    shouldBe(d.evalInArrow().next().value, 42);
    shouldBe(d.nested().next().value, 42);
    let afterYield = d.afterYield();
    shouldBe(afterYield.next().value, 0);
    shouldBe(afterYield.next().value, 42);
    shouldBe(d.evalInBlock().next().value, 42);
    shouldBe(d.computedKey().next().value, true);
    shouldBe(d.superInParameter().next().value, 42);
    shouldBe(Derived.staticDirect().next().value, 43);
    shouldBe(object.noSuper().next().value, 1);
    shouldBe(object.direct().next().value, 44);
    shouldBe(object.arrow().next().value, 44);
    if (i % 100 === 0) {
        check(d.asyncNoSuper(), 1);
        check(d.asyncDirect(), 42);
        check(d.asyncArrow(), 42);
        check(d.asyncEval(), 42);
        check(d.asyncGenNoSuper().next().then((result) => result.value), 1);
        check(d.asyncGenDirect().next().then((result) => result.value), 42);
        check(d.asyncGenArrow().next().then((result) => result.value), 42);
        check(d.asyncGenEval().next().then((result) => result.value), 42);
        check(d.asyncArrowInBody().next().value, 42);
        check(Derived.staticAsync(), 43);
        check(object.asyncDirect(), 44);
        check(object.asyncGenDirect().next().then((result) => result.value), 44);
    }
}

drainMicrotasks();
if (errors.length)
    throw errors[0];

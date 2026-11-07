function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowReferenceError(func, name) {
    let error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof ReferenceError))
        throw new Error(`expected ReferenceError but got ${error}`);
    if (name)
        shouldBe(String(error), `ReferenceError: Cannot access '${name}' before initialization.`);
}

function useAfterDeclaration(object) {
    const { a, b } = object;
    let { c } = object;
    c += a;
    return a + b + c;
}
noInline(useAfterDeclaration);

function defaultValueRefersToLaterBinding(object) {
    const { a = b, b } = object;
    return a + b;
}
noInline(defaultValueRefersToLaterBinding);

function defaultValueRefersToEarlierBinding(object) {
    const { a, b = a } = object;
    return a + b;
}
noInline(defaultValueRefersToEarlierBinding);

function defaultValueRefersToItself(object) {
    let { a = a } = object;
    return a;
}
noInline(defaultValueRefersToItself);

function computedKeyRefersToLaterBinding(object) {
    const { [b]: a, b } = object;
    return a + b;
}
noInline(computedKeyRefersToLaterBinding);

function computedKeyRefersToEarlierBinding(object) {
    const { a, [a]: b } = object;
    return b;
}
noInline(computedKeyRefersToEarlierBinding);

function nestedPatternRefersToLaterBinding(object) {
    const { inner: { a = b }, b } = object;
    return a + b;
}
noInline(nestedPatternRefersToLaterBinding);

function nestedPatternRefersToEarlierBinding(object) {
    const { inner: { a }, b = a } = object;
    return a + b;
}
noInline(nestedPatternRefersToEarlierBinding);

function arrayInObjectRefersToLaterBinding(object) {
    const { inner: [a = b], b } = object;
    return a + b;
}
noInline(arrayInObjectRefersToLaterBinding);

function objectInArrayRefersToLaterBinding(array) {
    const [{ a = b }, b] = array;
    return a + b;
}
noInline(objectInArrayRefersToLaterBinding);

function restRefersToLaterBinding(object) {
    const { a = rest, ...rest } = object;
    return a;
}
noInline(restRefersToLaterBinding);

function useAfterRest(object) {
    const { a, ...rest } = object;
    return a + rest.b + rest.c;
}
noInline(useAfterRest);

function rightHandSideRefersToBinding() {
    const { a } = { a: a };
    return a;
}
noInline(rightHandSideRefersToBinding);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(useAfterDeclaration({ a: 1, b: 2, c: 3 }), 7);
    shouldBe(defaultValueRefersToLaterBinding({ a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => defaultValueRefersToLaterBinding({ b: 2 }), "b");
    shouldBe(defaultValueRefersToEarlierBinding({ a: 1 }), 2);
    shouldBe(defaultValueRefersToItself({ a: 1 }), 1);
    shouldThrowReferenceError(() => defaultValueRefersToItself({ }), "a");
    shouldThrowReferenceError(() => computedKeyRefersToLaterBinding({ b: "b" }), "b");
    shouldBe(computedKeyRefersToEarlierBinding({ a: "key", key: 42 }), 42);
    shouldBe(nestedPatternRefersToLaterBinding({ inner: { a: 1 }, b: 2 }), 3);
    shouldThrowReferenceError(() => nestedPatternRefersToLaterBinding({ inner: { }, b: 2 }), "b");
    shouldBe(nestedPatternRefersToEarlierBinding({ inner: { a: 1 } }), 2);
    shouldBe(arrayInObjectRefersToLaterBinding({ inner: [1], b: 2 }), 3);
    shouldThrowReferenceError(() => arrayInObjectRefersToLaterBinding({ inner: [], b: 2 }), "b");
    shouldBe(objectInArrayRefersToLaterBinding([{ a: 1 }, 2]), 3);
    shouldThrowReferenceError(() => objectInArrayRefersToLaterBinding([{ }, 2]), "b");
    shouldBe(restRefersToLaterBinding({ a: 1, b: 2 }), 1);
    shouldThrowReferenceError(() => restRefersToLaterBinding({ b: 2 }), "rest");
    shouldBe(useAfterRest({ a: 1, b: 2, c: 3 }), 6);
    shouldThrowReferenceError(rightHandSideRefersToBinding, "a");
}

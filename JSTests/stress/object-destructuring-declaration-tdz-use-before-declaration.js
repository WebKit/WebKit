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

function useBeforeDeclaration(object) {
    let result = a;
    const { a } = object;
    return result + a;
}
noInline(useBeforeDeclaration);

function typeofBeforeDeclaration(object) {
    let result = typeof a;
    let { a } = object;
    return result + a;
}
noInline(typeofBeforeDeclaration);

function assignBeforeDeclaration(object) {
    a = 1;
    let { a } = object;
    return a;
}
noInline(assignBeforeDeclaration);

function destructuringAssignBeforeDeclaration(object) {
    ({ a } = object);
    let { a } = object;
    return a;
}
noInline(destructuringAssignBeforeDeclaration);

function destructuringAssignWithTargetBeforeDeclaration(object) {
    ({ a: a } = object);
    let { a } = object;
    return a;
}
noInline(destructuringAssignWithTargetBeforeDeclaration);

function destructuringAssignRestBeforeDeclaration(object) {
    ({ ...a } = object);
    let { a } = object;
    return a;
}
noInline(destructuringAssignRestBeforeDeclaration);

function destructuringAssignAfterDeclaration(object) {
    let { a, b } = object;
    ({ a: b, b: a } = object);
    return a * 10 + b;
}
noInline(destructuringAssignAfterDeclaration);

function closureCreatedBeforeDeclaration(object, callEarly) {
    const getter = () => a;
    if (callEarly)
        return getter();
    const { a } = object;
    return getter();
}
noInline(closureCreatedBeforeDeclaration);

function closureCreatedAfterDeclaration(object) {
    const { a, b } = object;
    const getter = () => a + b;
    return getter();
}
noInline(closureCreatedAfterDeclaration);

function closureInDefaultValueCalledImmediately(object) {
    const { a = (() => b)(), b } = object;
    return a + b;
}
noInline(closureInDefaultValueCalledImmediately);

function closureInDefaultValueCalledLater(object) {
    const { a, getter = () => a + b, b } = object;
    return getter();
}
noInline(closureInDefaultValueCalledLater);

function hoistedFunctionCalledBeforeDeclaration(object, callEarly) {
    if (callEarly)
        return getter();
    const { a } = object;
    return getter();
    function getter() { return a; }
}
noInline(hoistedFunctionCalledBeforeDeclaration);

function evalInDefaultValue(object) {
    const { a = eval("b"), b } = object;
    return a + b;
}
noInline(evalInDefaultValue);

function evalAfterDeclaration(object) {
    const { a, b } = object;
    return eval("a + b");
}
noInline(evalAfterDeclaration);

function classInDefaultValue(object) {
    const { a = class { static [b]() { } }, b } = object;
    return a;
}
noInline(classInDefaultValue);

let leakedGetters = [];
function throwingGetterLeavesLaterBindingUninitialized() {
    const { a, b = leakedGetters.push(() => a, () => b, () => c), c } = {
        a: 1,
        get c() { throw new Error("c"); },
    };
    return a + b + c;
}
noInline(throwingGetterLeavesLaterBindingUninitialized);

function throwingRightHandSideLeavesBindingUninitialized(value) {
    leakedGetters.push(getter);
    const { a } = value;
    return a;
    function getter() { return a; }
}
noInline(throwingRightHandSideLeavesBindingUninitialized);

function* generatorYieldInDefaultValue(object) {
    const getter = () => b;
    const { a = yield getter, b } = object;
    return a + b;
}

for (let i = 0; i < testLoopCount; i++) {
    shouldThrowReferenceError(() => useBeforeDeclaration({ a: 1 }), "a");
    shouldThrowReferenceError(() => typeofBeforeDeclaration({ a: 1 }), "a");
    shouldThrowReferenceError(() => assignBeforeDeclaration({ a: 1 }), "a");
    shouldThrowReferenceError(() => destructuringAssignBeforeDeclaration({ a: 1 }));
    shouldThrowReferenceError(() => destructuringAssignWithTargetBeforeDeclaration({ a: 1 }));
    shouldThrowReferenceError(() => destructuringAssignRestBeforeDeclaration({ a: 1 }));
    shouldBe(destructuringAssignAfterDeclaration({ a: 1, b: 2 }), 21);
    shouldBe(closureCreatedBeforeDeclaration({ a: 1 }, false), 1);
    shouldThrowReferenceError(() => closureCreatedBeforeDeclaration({ a: 1 }, true), "a");
    shouldBe(closureCreatedAfterDeclaration({ a: 1, b: 2 }), 3);
    shouldBe(closureInDefaultValueCalledImmediately({ a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => closureInDefaultValueCalledImmediately({ b: 2 }), "b");
    shouldBe(closureInDefaultValueCalledLater({ a: 1, b: 2 }), 3);
    shouldBe(hoistedFunctionCalledBeforeDeclaration({ a: 1 }, false), 1);
    shouldThrowReferenceError(() => hoistedFunctionCalledBeforeDeclaration({ a: 1 }, true), "a");
}

for (let i = 0; i < 1e3; i++) {
    shouldBe(evalInDefaultValue({ a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => evalInDefaultValue({ b: 2 }), "b");
    shouldBe(evalAfterDeclaration({ a: 1, b: 2 }), 3);

    shouldBe(typeof classInDefaultValue({ a: 1, b: "b" }), "number");
    shouldThrowReferenceError(() => classInDefaultValue({ b: "b" }), "b");

    leakedGetters = [];
    let error = null;
    try {
        throwingGetterLeavesLaterBindingUninitialized();
    } catch (e) {
        error = e;
    }
    shouldBe(String(error), "Error: c");
    shouldBe(leakedGetters.length, 3);
    shouldBe(leakedGetters[0](), 1);
    shouldBe(leakedGetters[1](), 3);
    shouldThrowReferenceError(leakedGetters[2], "c");

    leakedGetters = [];
    error = null;
    try {
        throwingRightHandSideLeavesBindingUninitialized(null);
    } catch (e) {
        error = e;
    }
    shouldBe(error instanceof TypeError, true);
    shouldThrowReferenceError(leakedGetters[0], "a");

    let generator = generatorYieldInDefaultValue({ b: 2 });
    let getter = generator.next().value;
    shouldThrowReferenceError(getter, "b");
    shouldBe(generator.next(1).value, 3);
    shouldBe(getter(), 2);

    generator = generatorYieldInDefaultValue({ b: 2 });
    getter = generator.next().value;
    generator.return();
    shouldThrowReferenceError(getter, "b");
}

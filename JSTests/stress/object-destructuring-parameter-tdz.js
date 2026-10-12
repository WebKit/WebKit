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
    shouldBe(String(error), `ReferenceError: Cannot access '${name}' before initialization.`);
}

function useParameter({ a, b }) {
    return a + b + a;
}
noInline(useParameter);

function defaultValueRefersToLaterBinding({ a = b, b }) {
    return a + b;
}
noInline(defaultValueRefersToLaterBinding);

function defaultValueRefersToEarlierBinding({ a, b = a }) {
    return a + b;
}
noInline(defaultValueRefersToEarlierBinding);

function defaultValueRefersToLaterParameter({ a = b }, b) {
    return a + b;
}
noInline(defaultValueRefersToLaterParameter);

function laterParameterRefersToBinding({ a }, b = a) {
    return a + b;
}
noInline(laterParameterRefersToBinding);

function earlierParameterRefersToBinding(b = a, { a }) {
    return a + b;
}
noInline(earlierParameterRefersToBinding);

function parameterDefaultRefersToItsBinding({ a } = { a: a }) {
    return a;
}
noInline(parameterDefaultRefersToItsBinding);

function restRefersToLaterBinding({ a = rest, ...rest }) {
    return a;
}
noInline(restRefersToLaterBinding);

function closureInLaterParameter({ a }, getter = () => a) {
    return getter();
}
noInline(closureInLaterParameter);

function closureInEarlierParameter(getter = () => a, { a }) {
    return getter();
}
noInline(closureInEarlierParameter);

function closureInEarlierParameterCalledInPattern(getter = () => a, { b = getter(), a }) {
    return a + b;
}
noInline(closureInEarlierParameterCalledInPattern);

function closureInBody({ a, b }) {
    const getter = () => a + b;
    return getter();
}
noInline(closureInBody);

function varWithSameName({ a }) {
    var a;
    return a;
}
noInline(varWithSameName);

function varWithSameNameAndClosure({ a }, getter = () => a) {
    var a = a + 1;
    return a * 10 + getter();
}
noInline(varWithSameNameAndClosure);

function usesArguments({ a, b = a }) {
    return a + b + arguments.length;
}
noInline(usesArguments);

const arrowFunction = ({ a = b, b }) => a + b;
noInline(arrowFunction);

async function asyncFunction({ a = b, b }) {
    return a + b;
}
noInline(asyncFunction);

function* generatorFunction({ a = b, b }) {
    yield a + b;
}
noInline(generatorFunction);

class Klass {
    constructor({ a = b, b }) {
        this.value = a + b;
    }

    method({ a, b = a }) {
        return a + b + this.value;
    }

    set accessor({ a = b, b }) {
        this.value = a + b;
    }
}

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(useParameter({ a: 1, b: 2 }), 4);

    shouldBe(defaultValueRefersToLaterBinding({ a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => defaultValueRefersToLaterBinding({ b: 2 }), "b");
    shouldBe(defaultValueRefersToEarlierBinding({ a: 1 }), 2);

    shouldBe(defaultValueRefersToLaterParameter({ a: 1 }, 2), 3);
    shouldThrowReferenceError(() => defaultValueRefersToLaterParameter({ }, 2), "b");
    shouldBe(laterParameterRefersToBinding({ a: 1 }), 2);
    shouldBe(earlierParameterRefersToBinding(1, { a: 2 }), 3);
    shouldThrowReferenceError(() => earlierParameterRefersToBinding(undefined, { a: 2 }), "a");
    shouldBe(parameterDefaultRefersToItsBinding({ a: 1 }), 1);
    shouldThrowReferenceError(() => parameterDefaultRefersToItsBinding(), "a");

    shouldBe(restRefersToLaterBinding({ a: 1, b: 2 }), 1);
    shouldThrowReferenceError(() => restRefersToLaterBinding({ b: 2 }), "rest");

    shouldBe(closureInLaterParameter({ a: 1 }), 1);
    shouldBe(closureInEarlierParameter(undefined, { a: 1 }), 1);
    shouldBe(closureInEarlierParameterCalledInPattern(undefined, { a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => closureInEarlierParameterCalledInPattern(undefined, { a: 1 }), "a");
    shouldBe(closureInBody({ a: 1, b: 2 }), 3);

    shouldBe(varWithSameName({ a: 1 }), 1);
    shouldBe(varWithSameNameAndClosure({ a: 1 }), 21);
    shouldBe(usesArguments({ a: 1 }, 2, 3), 5);

    shouldBe(arrowFunction({ a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => arrowFunction({ b: 2 }), "b");

    shouldBe(generatorFunction({ a: 1, b: 2 }).next().value, 3);
    shouldThrowReferenceError(() => generatorFunction({ b: 2 }), "b");

    shouldBe(new Klass({ a: 1, b: 2 }).value, 3);
    shouldThrowReferenceError(() => new Klass({ b: 2 }), "b");
    let instance = new Klass({ a: 1, b: 2 });
    shouldBe(instance.method({ a: 1 }), 5);
    instance.accessor = { a: 3, b: 4 };
    shouldBe(instance.value, 7);
    shouldThrowReferenceError(() => { instance.accessor = { b: 4 }; }, "b");
}

for (let i = 0; i < 1e3; i++) {
    let result = null;
    let error = null;
    asyncFunction({ a: 1, b: 2 }).then((value) => { result = value; });
    asyncFunction({ b: 2 }).catch((e) => { error = e; });
    drainMicrotasks();
    shouldBe(result, 3);
    shouldBe(error instanceof ReferenceError, true);
    shouldBe(String(error), "ReferenceError: Cannot access 'b' before initialization.");
}

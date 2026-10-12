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

function switchCase(object, value) {
    switch (value) {
    case 0:
        const { a } = object;
        return a;
    case 1:
        return a;
    }
}
noInline(switchCase);

function switchCaseLet(object, value) {
    switch (value) {
    case 0:
        let { a, b } = object;
        return a + b;
    default:
        return b;
    }
}
noInline(switchCaseLet);

function switchCaseFallThrough(object, value) {
    switch (value) {
    case 0:
        const { a } = object;
    case 1:
        return a;
    }
}
noInline(switchCaseFallThrough);

function loopBody(object, count) {
    let result = 0;
    for (let i = 0; i < count; i++) {
        const getter = () => a;
        if (i === 2)
            result += getter();
        const { a } = object;
        result += a;
    }
    return result;
}
noInline(loopBody);

function forStatementInitializer(object) {
    let result = 0;
    for (let { a, b } = object; a < b; a++)
        result += a;
    return result;
}
noInline(forStatementInitializer);

function forOfDeclaration(list) {
    let result = 0;
    for (const { a, b = a } of list)
        result += a + b;
    return result;
}
noInline(forOfDeclaration);

function forOfDeclarationRefersToLaterBinding(list) {
    let result = 0;
    for (const { a = b, b } of list)
        result += a + b;
    return result;
}
noInline(forOfDeclarationRefersToLaterBinding);

function forOfHeadRefersToBinding() {
    for (const { a } of [a])
        return a;
}
noInline(forOfHeadRefersToBinding);

function forInDeclaration(object) {
    let result = 0;
    for (const { length, [0]: first = length } in object)
        result += length + first.length;
    return result;
}
noInline(forInDeclaration);

function forInDeclarationRefersToLaterBinding(object) {
    for (const { missing = length, length } in object)
        return missing;
}
noInline(forInDeclarationRefersToLaterBinding);

function catchParameter(value) {
    try {
        throw value;
    } catch ({ a, b = a }) {
        return a + b;
    }
}
noInline(catchParameter);

function catchParameterRefersToLaterBinding(value) {
    try {
        throw value;
    } catch ({ a = b, b }) {
        return a + b;
    }
}
noInline(catchParameterRefersToLaterBinding);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(switchCase({ a: 1 }, 0), 1);
    shouldThrowReferenceError(() => switchCase({ a: 1 }, 1), "a");
    shouldBe(switchCaseLet({ a: 1, b: 2 }, 0), 3);
    shouldThrowReferenceError(() => switchCaseLet({ a: 1, b: 2 }, 1), "b");
    shouldBe(switchCaseFallThrough({ a: 1 }, 0), 1);
    shouldThrowReferenceError(() => switchCaseFallThrough({ a: 1 }, 1), "a");
    shouldBe(loopBody({ a: 1 }, 2), 2);
    shouldThrowReferenceError(() => loopBody({ a: 1 }, 3), "a");
    shouldBe(forStatementInitializer({ a: 1, b: 4 }), 6);
    shouldBe(forOfDeclaration([{ a: 1 }, { a: 2, b: 3 }]), 7);
    shouldBe(forOfDeclarationRefersToLaterBinding([{ a: 1, b: 2 }]), 3);
    shouldThrowReferenceError(() => forOfDeclarationRefersToLaterBinding([{ b: 2 }]), "b");
    shouldThrowReferenceError(forOfHeadRefersToBinding, "a");
    shouldBe(forInDeclaration({ ab: 1, cde: 2 }), 7);
    shouldThrowReferenceError(() => forInDeclarationRefersToLaterBinding({ ab: 1 }), "length");
    shouldBe(catchParameter({ a: 1 }), 2);
    shouldBe(catchParameterRefersToLaterBinding({ a: 1, b: 2 }), 3);
    shouldThrowReferenceError(() => catchParameterRefersToLaterBinding({ b: 2 }), "b");
}

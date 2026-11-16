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

class Private {
    #method(strings) { return this; }
    static call(o) { return (o?.#method)`x${arg()}`; }
}

const receiver = { ownProperty: 1 };

function test(base) {
    shouldThrowTypeError(() => (base?.b)`x${arg()}`, 'arg');
    shouldThrowTypeError(() => (base?.b.c)`x${arg()}`, 'arg');
    shouldThrowTypeError(() => (base?.[key()])`x${arg()}`, 'arg');
    shouldThrowTypeError(() => Private.call(base), 'arg');
    shouldThrowTypeError(() => (receiver?.missing)`x${arg()}`, 'arg');
    shouldThrowTypeError(() => (receiver?.ownProperty)`x${arg()}`, 'arg');
}

for (let i = 0; i < testLoopCount; ++i) {
    test(null);
    test(undefined);
}

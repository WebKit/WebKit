//@ runDefault("--jitPolicyScale=0.1", "--useConcurrentJIT=0")

function shouldThrow(caseName, fn, expectedErrorCtor, expectedErrorMessage) {
    if (!caseName)
        throw new Error(`must specify test case name`);

    const expected = `${expectedErrorCtor.name}(${expectedErrorMessage})`;
    try {
        fn();
        throw new Error(`${caseName}: Expected to throw ${expected}, but succeeded`);
    } catch (e) {
        const actual = `${e.name}(${e.message})`;
        if (!(e instanceof expectedErrorCtor) || e.message !== expectedErrorMessage)
            throw new Error(`${caseName}: Expected ${expected} but got ${actual}`);
    }
}

function test(size) {
    const array = new Float32Array(size);
    return array;
}
noInline(test);

let result = 0;

// warm up
for (let i = 0; i < testLoopCount; ++i) {
    const size = 1.0;
    const len = test(size).length;
    result += len;
}

// At this point, the function should be compiled down to the DFG/FTL

// This checks the compliance to the step 9 |ToIndex(firstArgument)| of https://tc39.es/ecma262/2026/#sec-typedarray
shouldThrow('compliant with ToIndex(firstArgument)', () => {
    test(Number.POSITIVE_INFINITY);
}, RangeError, 'length larger than (2 ** 53) - 1');

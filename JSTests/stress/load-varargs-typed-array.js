function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

function makeTA(ctor, values) {
    let ta = new ctor(values.length);
    for (let i = 0; i < values.length; ++i)
        ta[i] = values[i];
    return ta;
}

// Sum via apply-spread; exercises loadVarargs on a typed array argument.
function sumApply(...args) {
    let s = 0;
    for (let i = 0; i < args.length; ++i)
        s += Number(args[i]);
    return s;
}
noInline(sumApply);

function collect(...args) { return args; }
noInline(collect);

const numberCtors = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

function oracleSum(ta) {
    let s = 0;
    for (let i = 0; i < ta.length; ++i)
        s += Number(ta[i]);
    return s;
}

function test() {
    for (const ctor of numberCtors) {
        const ta = makeTA(ctor, [1, 2, 3, 4, 5, 6, 7]);
        const expected = oracleSum(ta);

        // f.apply(null, ta)
        shouldBe(sumApply.apply(null, ta), expected);
        // f(...ta)
        shouldBe(sumApply(...ta), expected);
        // [...ta] length and content
        const spread = [...ta];
        shouldBe(spread.length, ta.length);
        for (let i = 0; i < ta.length; ++i)
            shouldBe(spread[i], ta[i]);
        // leading args + spread (non-zero offset in the callee frame)
        const withLead = collect(100, 200, ...ta);
        shouldBe(withLead.length, ta.length + 2);
        shouldBe(withLead[0], 100);
        shouldBe(withLead[1], 200);
        for (let i = 0; i < ta.length; ++i)
            shouldBe(withLead[i + 2], ta[i]);
    }

    // Empty typed array spread.
    shouldBe(sumApply.apply(null, new Uint8Array(0)), 0);
    shouldBe(collect(...new Float64Array(0)).length, 0);

    // BigInt typed arrays go through the same path.
    // BigInt typed arrays go through the same path. Include large values that require
    // heap allocation (cannot be a BigInt32) and the .apply spelling, since boxing them
    // needs a real globalObject.
    {
        const ta = new BigInt64Array(4);
        ta[0] = 10n; ta[1] = 20n; ta[2] = 9223372036854775807n; ta[3] = -9223372036854775808n;
        const expected = [10n, 20n, 9223372036854775807n, -9223372036854775808n];
        const out = collect(...ta);
        shouldBe(out.length, 4);
        for (let i = 0; i < 4; ++i)
            shouldBe(out[i], expected[i]);
        const out2 = collect.apply(null, ta);
        shouldBe(out2.length, 4);
        for (let i = 0; i < 4; ++i)
            shouldBe(out2[i], expected[i]);

        const bu = new BigUint64Array([0n, 18446744073709551615n]);
        const out3 = collect.apply(null, bu);
        shouldBe(out3[0], 0n);
        shouldBe(out3[1], 18446744073709551615n);
    }

    // fromCharCode.apply over a Uint16Array (mirrors the TextDecoder polyfill hot path).
    {
        const codes = new Uint16Array([72, 101, 108, 108, 111]);
        shouldBe(String.fromCharCode.apply(null, codes), "Hello");
    }
}

for (let i = 0; i < testLoopCount; ++i)
    test();

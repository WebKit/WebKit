//@ runDefault("--maxPerThreadStackUsage=524288")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + String(actual).slice(0, 200) + ' expected: ' + String(expected).slice(0, 200));
}

function objectChain(depth) {
    let chain = { a: 1 };
    for (let i = 0; i < depth; ++i)
        chain = { a: chain };
    return chain;
}

function arrayChain(depth) {
    let chain = { a: 1 };
    for (let i = 0; i < depth; ++i)
        chain = { a: [chain] };
    return chain;
}

function nestedArrayChain(depth) {
    let chain = [];
    for (let i = 0; i < depth; ++i)
        chain = { a: [[chain]] };
    return chain;
}

// With the reduced stack, the deeper chains exhaust the fast path's stack limit, and the general
// purpose Stringifier has to produce the same output.
for (let depth of [10, 1000, 2000, 4000])
    shouldBe(JSON.stringify([{ a: 1 }, objectChain(depth)]), '[{"a":1},' + '{"a":'.repeat(depth) + '{"a":1}' + '}'.repeat(depth) + ']');
for (let depth of [10, 1000, 2000])
    shouldBe(JSON.stringify([{ a: 1 }, arrayChain(depth)]), '[{"a":1},' + '{"a":['.repeat(depth) + '{"a":1}' + ']}'.repeat(depth) + ']');
for (let depth of [10, 500, 1000])
    shouldBe(JSON.stringify([{ a: 1 }, nestedArrayChain(depth)]), '[{"a":1},' + '{"a":[['.repeat(depth) + '[]' + ']]}'.repeat(depth) + ']');

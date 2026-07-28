function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorType)
{
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}`);
}

// FastStringifier's property loop only tests for toJSON on the DontEnum branch, since that
// branch is the one that would otherwise skip the entry. An enumerable own toJSON needs no
// such test: a callable one fails the fast path when its value is appended, and a
// non-callable one is serialized as an ordinary property, which is what the loop does.
// These cases cover that reasoning; the expectations match V8.

// An enumerable own callable toJSON is invoked.
{
    shouldBe(JSON.stringify({ a: 1, toJSON() { return 'method'; } }), '"method"');
    shouldBe(JSON.stringify({ a: 1, toJSON: () => 'arrow' }), '"arrow"');
    shouldBe(JSON.stringify({ a: 1, toJSON: function () { return { b: 2 }; } }), '{"b":2}');
    shouldBe(JSON.stringify({ a: 1, toJSON: function (key) { return key; } }), '""');
    shouldBe(JSON.stringify({ key: { a: 1, toJSON: function (key) { return key; } } }), '{"key":"key"}');
    shouldBe(JSON.stringify({ nested: { a: 1, toJSON() { return 42; } } }), '{"nested":42}');
    shouldBe(JSON.stringify([{ a: 1, toJSON() { return [1]; } }]), '[[1]]');
    shouldBe(JSON.stringify({ a: 1, toJSON: (function () { }).bind(null) }), undefined);
    shouldBe(JSON.stringify({ a: 1, toJSON: new Proxy(function () { return 'proxy'; }, { }) }), '"proxy"');
    shouldBe(JSON.stringify({ a: 1, toJSON: function* () { } }), '{}');
    shouldThrow(() => JSON.stringify({ a: 1, toJSON: class C { } }), TypeError);
}

// An enumerable own non-callable toJSON is serialized as an ordinary property.
{
    shouldBe(JSON.stringify({ a: 1, toJSON: 42 }), '{"a":1,"toJSON":42}');
    shouldBe(JSON.stringify({ a: 1, toJSON: 'str' }), '{"a":1,"toJSON":"str"}');
    shouldBe(JSON.stringify({ a: 1, toJSON: true }), '{"a":1,"toJSON":true}');
    shouldBe(JSON.stringify({ a: 1, toJSON: null }), '{"a":1,"toJSON":null}');
    shouldBe(JSON.stringify({ a: 1, toJSON: undefined }), '{"a":1}');
    shouldBe(JSON.stringify({ a: 1, toJSON: { b: 2 } }), '{"a":1,"toJSON":{"b":2}}');
    shouldBe(JSON.stringify({ a: 1, toJSON: [1, 2] }), '{"a":1,"toJSON":[1,2]}');
    shouldBe(JSON.stringify({ toJSON: 'only' }), '{"toJSON":"only"}');
}

// The fast path and the general stringifier must agree, with and without a gap.
{
    let values = [
        { a: 1, toJSON() { return 'method'; } },
        { a: 1, toJSON: 42 },
        { a: 1, toJSON: { nested: 1 } },
        { a: 1, b: 2, c: 3, toJSON: 7 },
        { toJSON: undefined, a: 1 },
        { toJSON: 'only' },
    ];
    for (let value of values) {
        for (let space of [undefined, 2, '\t'])
            shouldBe(JSON.stringify(value, null, space), JSON.stringify(value, (key, x) => x, space));
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(JSON.stringify({ a: 1, toJSON() { return 'hot'; } }), '"hot"');
    shouldBe(JSON.stringify({ a: 1, toJSON: 42 }), '{"a":1,"toJSON":42}');
}

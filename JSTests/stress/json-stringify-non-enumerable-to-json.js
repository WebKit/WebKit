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

// https://tc39.es/ecma262/#sec-serializejsonproperty
// An own toJSON is called regardless of its enumerability.
function withNonEnumerableToJSON(object, toJSON)
{
    Object.defineProperty(object, 'toJSON', { value: toJSON, enumerable: false, writable: false, configurable: false });
    return object;
}

{
    let object = withNonEnumerableToJSON({ uri: 'u', cid: 'c', text: 't' }, function () { return { uri: this.uri, cid: this.cid }; });
    Object.freeze(object);
    shouldBe(JSON.stringify(object), '{"uri":"u","cid":"c"}');
    shouldBe(JSON.stringify({ record: object }), '{"record":{"uri":"u","cid":"c"}}');
    shouldBe(JSON.stringify([object]), '[{"uri":"u","cid":"c"}]');
    shouldBe(JSON.stringify(object, null, 2), '{\n  "uri": "u",\n  "cid": "c"\n}');
}

{
    shouldBe(JSON.stringify(withNonEnumerableToJSON({ a: 1 }, function () { return 'replaced'; })), '"replaced"');
    shouldBe(JSON.stringify(withNonEnumerableToJSON({ a: 1 }, function () { return undefined; })), undefined);
    shouldBe(JSON.stringify(withNonEnumerableToJSON({ a: 1 }, function (key) { return key; })), '""');
    shouldBe(JSON.stringify({ key: withNonEnumerableToJSON({ a: 1 }, function (key) { return key; }) }), '{"key":"key"}');
}

// A non-reified static table can hold a toJSON. Date.prototype.toJSON throws on a non-Date.
{
    shouldThrow(() => JSON.stringify(Date.prototype), TypeError);
    Object.getOwnPropertyNames(Date.prototype);
    shouldThrow(() => JSON.stringify(Date.prototype), TypeError);
}

// An own toJSON that is not callable is serialized as a normal property.
{
    shouldBe(JSON.stringify(withNonEnumerableToJSON({ a: 1 }, 42)), '{"a":1}');
    shouldBe(JSON.stringify({ a: 1, toJSON: 42 }), '{"a":1,"toJSON":42}');
}

// An enumerable own toJSON keeps working.
{
    shouldBe(JSON.stringify({ a: 1, toJSON() { return 'enumerable'; } }), '"enumerable"');
    shouldBe(JSON.stringify(Object.freeze({ a: 1, toJSON() { return 'frozen'; } })), '"frozen"');
}

// A callable replacer forces the general stringifier; both must agree.
{
    let values = [
        withNonEnumerableToJSON({ a: 1 }, function () { return { b: 2 }; }),
        withNonEnumerableToJSON({ a: 1 }, function () { return 'string'; }),
        withNonEnumerableToJSON({ a: 1 }, 'not callable'),
        { nested: withNonEnumerableToJSON({ a: 1 }, function () { return [1, 2]; }) },
        [withNonEnumerableToJSON({ a: 1 }, function () { return null; })],
        withNonEnumerableToJSON({ 'キー': '値' }, function () { return this['キー']; }),
    ];
    for (let value of values) {
        for (let space of [undefined, 2, '\t']) {
            shouldBe(JSON.stringify(value, null, space), JSON.stringify(value, (key, x) => x, space));
        }
    }
}

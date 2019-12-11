function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

Object.defineProperty(Array.prototype, '0', {
    get() {
        throw new Error('out');
    },
    set(value) {
        throw new Error('out');
    }
});

{
    let object = {
        a: 42,
        b: 42,
        c: 42
    };
    {
        let result = Object.keys(object);
        shouldBe(JSON.stringify(result), `["a","b","c"]`);
    }
    {
        let result = Object.values(object);
        shouldBe(JSON.stringify(result), `[42,42,42]`);
    }
}
{
    let object = {
        [Symbol.iterator]: 42,
        b: 42,
        c: 42
    };
    {
        let result = Object.getOwnPropertyNames(object);
        shouldBe(JSON.stringify(result), `["b","c"]`);
    }
    {
        let result = Object.getOwnPropertySymbols(object);
        shouldBe(result.length, 1);
        shouldBe(result[0], Symbol.iterator);
    }
}

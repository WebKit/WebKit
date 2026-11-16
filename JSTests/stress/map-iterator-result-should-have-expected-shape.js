function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let map = new Map();
    map.set(42, 42);
    let iterator = map[Symbol.iterator]();
    {
        let result = iterator.next();
        shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["done","value"]`);
        shouldBe(result.done, false);
        shouldBe(JSON.stringify(result.value), `[42,42]`);
    }
    {
        let result = iterator.next();
        shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["done","value"]`);
        shouldBe(result.done, true);
        shouldBe(result.value, undefined);
    }
}

{
    let map = new Map();
    let iterator = map[Symbol.iterator]();
    {
        let result = iterator.next();
        shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["done","value"]`);
        shouldBe(result.done, true);
        shouldBe(result.value, undefined);
    }
}

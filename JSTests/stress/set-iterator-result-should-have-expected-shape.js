function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let set = new Set();
    set.add(42);
    let iterator = set[Symbol.iterator]();
    {
        let result = iterator.next();
        shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["done","value"]`);
        shouldBe(result.done, false);
        shouldBe(result.value, 42);
    }
    {
        let result = iterator.next();
        shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["done","value"]`);
        shouldBe(result.done, true);
        shouldBe(result.value, undefined);
    }
}

{
    let set = new Set();
    let iterator = set[Symbol.iterator]();
    {
        let result = iterator.next();
        shouldBe(JSON.stringify(Object.getOwnPropertyNames(result).sort()), `["done","value"]`);
        shouldBe(result.done, true);
        shouldBe(result.value, undefined);
    }
}

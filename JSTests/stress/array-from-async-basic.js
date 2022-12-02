//@ requireOptions("--useArrayFromAsync=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        try {
            shouldBe(actual[i], expected[i]);
        } catch(e) {
            print(JSON.stringify(actual));
            throw e;
        }
    }
}

shouldBe(Array.fromAsync.length, 1);

async function test()
{
    // sync iterator
    {
        let result = await Array.fromAsync([0, 1, 2, 3, 4, 5]);
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }
    {
        let result = await Array.fromAsync(function* generator() {
            for (var i = 0; i < 6; ++i)
                yield i;
        }());
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }

    // async iterator
    {
        let result = await Array.fromAsync(async function* generator() {
            for (var i = 0; i < 6; ++i)
                yield i;
        }());
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }

    // array-like
    {
        let result = await Array.fromAsync({
            [0]: 0,
            [1]: 1,
            [2]: 2,
            [3]: 3,
            [4]: 4,
            [5]: 5,
            length: 6,
        });
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }

    try {
        await Array.fromAsync(null);
    } catch (error) {
        shouldBe(String(error), `TypeError: null is not an object`);
    }

    try {
        await Array.fromAsync(undefined);
    } catch (error) {
        shouldBe(String(error), `TypeError: undefined is not an object`);
    }

    try {
        await Array.fromAsync({ [Symbol.asyncIterator]: 42 });
    } catch (error) {
        shouldBe(String(error), `TypeError: Array.fromAsync requires that the property of the first argument, items[Symbol.asyncIterator], when exists, be a function`);
    }

    try {
        await Array.fromAsync({ [Symbol.iterator]: 42 });
    } catch (error) {
        shouldBe(String(error), `TypeError: Array.fromAsync requires that the property of the first argument, items[Symbol.iterator], when exists, be a function`);
    }

    // array-like, asyncIterator is ignored.
    {
        let result = await Array.fromAsync({
            [Symbol.asyncIterator]: null,
            [0]: 0,
            [1]: 1,
            [2]: 2,
            [3]: 3,
            [4]: 4,
            [5]: 5,
            length: 6,
        });
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }
    {
        let result = await Array.fromAsync({
            [Symbol.asyncIterator]: undefined,
            [0]: 0,
            [1]: 1,
            [2]: 2,
            [3]: 3,
            [4]: 4,
            [5]: 5,
            length: 6,
        });
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }

    // array-like, iterator is ignored.
    {
        let result = await Array.fromAsync({
            [Symbol.iterator]: null,
            [0]: 0,
            [1]: 1,
            [2]: 2,
            [3]: 3,
            [4]: 4,
            [5]: 5,
            length: 6,
        });
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }
    {
        let result = await Array.fromAsync({
            [Symbol.iterator]: undefined,
            [0]: 0,
            [1]: 1,
            [2]: 2,
            [3]: 3,
            [4]: 4,
            [5]: 5,
            length: 6,
        });
        shouldBeArray(result, [0, 1, 2, 3, 4, 5]);
    }
}

test().catch(function (error) {
    print("FAIL");
    print(String(error));
    print(String(error.stack));
    $vm.abort()
});
drainMicrotasks();

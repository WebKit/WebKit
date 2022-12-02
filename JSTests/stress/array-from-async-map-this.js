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
        let result = await Array.fromAsync([0, 1, 2, 3, 4, 5], function (value) { return value * value + this }, 42);
        shouldBeArray(result, [42, 43, 46, 51, 58, 67]);
    }
    {
        let result = await Array.fromAsync(function* generator() {
            for (var i = 0; i < 6; ++i)
                yield i;
        }(), function (value) { return value * value + this }, 42);
        shouldBeArray(result, [42, 43, 46, 51, 58, 67]);
    }

    // async iterator
    {
        let result = await Array.fromAsync(async function* generator() {
            for (var i = 0; i < 6; ++i)
                yield i;
        }(), function (value) { return value * value + this }, 42);
        shouldBeArray(result, [42, 43, 46, 51, 58, 67]);
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
        }, function (value) { return value * value + this }, 42);
        shouldBeArray(result, [42, 43, 46, 51, 58, 67]);
    }
}

test().catch(function (error) {
    print("FAIL");
    print(String(error));
    print(String(error.stack));
    $vm.abort()
}, 42);
drainMicrotasks();

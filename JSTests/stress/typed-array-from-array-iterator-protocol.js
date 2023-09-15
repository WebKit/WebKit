//@ $skipModes << :lockdown

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

{
    let length = 5;
    let index = 0;
    [].values().__proto__.next = function () {
        if (index < length) {
            ++index;
            return {
                value: index,
                done: false
            };
        }
        return {
            value: null,
            done: true
        };
    };

    let a0 = new Uint32Array([0xffffffff, 0xfffffffe, 0xfffffff0, 0xfffff0f0]);
    let a1 = Uint8Array.from(a0);
    shouldBeArray(a1, [1, 2, 3, 4, 5]);
}

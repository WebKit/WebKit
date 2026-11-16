function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


function countUp(count) {
    let value = 0;

    const q = {
        async next() {
            if (value === count) {
                return {
                    done: true,
                    value: undefined,
                };
            }

            value++;

            return {
                done: false,
                value,
            };
        },
    };

    return {
        [Symbol.asyncIterator]: () => q,
    };
}

let count = 0;
for await (const i of countUp(10)) {
    count += i;
}
shouldBe(count, 55);

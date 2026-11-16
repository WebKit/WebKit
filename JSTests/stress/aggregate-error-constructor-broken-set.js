function test(set) {
    return new AggregateError(set);
}

function shouldBe(a, b) {
    if (a !== b) {
        throw new Error(`Expected ${b} but got ${a}`);
    }
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    let count = 10;
    set[Symbol.iterator] = () => {
        return {
            next() {
                if (count++ > 12)
                    return { value: count, done: true };
                return { value: count, done: false };
            }
        }
    };
    const e = test(set);
    shouldBe(e.errors.length, 3);
    shouldBe(e.errors[0], 11);
    shouldBe(e.errors[1], 12);
    shouldBe(e.errors[2], 13);
}

{
    const set = new Set([1, 2, 3, 4, 5]);
    let count = 10;
    // break the watchpoint
    Set.prototype[Symbol.iterator] = () => {
        return {
            next() {
                if (count++ > 12)
                    return { value: count, done: true };
                return { value: count, done: false };
            }
        }
    };
    const e = test(set);
    shouldBe(e.errors.length, 3);
    shouldBe(e.errors[0], 11);
    shouldBe(e.errors[1], 12);
    shouldBe(e.errors[2], 13);
}


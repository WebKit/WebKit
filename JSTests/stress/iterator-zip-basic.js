//@ requireOptions("--useIteratorZip=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

{
    const result = [...Iterator.zip([[1, 2, 3], ['a', 'b', 'c']])];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
    shouldBeArray(result[2], [3, 'c']);
}

{
    const result = [...Iterator.zip([])];
    shouldBe(result.length, 0);
}

{
    const result = [...Iterator.zip([[1, 2, 3]])];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [1]);
    shouldBeArray(result[1], [2]);
    shouldBeArray(result[2], [3]);
}

{
    const result = [...Iterator.zip([[1, 2], ['a', 'b', 'c', 'd']])];
    shouldBe(result.length, 2);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
}

{
    const result = [...Iterator.zip([[1, 2], ['a', 'b', 'c', 'd']], { mode: 'shortest' })];
    shouldBe(result.length, 2);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
}

{
    const result = [...Iterator.zip([[1, 2], ['a', 'b', 'c']], { mode: 'longest' })];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
    shouldBe(result[2][0], undefined);
    shouldBe(result[2][1], 'c');
}

{
    const result = [...Iterator.zip([[1, 2], ['a', 'b', 'c']], { mode: 'longest', padding: [0, 'z'] })];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
    shouldBeArray(result[2], [0, 'c']);
}

{
    const result = [...Iterator.zip([[1, 2], ['a', 'b']], { mode: 'strict' })];
    shouldBe(result.length, 2);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
}

{
    const customIterable = {
        [Symbol.iterator]() {
            let i = 0;
            return {
                next() {
                    if (i < 3) return { done: false, value: i++ };
                    return { done: true };
                }
            };
        }
    };
    const result = [...Iterator.zip([customIterable, [10, 20, 30]])];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [0, 10]);
    shouldBeArray(result[1], [1, 20]);
    shouldBeArray(result[2], [2, 30]);
}

{
    function* gen() {
        yield 1;
        yield 2;
        yield 3;
    }
    const result = [...Iterator.zip([gen(), ['a', 'b', 'c']])];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [1, 'a']);
    shouldBeArray(result[1], [2, 'b']);
    shouldBeArray(result[2], [3, 'c']);
}

{
    const iter = Iterator.zip([[1, 2], ['a', 'b']]);
    const first = iter.next();
    shouldBe(first.done, false);
    shouldBeArray(first.value, [1, 'a']);
    const second = iter.next();
    shouldBe(second.done, false);
    shouldBeArray(second.value, [2, 'b']);
    const third = iter.next();
    shouldBe(third.done, true);
}

{
    const result = [...Iterator.zip([[1, 2, 3], ['a', 'b', 'c'], [true, false, null]])];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], [1, 'a', true]);
    shouldBeArray(result[1], [2, 'b', false]);
    shouldBeArray(result[2], [3, 'c', null]);
}

{
    const result = [...Iterator.zip([[], []])];
    shouldBe(result.length, 0);
}

{
    let closed = false;
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const iter2 = [1, 2];
    const zipped = Iterator.zip([iter1, iter2]);
    const results = [];
    for (const item of zipped) {
        results.push(item);
    }
    shouldBe(results.length, 2);
    shouldBe(closed, true);
}

{
    let returnCalled = false;
    const iter = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { returnCalled = true; return { done: true }; }
    };
    const zipped = Iterator.zip([[1, 2, 3], iter]);
    for (const item of zipped) {
        break;
    }
    shouldBe(returnCalled, true);
}

{
    const result = [...Iterator.zip([[1], ['a', 'b', 'c', 'd']], { mode: 'longest', padding: [99] })];
    shouldBe(result.length, 4);
    shouldBeArray(result[0], [1, 'a']);
    shouldBe(result[1][0], 99);
    shouldBe(result[1][1], 'b');
    shouldBe(result[2][0], 99);
    shouldBe(result[2][1], 'c');
    shouldBe(result[3][0], 99);
    shouldBe(result[3][1], 'd');
}

{
    const set = new Set([1, 2, 3]);
    const map = new Map([['a', 1], ['b', 2], ['c', 3]]);
    const result = [...Iterator.zip([set, map])];
    shouldBe(result.length, 3);
    shouldBe(result[0][0], 1);
    shouldBeArray(result[0][1], ['a', 1]);
}

{
    const result = [...Iterator.zip([Array.from('abc'), [1, 2, 3]])];
    shouldBe(result.length, 3);
    shouldBeArray(result[0], ['a', 1]);
    shouldBeArray(result[1], ['b', 2]);
    shouldBeArray(result[2], ['c', 3]);
}

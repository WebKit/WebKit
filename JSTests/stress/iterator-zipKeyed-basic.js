//@ requireOptions("--useIteratorZip=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2, 3], b: ['x', 'y', 'z'] })];
    shouldBe(result.length, 3);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
    shouldBe(result[2].a, 3);
    shouldBe(result[2].b, 'z');
}

{
    const result = [...Iterator.zipKeyed({})];
    shouldBe(result.length, 0);
}

{
    const result = [...Iterator.zipKeyed({ nums: [1, 2, 3] })];
    shouldBe(result.length, 3);
    shouldBe(result[0].nums, 1);
    shouldBe(result[1].nums, 2);
    shouldBe(result[2].nums, 3);
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y', 'z', 'w'] })];
    shouldBe(result.length, 2);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y', 'z', 'w'] }, { mode: 'shortest' })];
    shouldBe(result.length, 2);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y', 'z'] }, { mode: 'longest' })];
    shouldBe(result.length, 3);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
    shouldBe(result[2].a, undefined);
    shouldBe(result[2].b, 'z');
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y', 'z'] }, { mode: 'longest', padding: { a: 0, b: 'pad' } })];
    shouldBe(result.length, 3);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
    shouldBe(result[2].a, 0);
    shouldBe(result[2].b, 'z');
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y'] }, { mode: 'strict' })];
    shouldBe(result.length, 2);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
}

{
    const sym = Symbol('test');
    const result = [...Iterator.zipKeyed({ a: [1, 2], [sym]: ['x', 'y'] })];
    shouldBe(result.length, 2);
    shouldBe(result[0].a, 1);
    shouldBe(result[0][sym], 'x');
    shouldBe(result[1].a, 2);
    shouldBe(result[1][sym], 'y');
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
    const result = [...Iterator.zipKeyed({ custom: customIterable, arr: [10, 20, 30] })];
    shouldBe(result.length, 3);
    shouldBe(result[0].custom, 0);
    shouldBe(result[0].arr, 10);
    shouldBe(result[1].custom, 1);
    shouldBe(result[1].arr, 20);
    shouldBe(result[2].custom, 2);
    shouldBe(result[2].arr, 30);
}

{
    function* gen() {
        yield 1;
        yield 2;
        yield 3;
    }
    const result = [...Iterator.zipKeyed({ gen: gen(), arr: ['a', 'b', 'c'] })];
    shouldBe(result.length, 3);
    shouldBe(result[0].gen, 1);
    shouldBe(result[0].arr, 'a');
    shouldBe(result[1].gen, 2);
    shouldBe(result[1].arr, 'b');
    shouldBe(result[2].gen, 3);
    shouldBe(result[2].arr, 'c');
}

{
    const iter = Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y'] });
    const first = iter.next();
    shouldBe(first.done, false);
    shouldBe(first.value.a, 1);
    shouldBe(first.value.b, 'x');
    const second = iter.next();
    shouldBe(second.done, false);
    shouldBe(second.value.a, 2);
    shouldBe(second.value.b, 'y');
    const third = iter.next();
    shouldBe(third.done, true);
}

{
    const result = [...Iterator.zipKeyed({ a: [1, 2, 3], b: ['x', 'y', 'z'], c: [true, false, null] })];
    shouldBe(result.length, 3);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[0].c, true);
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 'y');
    shouldBe(result[1].c, false);
    shouldBe(result[2].a, 3);
    shouldBe(result[2].b, 'z');
    shouldBe(result[2].c, null);
}

{
    let closed = false;
    const iter1 = {
        [Symbol.iterator]() { return this; },
        next() { return { done: false, value: 1 }; },
        return() { closed = true; return { done: true }; }
    };
    const zipped = Iterator.zipKeyed({ iter1, arr: [1, 2] });
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
    const zipped = Iterator.zipKeyed({ arr: [1, 2, 3], iter });
    for (const item of zipped) {
        break;
    }
    shouldBe(returnCalled, true);
}

{
    const result = [...Iterator.zipKeyed({ a: [1], b: ['x', 'y', 'z', 'w'] }, { mode: 'longest', padding: { a: 99 } })];
    shouldBe(result.length, 4);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 'x');
    shouldBe(result[1].a, 99);
    shouldBe(result[1].b, 'y');
    shouldBe(result[2].a, 99);
    shouldBe(result[2].b, 'z');
    shouldBe(result[3].a, 99);
    shouldBe(result[3].b, 'w');
}

{
    const obj = { a: [1, 2], b: [3, 4] };
    Object.defineProperty(obj, 'hidden', {
        value: [5, 6],
        enumerable: false
    });
    const result = [...Iterator.zipKeyed(obj)];
    shouldBe(result.length, 2);
    shouldBe(result[0].a, 1);
    shouldBe(result[0].b, 3);
    shouldBe(result[0].hidden, undefined);
    shouldBe(result[1].a, 2);
    shouldBe(result[1].b, 4);
    shouldBe(result[1].hidden, undefined);
}

{
    const result = [...Iterator.zipKeyed({ str: Array.from('abc'), arr: [1, 2, 3] })];
    shouldBe(result.length, 3);
    shouldBe(result[0].str, 'a');
    shouldBe(result[0].arr, 1);
    shouldBe(result[1].str, 'b');
    shouldBe(result[1].arr, 2);
    shouldBe(result[2].str, 'c');
    shouldBe(result[2].arr, 3);
}

{
    const result = [...Iterator.zipKeyed({ a: undefined, b: [1, 2] })];
    shouldBe(result.length, 2);
    shouldBe(result[0].b, 1);
    shouldBe(result[0].a, undefined);
    shouldBe(result[1].b, 2);
    shouldBe(result[1].a, undefined);
}

{
    const result = Iterator.zipKeyed({ a: [1, 2], b: ['x', 'y'] }).next().value;
    shouldBe(Object.getPrototypeOf(result), null);
}

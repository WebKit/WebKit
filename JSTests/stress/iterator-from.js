//@ requireOptions("--useIteratorHelpers=1")

function assert(a, text) {
    if (!a)
        throw new Error(`Failed assertion: ${text}`);
}

function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldThrow(fn, error, message) {
    try {
        fn();
        throw new Error('Expected to throw, but succeeded');
    } catch (e) {
        if (!(e instanceof error))
            throw new Error(`Expected to throw ${error.name} but got ${e.name}`);
        if (e.message !== message)
            throw new Error(`Expected ${error.name} with '${message}' but got '${e.message}'`);
    }
}

{
    class Iter {
        next() {
            return { done: false, value: 1 };
        }
    }
    const iter = new Iter();
    const wrapper = Iterator.from(iter);
    const result = wrapper.next();
    sameValue(result.value, 1);
    sameValue(result.done, false);
}

{
    const iter = {};
    const wrapper = Iterator.from(iter);
    const result = wrapper.return();
    assert(Object.hasOwn(result, "value"), "Object.hasOwn(result, 'value')");
    sameValue(result.value, undefined);
    sameValue(result.done, true);
}

{
    class Iter {
        return() {
            return { done: true, value: 5 };
        }
    }
    const iter = new Iter();
    const wrapper = Iterator.from(iter);
    const result = wrapper.return();
    sameValue(result.value, 5);
    sameValue(result.done, true);
}

{
    class Iter extends Iterator {
        next() {
            return { done: false, value: 1 };
        }
    }
    const iter = new Iter();
    const wrapper = Iterator.from(iter);
    assert(wrapper instanceof Iterator, "wrapper instanceof Iterator");
    assert(wrapper === iter, "wrapper === iter");
    const result = wrapper.next();
    sameValue(result.value, 1);
    sameValue(result.done, false);
}

{
    let nextCount = 0;
    let returnCount = 0;

    const iter = {
        get next() {
            nextCount++;
            return function () {
                return { done: false, value: 1 };
            };
        },
        get return() {
            returnCount++;
            return function () {
                return { done: true, value: 5 };
            };
        },
    };

    const wrapper = Iterator.from(iter);
    sameValue(nextCount, 1);
    sameValue(returnCount, 0);

    const nextResult = wrapper.next();
    sameValue(nextCount, 1);
    sameValue(returnCount, 0);
    sameValue(nextResult.value, 1);
    sameValue(nextResult.done, false);

    const returnResult = wrapper.return();
    sameValue(nextCount, 1);
    sameValue(returnCount, 1);
    sameValue(returnResult.value, 5);
    sameValue(returnResult.done, true);
}

{
    let nextCount = 0;
    let returnCount = 0;

    class Iter extends Iterator {}
    const iter = new Iter();
    Object.defineProperties(iter, {
        next: {
            get() {
                nextCount++;
                return function () {
                    return { done: false, value: 1 };
                };
            },
        },
        return: {
            get() {
                returnCount++;
                return function () {
                    return { done: true, value: 5 };
                };
            },
        },
    });
    const wrapper = Iterator.from(iter);
    assert(wrapper instanceof Iterator);
    sameValue(nextCount, 1);
    sameValue(returnCount, 0);

    const nextResult = wrapper.next();
    sameValue(nextCount, 2);
    sameValue(returnCount, 0);
    sameValue(nextResult.value, 1);
    sameValue(nextResult.done, false);

    const returnResult = wrapper.return();
    sameValue(nextCount, 2);
    sameValue(returnCount, 1);
    sameValue(returnResult.value, 5);
    sameValue(returnResult.done, true);
}

{
    const iterator = Iterator.from("string");
    sameValue(iterator.toString(), "[object String Iterator]");
}

{
    const invalidIterators = [
        1,
        1n,
        true,
        false,
        null,
        undefined,
        Symbol("symbol"),
    ];
    for (const invalidIterator of invalidIterators) {
        shouldThrow(() => { Iterator.from(invalidIterator) }, TypeError, "Iterafor.from requires an object or string");
    }
}

{
    const iter = {};
    const wrapper = Iterator.from(iter);
    const WrapForValidIteratorPrototypeNext = wrapper.next.bind(null);
    const WrapForValidIteratorPrototypeReturn = wrapper.return.bind(null);

    shouldThrow(() => {
        WrapForValidIteratorPrototypeNext.call({})
    }, TypeError, "%WrapForValidIteratorPrototype%.next requires that |this| be a WrapForValidIteratorPrototype object")

    shouldThrow(() => {
        WrapForValidIteratorPrototypeReturn.call({})
    }, TypeError, "%WrapForValidIteratorPrototype%.next requires that |this| be a WrapForValidIteratorPrototype object")
}

{
    let symbolIteraterGetCount = 0;
    const iter = {
        get [Symbol.iterator]() {
          symbolIteraterGetCount++;
          return function() { return this };
        },
    };
    const wrapper = Iterator.from(iter);
    sameValue(symbolIteraterGetCount, 1);
}

{
    const iter = {
        [Symbol.iterator]() {
          return 3;
        },
    };
    shouldThrow(() => { Iterator.from(iter); }, TypeError, "Iterator is not an object");
}

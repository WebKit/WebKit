function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("Bad value: " + String(actual) + ", expected: " + String(expected));
}

function shouldBeArray(array, expected) {
    shouldBe(array.length, expected.length);
    for (let i = 0; i < expected.length; ++i) {
        shouldBe(Object.hasOwn(array, i), Object.hasOwn(expected, i));
        shouldBe(array[i], expected[i]);
    }
}

const object = {};
const makers = [j => j, j => j + 0.5, j => "s" + j];

// Each indexing type gets a function of its own, so that DFG / FTL compile it to a monomorphic ArrayUnshift.
// The source text differs each time because functions from the same source share their array profiles.
let unshiftCount = 0;
function makeUnshift() {
    const unshift = new Function("array", "value", `return array.unshift(value); // ${unshiftCount++}`);
    noInline(unshift);
    return unshift;
}

// `length` elements, with room for one more. The array allocation profile remembers what became of earlier arrays,
// so the code is also renewed by resetMakeArray(), to keep getting Int32 and Double arrays.
let makeArrayImpl;
let makeArrayCount = 0;
function resetMakeArray() {
    makeArrayImpl = new Map();
}
resetMakeArray();

function makeArray(make, length) {
    let impl = makeArrayImpl.get(make);
    if (!impl) {
        impl = new Function("make", "length", `
            const array = [];
            for (let j = 0; j <= length; ++j)
                array.push(make(j));
            array.pop();
            return array; // ${makeArrayCount++}`);
        makeArrayImpl.set(make, impl);
    }
    return impl(make, length);
}

function warmUp(make) {
    resetMakeArray();
    const unshift = makeUnshift();
    for (let i = 0; i < testLoopCount; ++i) {
        const value = make(9);
        const array = makeArray(make, 3);
        shouldBe(unshift(array, value), 4);
        shouldBeArray(array, [value, make(0), make(1), make(2)]);
    }
    return unshift;
}

for (const make of makers) {
    const unshift = warmUp(make);

    // With and without spare capacity.
    for (let length = 0; length < 140; ++length) {
        for (const spare of [true, false]) {
            let array = makeArray(make, length);
            if (!spare)
                array = array.slice();
            const value = make(1000);
            const expected = [value];
            for (let j = 0; j < length; ++j)
                expected.push(make(j));
            shouldBe(unshift(array, value), length + 1);
            shouldBeArray(array, expected);
        }
    }

    // Growing from empty.
    for (let i = 0; i < 100; ++i) {
        const array = [];
        for (let j = 0; j < 200; ++j)
            shouldBe(unshift(array, make(j)), j + 1);
        for (let j = 0; j < 200; ++j)
            shouldBe(array[j], make(199 - j));
        shouldBe(200 in array, false);
    }
}

// Around MIN_SPARSE_ARRAY_INDEX.
for (const make of makers) {
    const unshift = warmUp(make);
    for (let length = 99998; length <= 100002; ++length) {
        const array = makeArray(make, length);
        const value = make(7);
        shouldBe(unshift(array, value), length + 1);
        shouldBe(array.length, length + 1);
        shouldBe(array[0], value);
        for (let j = 0; j < length; j += 997)
            shouldBe(array[j + 1], make(j));
        shouldBe(array[length], make(length - 1));
    }
}

// Holes move along with the elements.
for (const make of makers) {
    const unshift = warmUp(make);
    const value = make(9);
    for (let hole = 0; hole < 3; ++hole) {
        for (let i = 0; i < 1000; ++i) {
            const array = makeArray(make, 3);
            delete array[hole];
            const expected = [value, make(0), make(1), make(2)];
            delete expected[hole + 1];
            shouldBe(unshift(array, value), 4);
            shouldBeArray(array, expected);
        }
    }
}

// Values that do not fit in the indexing type.
for (const make of makers) {
    const unshift = warmUp(make);
    for (const value of [1, 1.5, NaN, -0, Infinity, object, undefined, null, "s"]) {
        resetMakeArray();
        for (let i = 0; i < 10; ++i) {
            const array = makeArray(make, 3);
            shouldBe(unshift(array, value), 4);
            shouldBeArray(array, [value, make(0), make(1), make(2)]);
        }
    }
}

// Copy-on-write arrays.
for (const literal of [() => [1, 2, 3], () => [1.5, 2.5, 3.5], () => ["a", "b", "c"]]) {
    const unshift = makeUnshift();
    const original = literal().slice();
    for (let i = 0; i < testLoopCount; ++i) {
        const array = literal();
        shouldBe(unshift(array, original[1]), 4);
        shouldBe(array[0], original[1]);
        shouldBe(array[3], original[2]);
        const fresh = literal();
        shouldBe(fresh.length, 3);
        shouldBe(fresh[0], original[0]);
    }
}

// Arrays that cannot grow.
for (const make of makers) {
    const unshift = warmUp(make);
    for (let i = 0; i < 100; ++i) {
        for (const restrict of [Object.freeze, Object.seal, Object.preventExtensions, array => Object.defineProperty(array, "length", { writable: false })]) {
            let error = null;
            try {
                unshift(restrict(makeArray(make, 3)), make(0));
            } catch (e) {
                error = e;
            }
            shouldBe(error instanceof TypeError, true);
        }
    }
}

// Subclasses.
for (const make of makers) {
    class Derived extends Array { }
    const unshift = makeUnshift();
    for (let i = 0; i < testLoopCount; ++i) {
        const array = new Derived();
        array.push(make(0), make(1), make(2), make(3));
        array.pop();
        shouldBe(unshift(array, make(9)), 4);
        shouldBe(array instanceof Derived, true);
        shouldBeArray(array, [make(9), make(0), make(1), make(2)]);
    }
}

// Holes that have to be read through the prototype.
resetMakeArray();
function makeHoley(make) {
    const array = makeArray(make, 3);
    delete array[1];
    return array;
}

for (const make of makers) {
    const prototype = { __proto__: Array.prototype, 1: "proto" };
    const unshift = makeUnshift();
    for (let i = 0; i < testLoopCount; ++i) {
        const array = Object.setPrototypeOf(makeHoley(make), prototype);
        shouldBe(unshift(array, make(9)), 4);
        shouldBeArray(array, [make(9), make(0), "proto", make(2)]);
    }
}

{
    const unshifts = makers.map(make => {
        const unshift = makeUnshift();
        for (let i = 0; i < testLoopCount; ++i) {
            const array = makeHoley(make);
            shouldBe(unshift(array, make(9)), 4);
            shouldBe(Object.hasOwn(array, 2), false);
        }
        return unshift;
    });

    Array.prototype[1] = "proto";
    makers.forEach((make, index) => {
        const array = makeHoley(make);
        shouldBe(unshifts[index](array, make(9)), 4);
        shouldBeArray(array, [make(9), make(0), "proto", make(2)]);
    });
    delete Array.prototype[1];
}

// Having a bad time.
{
    const unshifts = makers.map(warmUp);
    let setterCalls = 0;
    Object.defineProperty(Array.prototype, 3, { configurable: true, get() { return undefined; }, set() { ++setterCalls; } });
    makers.forEach((make, index) => {
        const array = [make(0), make(1), make(2)];
        shouldBe(unshifts[index](array, make(9)), 4);
        shouldBe(array[0], make(9));
        shouldBe(array[2], make(1));
        shouldBe(Object.hasOwn(array, 3), false);
        shouldBe(setterCalls, index + 1);
    });
    delete Array.prototype[3];
}

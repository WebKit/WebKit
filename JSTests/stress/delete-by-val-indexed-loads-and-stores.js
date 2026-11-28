function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + String(actual) + " expected: " + String(expected));
}

function around(object, index, oldValue, newValue) {
    const before = object[index];
    const deleted = delete object[index];
    const after = object[index];
    object[index] = newValue;
    return deleted && before === oldValue && after === undefined && object[index] === newValue;
}

function copyOf(func) {
    const copy = new Function("return " + func.toString())();
    noInline(copy);
    return copy;
}

const arrayInt32 = copyOf(around);
const arrayDouble = copyOf(around);
const arrayContiguous = copyOf(around);
const objectInt32 = copyOf(around);
const objectDouble = copyOf(around);
const objectContiguous = copyOf(around);

function sumAfterDelete(array) {
    let sum = 0;
    for (let i = 0; i < array.length; ++i) {
        sum += array[i];
        if (i === 1)
            delete array[2];
    }
    return sum;
}
const sumInt32 = copyOf(sumAfterDelete);
const sumDouble = copyOf(sumAfterDelete);

// The result is used by a branch, and the base and the index are live after the delete.
function removeAll(object, count) {
    let removed = 0;
    for (let i = 0; i < count; ++i) {
        if (delete object[i])
            removed += i in object ? 0 : i + 1;
    }
    return removed;
}
noInline(removeAll);

// A phantom allocation is live across the delete.
function sink(array, a, b) {
    const object = { x: a, y: b };
    return delete array[a] ? object.x + object.y : 0;
}
noInline(sink);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(arrayInt32([i, 1, 2], 1, 1, 7), true);
    shouldBe(arrayDouble([i + 0.5, 1.5, 2.5], 1, 1.5, 7.5), true);
    shouldBe(arrayContiguous([i, "a", null], 1, "a", "b"), true);
    shouldBe(objectInt32({ 0: i, 1: 1, 2: 2 }, 1, 1, 7), true);
    shouldBe(objectDouble({ 0: i + 0.5, 1: 1.5, 2: 2.5 }, 1, 1.5, 7.5), true);
    shouldBe(objectContiguous({ 0: i, 1: "a", 2: null }, 1, "a", "b"), true);
    shouldBe(sumInt32([i, 1, 2, 3]), NaN);
    shouldBe(sumDouble([i + 0.5, 1.5, 2.5, 3.5]), NaN);
    shouldBe(sink([0, 1, 2], 1, i), i + 1);
}

for (let i = 0; i < testLoopCount / 10; ++i) {
    shouldBe(removeAll([1, 2, 3, 4], 4), 10);
    shouldBe(removeAll({ 0: 1.5, 1: 2.5, 2: 3.5 }, 3), 6);
    shouldBe(removeAll({ 0: "a", 1: "b", 2: { } }, 5), 15);
}
shouldBe(removeAll(Object.freeze({ 0: 1, 1: 2 }), 2), 0);

function removeAndGet(array, index) {
    delete array[index];
    return array[index];
}
noInline(removeAndGet);

const live = [];
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(removeAndGet([i, 1, 2], 1), undefined);
for (let i = 0; i < 4; ++i)
    live.push([i, 1, 2], [0.5, 1.5, 2.5], ["a", "b", { }]);
Array.prototype[1] = "from the prototype";
for (let i = 0; i < testLoopCount / 10; ++i)
    shouldBe(removeAndGet([i, 1, 2], 1), "from the prototype");
Object.defineProperty(Array.prototype, 1, { get() { return "getter"; }, configurable: true });
for (const array of live)
    shouldBe(removeAndGet(array, 1), "getter");
for (let i = 0; i < testLoopCount / 10; ++i)
    shouldBe(removeAndGet([i, 1, 2], 1), "getter");
shouldBe(removeAndGet(Array.prototype, 1), undefined);

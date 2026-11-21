function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL " + msg + ": got " + actual + ", expected " + expected);
}

// Arrays whose elements repeat, in the same order, in rotated or reversed orders, mixed with elements that are not
// repeated, and with enough distinct elements in between that the ones seen before are forgotten.
const pool = [];
for (let i = 0; i < 24; ++i)
    pool.push({ functionName: "f" + i, url: "https://example.com/" + (i % 5) + ".js", scriptId: String(i % 3), lineNumber: i, columnNumber: i * 7 });
pool.push({ type: "click" }, { a: [], b: { c: 1 } }, { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 });

function makeArray(i) {
    const length = 1 + i % 6;
    const result = [];
    for (let j = 0; j < length; ++j) {
        let index;
        switch (i % 4) {
        case 0: index = j; break;
        case 1: index = (j + i) % pool.length; break;
        case 2: index = (length - j) % pool.length; break;
        default: index = (i * 7 + j * 3) % pool.length; break;
        }
        result.push(pool[index]);
        if (i % 9 === 0 && j === 1)
            result.push({ time: i + 0.5, unique: "u" + i });
        if (i % 11 === 0 && j === 0)
            result.push({ nested: [pool[(i + 1) % pool.length], pool[(i + 2) % pool.length]] });
    }
    return result;
}

function check(parsed, expected, label) {
    shouldBe(JSON.stringify(parsed), expected, label + " text");
    const seen = new Set();
    for (const { a: array } of parsed) {
        shouldBe(Array.isArray(array), true, label + " array");
        for (const element of array) {
            shouldBe(seen.has(element), false, label + " distinct elements");
            seen.add(element);
        }
    }
}

for (let round = 0; round < 4; ++round) {
    const arrays = [];
    for (let i = 0; i < 600; ++i)
        arrays.push({ a: makeArray(i + round) });
    const text = JSON.stringify(arrays);
    for (let repeat = 0; repeat < 3; ++repeat) {
        const parsed = JSON.parse(text);
        check(parsed, text, "round " + round + " repeat " + repeat);
        parsed[0].a[0].functionName = "changed";
        shouldBe(JSON.parse(text)[0].a[0].functionName === "changed", false, "elements are not shared between parses");
    }

    // Many elements seen only once replace the templates of the repeated ones.
    const unique = [];
    for (let i = 0; i < 400; ++i)
        unique.push({ a: [{ id: "x" + round + "_" + i, value: i }, { id: "y" + round + "_" + i, value: i }] });
    const uniqueText = JSON.stringify(unique);
    shouldBe(JSON.stringify(JSON.parse(uniqueText)), uniqueText, "round " + round + " unique");
    check(JSON.parse(text), text, "round " + round + " after unique");
}

const other = createGlobalObject();
const text = JSON.stringify([{ a: makeArray(1) }, { a: makeArray(5) }, { a: makeArray(1) }]);
for (let round = 0; round < 3; ++round) {
    for (const { a: array } of JSON.parse(text)) {
        for (const element of array)
            shouldBe(Object.getPrototypeOf(element), Object.prototype, "this global object " + round);
    }
    for (const { a: array } of other.JSON.parse(text)) {
        for (const element of array)
            shouldBe(Object.getPrototypeOf(element), other.Object.prototype, "other global object " + round);
    }
}

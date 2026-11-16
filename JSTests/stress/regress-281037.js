load("./resources/typedarray-test-helper-functions.js", "caller relative");

function shouldBeArray(actual, expected) {
    const isEqual = actual.length === expected.length && actual.every((item, index) => item === expected[index]);
    if (!isEqual)
        throw new Error(`Expected [${actual.map(String)}] to equal [${expected.map(String)}]`);
}

shouldBeArray(Array.from([0, 1, 2], makeMasquerader()), [null, null, null]);
shouldBeArray(Array.from([0, 1, 2].values(), makeMasquerader()), [null, null, null]);
shouldBeArray(Array.from({0:0, 1:1, 2:2, length: 3}, makeMasquerader()), [null, null, null]);

for (const TypedArray of typedArrays) {
    shouldBeArray(TypedArray.from([0, 1, 2], makeMasquerader()), [0, 0, 0]);
    shouldBeArray(TypedArray.from([0, 1, 2].values(), makeMasquerader()), [0, 0, 0]);
    shouldBeArray(TypedArray.from({0:0, 1:1, 2:2, length: 3}, makeMasquerader()), [0, 0, 0]);
}

(async function() {
    shouldBeArray(await Array.fromAsync([0, 1, 2], makeMasquerader()), [null, null, null]);
    shouldBeArray(await Array.fromAsync([0, 1, 2].values(), makeMasquerader()), [null, null, null]);
    shouldBeArray(await Array.fromAsync({0:0, 1:1, 2:2, length: 3}, makeMasquerader()), [null, null, null]);
})().catch(err => { $vm.abort(err); });
drainMicrotasks();

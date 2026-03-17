//@ requireOptions("--useConcurrentJIT=0", "--useB3CanonicalizePrePostIncrements=1")

const arr1 = new Int32Array(256);
arr1.fill(0x1337)
const arr2 = new Int32Array(256);
arr2.fill(0x1337)

function trigger(arr, cond) {
    if (arr.length !== 256) return 0; // Eliminate bounds check side-exits

    let res = 0;
    for (let j = 0; j < 2; j++) {
        if (cond) {
            Atomics.add(arr, 1, 1337);
        }

        res = arr[1];
    }
    return res;
}

for (let i = 0; i < 100000; i++) {
    let ret = trigger(i % 2 === 0 ? arr1 : arr2, i % 2 === 0);
}

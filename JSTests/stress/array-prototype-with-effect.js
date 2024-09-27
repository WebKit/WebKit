function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const arr = [0, 1, 2];

let get0Count = 0;
Object.defineProperty(arr, "0", {
    get() {
        get0Count++;
        return 0;
    }
});

arr.with(1, 2);
sameValue(get0Count, 1);
arr.with(2, 2);
sameValue(get0Count, 2);
arr.with(0, 2);
sameValue(get0Count, 2);

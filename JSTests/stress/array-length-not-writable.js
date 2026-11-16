function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}

let arr = [];
assert(arr.length === 0);
Object.freeze(arr);
assert(arr.length === 0);
arr.length = 5;
assert(arr.length === 0);
arr.length = "test";
assert(arr.length === 0);

arr = [1];
assert(arr.length === 1);
Object.defineProperty(arr, "length", {value: 3, writable: false});
assert(arr.length === 3);
arr.length = 5;
assert(arr.length === 3);
arr.length = "test";
assert(arr.length === 3);

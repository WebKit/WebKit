//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

let n = 1000;
let arr = Array(n);

for(let i = 0; i < n; i++) {
    arr[i] = eval(`() => ${i}`);
    assert(arr[i](), i);
}

for(let i = 0; i < n; i++) {
    assert(arr[i](), i);
}


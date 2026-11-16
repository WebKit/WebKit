function assert(b) {
    if (!b)
        throw new Error;
}
function foo(a) {
    a[1] = 1;
    let b = 0;
    for (let j = 0; j < 10; j++) {
        a[1] = 2;
        b = a[1];
    }
    return b;
}
noInline(foo);

let arr = new Array(5);
for (let i = 0; i < 0x1000; i++) {
    arr[i] = i;
}
arr[100000] = 1;

for (let i = 0; i < 20000; i++){
    arr[1] = 1;
    assert(foo(arr) === 2);
}

let buffer = new ArrayBuffer(4);
let int32View = new Int32Array(buffer);
int32View[0] = -1;
let floatView = new Float32Array(buffer);

function foo() {
    let tmp = floatView[0];
    for (let i = 0; i < 10000; ++i) { }
    if (tmp) {}
}

for (let i = 0; i < 100; ++i)
    foo();

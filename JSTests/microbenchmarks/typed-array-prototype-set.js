function test() {
    let x = new Int8Array(10000);
    let y = new Uint8Array(10000);
    for (let i = 0; i < x.length; ++i)
        x[i] = i;
    for (let i = 0; i < 500; ++i)
        y.set(x);
    for (let i = 0; i < 500; ++i)
        x.set(y);
}

function test2() {
    let x = new Int16Array(10000);
    let y = new Uint16Array(10000);
    for (let i = 0; i < x.length; ++i)
        x[i] = i;
    for (let i = 0; i < 500; ++i)
        y.set(x);
    for (let i = 0; i < 500; ++i)
        x.set(y);
}

function test3() {
    let x = new Int32Array(10000);
    let y = new Uint32Array(10000);
    for (let i = 0; i < x.length; ++i)
        x[i] = i;
    for (let i = 0; i < 500; ++i)
        y.set(x);
    for (let i = 0; i < 500; ++i)
        x.set(y);
}

function test4() {
    let x = new Uint8ClampedArray(10000);
    let y = new Int8Array(10000);
    for (let i = 0; i < x.length; ++i)
        x[i] = i;
    for (let i = 0; i < 500; ++i)
        y.set(x);
}

test();
test2();
test3();
test4();

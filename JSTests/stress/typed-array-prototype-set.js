function assert(b) {
    if (!b)
        throw new Error;
}

function test1() {
    let x = new Int8Array(1);
    let y = new Uint8Array(1);
    x[0] = -1;
    y.set(x);
    assert(y[0] === 255);
}
test1();

function test2() {
    let x = new Int8Array(1);
    let y = new Uint8Array(1);
    y[0] = 255;
    x.set(y);
    assert(x[0] === -1);
}
test2();

function test3() {
    let x = new Int16Array(1);
    let y = new Uint16Array(1);
    x[0] = -1;
    y.set(x);
    assert(y[0] === 65535);
}
test3();

function test4() {
    let x = new Int16Array(1);
    let y = new Uint16Array(1);
    y[0] = 65535;
    x.set(y);
    assert(x[0] === -1);
}
test4();

function test5() {
    let x = new Int32Array(1);
    let y = new Uint32Array(1);
    x[0] = -1;
    y.set(x);
    assert(y[0] === 4294967295);
}
test5();

function test6() {
    let x = new Int32Array(1);
    let y = new Uint32Array(1);
    y[0] = 4294967295;
    x.set(y);
    assert(x[0] === -1);
}
test6();

function test7() {
    let x = new Uint8ClampedArray(1);
    let y = new Int8Array(1);
    x[0] = 255;
    y.set(x);
    assert(y[0] === -1);
}
test7();

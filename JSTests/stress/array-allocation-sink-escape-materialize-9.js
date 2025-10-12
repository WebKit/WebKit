function testInt32Array(idx) {
    let a = new Array(4);
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a[3] = 4;

    let len = a.length;

    if (idx % 11 === 0) {
        return a;
    }
    return len + a[idx % 4];
}

function testDoubleArray(idx) {
    let a = new Array(4);
    a[0] = 1.5;
    a[1] = 2.7;
    a[2] = 3.9;
    a[3] = 4.1;

    let len = a.length;

    if (idx % 13 === 0) {
        return a;
    }
    return len + a[idx % 4];
}

function testContiguousArray(idx) {
    let a = new Array(4);
    a[0] = "hello";
    a[1] = 42;
    a[2] = 3.14;
    a[3] = null;

    let len = a.length;

    if (idx % 17 === 0) {
        return a;
    }
    return len;
}

noInline(testInt32Array);
noInline(testDoubleArray);
noInline(testContiguousArray);

for (let i = 0; i < testLoopCount; i++) {
    testInt32Array(i);
    testDoubleArray(i);
    testContiguousArray(i);
}
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test1(view) {
    return view[1023];
}
noInline(test1);

{
    var arrayView1 = new Int8Array(new SharedArrayBuffer(1024));
    arrayView1[1023] = 42;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(test1(arrayView1), 42);
    arrayView1[1023] = 41;
    shouldBe(test1(arrayView1), 41);

    var arrayView2 = new Int8Array(new SharedArrayBuffer(1024, { maxByteLength: 4096 }));
    arrayView2[1023] = 42;
    shouldBe(test1(arrayView2), 42);
    arrayView2.buffer.grow(4096);
    shouldBe(test1(arrayView2), 42);

    for (var i = 0; i < 1e4; ++i)
        shouldBe(test1(arrayView2), 42);
    shouldBe(test1(arrayView1), 41);
}

function test2(view) {
    return view[1023];
}
noInline(test2);

{
    var arrayView1 = new Int8Array(new SharedArrayBuffer(1024));
    arrayView1[1023] = 42;
    for (var i = 0; i < 1e4; ++i)
        shouldBe(test2(arrayView1), 42);
    arrayView1[1023] = 41;
    shouldBe(test2(arrayView1), 41);

    var arrayView2 = new Int8Array(new SharedArrayBuffer(1024, { maxByteLength: 4096 }), 0, 1024);
    arrayView2[1023] = 42;
    shouldBe(test2(arrayView2), 42);
    shouldBe(arrayView2.length, 1024);
    arrayView2.buffer.grow(4096);
    shouldBe(arrayView2.length, 1024);
    shouldBe(test2(arrayView2), 42);

    for (var i = 0; i < 1e4; ++i)
        shouldBe(test2(arrayView2), 42);
    shouldBe(test2(arrayView1), 41);
}

function test3(view) {
    return view.getInt8(1023);
}
noInline(test3);

{
    var arrayView1 = new DataView(new SharedArrayBuffer(1024));
    arrayView1.setInt8(1023, 42);
    for (var i = 0; i < 1e4; ++i)
        shouldBe(test3(arrayView1), 42);
    arrayView1.setInt8(1023, 41);
    shouldBe(test3(arrayView1), 41);

    var arrayView2 = new DataView(new SharedArrayBuffer(1024, { maxByteLength: 4096 }), 0, 1024);
    arrayView2.setInt8(1023, 42);
    shouldBe(test3(arrayView2), 42);
    shouldBe(arrayView2.byteLength, 1024);
    arrayView2.buffer.grow(4096);
    shouldBe(arrayView2.byteLength, 1024);
    shouldBe(test3(arrayView2), 42);
    shouldBe(arrayView2.byteLength, 1024);
    shouldBe(test3(arrayView2), 42);

    for (var i = 0; i < 1e4; ++i)
        shouldBe(test3(arrayView2), 42);
    shouldBe(test3(arrayView1), 41);
}

function test4(view) {
    return view.getInt8(1023);
}
noInline(test4);

{
    var arrayView1 = new DataView(new SharedArrayBuffer(1024));
    arrayView1.setInt8(1023, 42);
    for (var i = 0; i < 1e4; ++i)
        shouldBe(test4(arrayView1), 42);
    arrayView1.setInt8(1023, 41);
    shouldBe(test4(arrayView1), 41);

    var arrayView2 = new DataView(new SharedArrayBuffer(1024, { maxByteLength: 4096 }));
    arrayView2.setInt8(1023, 42);
    shouldBe(test4(arrayView2), 42);
    shouldBe(arrayView2.byteLength, 1024);
    arrayView2.buffer.grow(4096);
    shouldBe(arrayView2.byteLength, 4096);
    shouldBe(test4(arrayView2), 42);
    shouldBe(arrayView2.byteLength, 4096);
    shouldBe(test4(arrayView2), 42);

    for (var i = 0; i < 1e4; ++i)
        shouldBe(test4(arrayView2), 42);
    shouldBe(test4(arrayView1), 41);
}

var array = new Uint8Array(new SharedArrayBuffer(1 << 16));
for (var i = 0; i < array.length; ++i)
    array[i] = (i * 7919) & 0xff;

function test(array) {
    return array.join();
}
noInline(test);

var expected = test(array).length;
var total = 0;
for (var i = 0; i < 100; ++i)
    total += test(array).length;
if (total !== expected * 100)
    throw new Error("bad result: " + total);

var array = new BigInt64Array(1 << 14);
for (var i = 0; i < array.length; ++i)
    array[i] = BigInt(i) * -0x9e3779b97f4an;

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

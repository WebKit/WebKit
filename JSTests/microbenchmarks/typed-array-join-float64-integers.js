var array = new Float64Array(1 << 16);
for (var i = 0; i < array.length; ++i)
    array[i] = ((i * 7919) & 0xffff) - 0x8000;

function test(array) {
    return array.join();
}
noInline(test);

var expected = test(array).length;
var total = 0;
for (var i = 0; i < 50; ++i)
    total += test(array).length;
if (total !== expected * 50)
    throw new Error("bad result: " + total);

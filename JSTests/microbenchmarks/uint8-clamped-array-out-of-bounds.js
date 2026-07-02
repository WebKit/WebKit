function test(array, length)
{
    var result = 0;
    for (var i = 0; i < length; ++i) {
        result += array[i] | 0;
    }
    return result;
}
noInline(test);

var array = new Uint8ClampedArray(1024);
for (var i = 0; i < array.length; ++i)
    array[i] = i & 0xff;

for (var i = 0; i < 1e2; ++i)
    test(array, 10000);

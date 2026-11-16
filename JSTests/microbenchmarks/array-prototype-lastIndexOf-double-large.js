function test(array, searchElement) {
    return array.lastIndexOf(searchElement);
}
noInline(test);

var array = new Array(1024);
for (var i = 0; i < array.length; i++)
    array[i] = i + 0.5;

var result = 0;
for (var i = 0; i < 1e6; ++i)
    result += test(array, -1.0); // absent: forces a full backward scan, returns -1

if (result !== -1e6)
    throw new Error("bad result: " + result);

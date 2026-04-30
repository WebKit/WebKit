var array = new Array(4096);
for (var i = 0; i < array.length; ++i)
    array[i] = { value: i };
var needle = array[1];
var missing = { value: -1 };

function test(array) {
    var result = 0;
    result += array.lastIndexOf(needle);
    result += array.lastIndexOf(missing);
    return result;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test(array);

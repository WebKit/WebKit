var array = new Array(4096);
for (var i = 0; i < array.length; ++i)
    array[i] = i;

function test(array) {
    var result = 0;
    result += array.lastIndexOf(1);
    result += array.lastIndexOf(-1);
    return result;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test(array);

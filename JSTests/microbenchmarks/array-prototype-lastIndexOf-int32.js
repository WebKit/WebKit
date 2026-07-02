function test(array, searchElement) {
    return array.lastIndexOf(searchElement);
}
noInline(test);

var array = new Array(1024);
for (var i = 0; i < array.length; i++)
    array[i] = i;

for (var i = 0; i < testLoopCount; ++i)
    test(array, i % 1024);

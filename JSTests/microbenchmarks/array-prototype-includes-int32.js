function test(array, searchElement) {
    return array.includes(searchElement);
}
noInline(test);

var array = new Array(1024);
for (var i = 0; i < array.length; i++)
    array[i] = i;

for (var i = 0; i < 1e6; ++i)
    test(array, 512);

function test(array, searchElement) {
    return array.indexOf(searchElement);
}
noInline(test);

var array = new Array(1024);
for (var i = 0; i < array.length; i++)
    array[i] = i + 0.5;

for (var i = 0; i < 1e6; ++i)
    test(array, (i % 1024) + 0.5);

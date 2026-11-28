function sum(array) {
    var result = 0;
    for (var value of array)
        result += value;
    return result;
}
noInline(sum);

var array = new Float64Array(1024);
for (var i = 0; i < array.length; ++i)
    array[i] = i;

for (var i = 0; i < 2e4; ++i) {
    if (sum(array) !== 523776)
        throw new Error("bad sum");
}

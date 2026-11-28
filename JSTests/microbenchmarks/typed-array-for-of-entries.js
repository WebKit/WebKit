function sum(array) {
    var result = 0;
    for (var [index, value] of array.entries())
        result += index + value;
    return result;
}
noInline(sum);

var array = new Int32Array(1024);
for (var i = 0; i < array.length; ++i)
    array[i] = i;

for (var i = 0; i < 2e4; ++i) {
    if (sum(array) !== 1047552)
        throw new Error("bad sum");
}

function sum(array) {
    var result = 0;
    for (var index of array.keys())
        result += index;
    return result;
}
noInline(sum);

var array = new Uint8Array(1024);

for (var i = 0; i < 2e4; ++i) {
    if (sum(array) !== 523776)
        throw new Error("bad sum");
}

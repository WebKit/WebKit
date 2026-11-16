var patterns = ["fo+", "ba[rz]", "x.*y", "^f$"];
var flags = ["g", "i", "gi", "m"];

function test(pattern, flags) {
    return new RegExp(pattern, flags);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(patterns[i & 3], flags[(i >> 2) & 3]);

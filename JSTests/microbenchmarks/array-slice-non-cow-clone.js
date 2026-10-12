var src8 = [1, 2, 3, 4, 5, 6, 7, 8];
src8[0] = 1; // Force non-CoW.
var src32 = [];
for (var i = 0; i < 32; ++i)
    src32.push(i);

function test(src) {
    return src.slice();
}
noInline(test);

for (var i = 0; i < 4e6; ++i) {
    test(src8);
    test(src32);
}

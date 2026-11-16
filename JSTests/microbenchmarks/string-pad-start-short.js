var strings = ["1", "2", "3", "4", "15", "26", "37", "48"];

function test(string) {
    return string.padStart(2, "0") + string.padStart(8, "0") + string.padEnd(8);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(strings[i & 7]);

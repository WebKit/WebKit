function test(string) {
    return +string;
}
noInline(test);

var strings = ["12.5", "0.25", "-99.75", "1e5", "3.5", "123456.5", "2.5e3", "100.0"];
var result = 0;
for (var i = 0; i < 1e6; ++i)
    result += test(strings[i & 7]);
if (result !== 28246625000)
    throw new Error(`bad result ${result}`);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var string = ``;
string += `var res = 0;\n`;
for (var i = 0; i < 5e4; ++i) {
    string += `res += ${i};\n`
}
string += `return res;`
var f = new Function(string);
shouldBe(f(), 1249975000);

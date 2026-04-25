function test(string) {
    var result;
    for (let i = 0; i < 1e6; i++) {
        result = string.at(-4);
    }
    return result;
}

noInline(test);

var string = "こんにちは、世界";
var result = test(string);
if (result != "は")
    throw "Bad result: " + result;

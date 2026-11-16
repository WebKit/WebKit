function test(string) {
    var result;
    for (let i = 0; i < 1e6; i++) {
        result = string.at(20);
    }
    return result;
}

noInline(test);

var string = "hello, world!";
var result = test(string);
if (result != undefined)
    throw "Bad result: " + result;

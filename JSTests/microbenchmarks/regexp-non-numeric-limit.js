function test(regexp, string) {
    var r = regexp[Symbol.split](string, {
        valueOf() {
            RegExp.prototype.exec = () => null;
            return 100;
        },
    });
    return r.length;
}
noInline(test);

var regexp = /test/g;
var string = "abba";
var result = 0;
var expected = 1;
for (let i = 0; i < testLoopCount; i++) {
    if (test(regexp, string) === expected) {
        result++;
    }
}
if (result !== testLoopCount) {
    throw "Error: bad: " + result;
}

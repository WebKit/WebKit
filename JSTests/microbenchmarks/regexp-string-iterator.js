function test(regexp, string) {
    return String.prototype.matchAll.call(string, regexp);
}
noInline(test);

const regexp = /test/g;
const string = "test test test";

for (let i = 0; i < testLoopCount; i++) {
    test(regexp, string);
}

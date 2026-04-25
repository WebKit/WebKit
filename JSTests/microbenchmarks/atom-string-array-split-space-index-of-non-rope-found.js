
let words = "break break break break break break break break yield super".split(" ");

function test(s) {
    return words.indexOf(s);
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    if (test('yield') != 8)
        throw new Error("bad");
}

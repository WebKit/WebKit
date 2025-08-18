
let words = "break break break break break break break break yield super".split(" ");

function test(s) {
    return words.includes(s);
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    if (test('yieldd'))
        throw new Error("bad");
}

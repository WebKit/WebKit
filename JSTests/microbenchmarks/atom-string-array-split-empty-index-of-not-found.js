
let words = "greedisgood".split('');

function test(s) {
    return words.indexOf(s);
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    if (test('ss') != -1)
        throw new Error("bad");
}

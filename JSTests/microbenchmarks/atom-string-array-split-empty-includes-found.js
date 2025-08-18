
let words = "greedisgood".split('');

function test(s) {
    return words.includes(s);
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    if (!test('s'))
        throw new Error("bad");
}

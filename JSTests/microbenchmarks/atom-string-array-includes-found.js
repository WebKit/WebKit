
let words = ['break', 'yield', 'break', 'yield', 'break', 'yield', 'super'];

function test(s) {
    return words.includes(s);
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    if (!test('super'))
        throw new Error("bad");
}

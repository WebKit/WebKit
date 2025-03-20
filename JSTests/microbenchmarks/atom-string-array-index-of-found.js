
let words = ['break', 'yield', 'break', 'yield', 'break', 'yield', 'super'];

function test(s) {
    return words.indexOf(s);
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    if (test('super') != 6)
        throw new Error("bad");
}

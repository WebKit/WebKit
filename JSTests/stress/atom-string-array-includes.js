
let words = "break yield super".split(" ");

function test(s) {
    return words.includes(s);
}
noInline(test);

let search = 'yield';
for (let i = 0; i < testLoopCount; i++) {
    if (!test(search))
        throw new Error("bad");
    if (i == 1)
        words.push('good');
}

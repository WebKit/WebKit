
let words = "break yield super".split(" ");

function test(s) {
    return words.indexOf(s);
}
noInline(test);

let search = 'yield';
for (let i = 0; i < testLoopCount; i++) {
    if (test(search) != 1)
        throw new Error("bad");
    if (i == 1)
        words.push('good');
}

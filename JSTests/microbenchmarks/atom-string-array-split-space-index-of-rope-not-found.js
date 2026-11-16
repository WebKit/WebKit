
let words = "break break break break yield super".split(" ");

function test(s) {
    return words.indexOf(s);
}
noInline(test);

let search = 'y' + 'i' + 'e' + 'l' + 'd' + 'd';
for (let i = 0; i < 1e5; i++) {
    if (test(search) != -1)
        throw new Error("bad");
}

const strings = [];
for (let i = 0; i < 16; ++i) {
    let s = "";
    for (let j = 0; j < 99; ++j)
        s += String.fromCharCode(32 + ((i + j) % 95));
    strings.push(s);
}

function test(s) {
    return s.split("");
}
noInline(test);

let sum = 0;
for (let i = 0; i < 1e5; ++i) {
    const arr = test(strings[i & 15]);
    sum += arr.length;
}
if (sum !== 1e5 * 99)
    throw new Error("bad sum: " + sum);

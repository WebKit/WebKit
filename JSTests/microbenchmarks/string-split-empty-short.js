const strings = [];
for (let i = 0; i < 64; ++i) {
    let s = "";
    for (let j = 0; j < 16; ++j)
        s += String.fromCharCode(97 + ((i + j) % 26));
    strings.push(s);
}

function test(s) {
    return s.split("");
}
noInline(test);

let sum = 0;
for (let i = 0; i < 5e5; ++i) {
    const arr = test(strings[i & 63]);
    sum += arr.length;
}
if (sum !== 5e5 * 16)
    throw new Error("bad sum: " + sum);

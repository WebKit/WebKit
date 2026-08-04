function splitmix32(seed) {
    var a = seed;
    return function () {
        a |= 0; a = (a + 0x9e3779b9) | 0;
        var t = a ^ (a >>> 16); t = Math.imul(t, 0x21f0aaad);
        t = t ^ (t >>> 15); t = Math.imul(t, 0x735a2d97);
        return ((t = t ^ (t >>> 15)) >>> 0) / 4294967296;
    };
}

function makeString(length, seed) {
    var rnd = splitmix32(seed);
    var out = [];
    for (var i = 0; i < length; ++i)
        out.push("a", String.fromCharCode(0xd800 + (rnd() * 0x400 | 0)));
    out.push("X");
    return out.join("");
}

var subjects = [];
for (var k = 0; k < 8; ++k)
    subjects.push(makeString(250, 0x3000 + k));

var re = /[^X]*X/u;
var count = 0;

for (var i = 0; i < 1e5; ++i) {
    if (re.test(subjects[i & 7]))
        count++;
}

if (count !== 1e5)
    throw new Error("bad");

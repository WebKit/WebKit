function splitmix32(seed) {
    var a = seed;
    return function () {
        a |= 0; a = (a + 0x9e3779b9) | 0;
        var t = a ^ (a >>> 16); t = Math.imul(t, 0x21f0aaad);
        t = t ^ (t >>> 15); t = Math.imul(t, 0x735a2d97);
        return ((t = t ^ (t >>> 15)) >>> 0) / 4294967296;
    };
}

function makeString(codePoints, length, seed) {
    var rnd = splitmix32(seed);
    var out = [];
    for (var i = 0; i < length; ++i)
        out.push(String.fromCharCode(codePoints[rnd() * codePoints.length | 0]));
    out.push("X");
    return out.join("");
}

var codePoints = [0x8a3a, 0xac00, 0xd7a3, 0xff21, 0xff9f, 0x9f8d];
var subjects = [];
for (var k = 0; k < 8; ++k)
    subjects.push(makeString(codePoints, 500, 0x1000 + k));

var re = /[^X]*X/u;
var count = 0;

for (var i = 0; i < 1e5; ++i) {
    if (re.test(subjects[i & 7]))
        count++;
}

if (count !== 1e5)
    throw new Error("bad");

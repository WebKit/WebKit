var as = [];
for (var a = 0; a < 8; a++) {
    var h = [];
    for (var j = 0; j < 8; j++)
        h.push(a * 100 + j);
    as.push(h);
}
var b = [];
for (var j = 0; j < 16; j++)
    b.push(j);
var c = [];
for (var j = 0; j < 4; j++)
    c.push(j);

var result = 0;
for (var i = 0; i < 2e6; i++) {
    var r = [].concat(as[i & 7], b, c);
    result += r.length;
}

if (result !== 56e6)
    throw new Error("bad result: " + result);

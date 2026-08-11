var heads = [];
for (var a = 0; a < 8; a++) {
    var h = [];
    for (var j = 0; j < 8; j++)
        h.push('h' + a + '_' + j);
    heads.push(h);
}
var mid = [];
for (var j = 0; j < 8; j++)
    mid.push('m' + j);

var result = 0;
for (var i = 0; i < 2e6; i++) {
    var r = [].concat(heads[i & 7], mid);
    result += r.length;
}

if (result !== 32e6)
    throw new Error("bad result: " + result);

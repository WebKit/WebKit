var a = [1, 2, 3, 4, 5, 6, 7, 8];
var b = [10, 20, 30, 40];

var result = 0;
for (var i = 0; i < 2e6; i++) {
    var r = a.concat(i & 7, 9);
    result += r.length;
    r = a.concat(b, i & 7);
    result += r.length;
}

if (result !== 46e6)
    throw new Error("bad result: " + result);

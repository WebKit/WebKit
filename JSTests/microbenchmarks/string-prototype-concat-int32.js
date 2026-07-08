function build(x, y) {
    return "translate3d(".concat(x, "px, ").concat(y, "px, 0)");
}
noInline(build);

var xs = [];
var ys = [];
for (var i = 0; i < 16; ++i) {
    xs.push(i * 3);
    ys.push(i * 7);
}

var len = 0;
for (var i = 0; i < 3000000; ++i)
    len += build(xs[i & 15], ys[i & 15]).length;

if (len != 77062500)
    throw "Error: bad result: " + len;

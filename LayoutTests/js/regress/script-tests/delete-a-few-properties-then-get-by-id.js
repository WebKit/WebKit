function MyObject(x, y) {
    this.x = x;
    this.y = y;
    this.deleteMe = "delete me";
}

function foo(o) {
    return o.x + o.y;
}

var niters = 100000;
var sum = 0;
var o = new MyObject(13, 42);
delete o.deleteMe;

for (var i = 0; i < niters; ++i) {
    sum += foo(o);
}

if (sum != 55 * niters)
    throw new Error("Bad result!");

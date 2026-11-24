function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected " + expected);
}

function Point(x, y) { this.x = x; this.y = y; }

function chain(p, q, k) {
    var a = p.x;
    var b = a * q.y;
    var c = b - p.y;
    var d = c + k;
    var e = d;
    var f = -e;
    return f * q.x;
}
noInline(chain);

function withJumps(p, n) {
    var s = 0;
    for (var i = 0; i < n; ++i) {
        var v = p.x;
        if (i & 1)
            v = v * 2;
        s = s + v;
    }
    return s;
}
noInline(withJumps);

function square(p) {
    var a = p.x;
    return a * a;
}
noInline(square);

var p = new Point(1.5, 2.5);
var q = new Point(3, 4);
var stringPoint = new Point("2", 2.5);
var weird = { x: "2", y: { valueOf() { return 5; } } };
var getterPoint = { get x() { return 7; }, y: 1 };

for (var i = 0; i < testLoopCount * 2; ++i) {
    shouldBe(chain(p, q, 1), -((1.5 * 4 - 2.5 + 1) * 3));
    shouldBe(withJumps(p, 4), 1.5 + 3 + 1.5 + 3);
    shouldBe(square(p), 1.5 * 1.5);
    if (!(i % 97)) {
        shouldBe(chain(stringPoint, q, 1), -((2 * 4 - 2.5 + 1) * 3));
        shouldBe(square(stringPoint), 4);
        shouldBe(chain(weird, q, 1), -((2 * 4 - 5 + 1) * 3));
        shouldBe(chain(getterPoint, p, 0), -((7 * 2.5 - 1) * 1.5));
        shouldBe(chain(p, { x: 2147483647, y: 2147483647 }, 0), -((1.5 * 2147483647 - 2.5) * 2147483647));
        shouldBe(withJumps(getterPoint, 3), 7 + 14 + 7);
    }
}

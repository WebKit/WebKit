function make(n) { var o = {}; for (var i = 0; i < n; ++i) o["p" + i] = i; return o; }
function C() { this.a = 1; this.b = 2; }
C.prototype.protoValue = 42;
function getA(o) { return o.a; }
function getP3(o) { return o.p3; }
function getP70(o) { return o.p70; }
function getProto(o) { return o.protoValue; }
function getProtoP9(o) { return o.p9; }
function setA(o, v) { o.a = v; }
function setP70(o, v) { o.p70 = v; }
noInline(getA); noInline(getP3); noInline(getP70); noInline(getProto); noInline(getProtoP9); noInline(setA); noInline(setP70);
function check(a, b) { if (a !== b) throw new Error("bad " + a + " " + b); }
var small = new C();
var big = make(100);
var mid = make(10);
var viaProto = Object.create(make(10));
for (var i = 0; i < testLoopCount / 2; ++i) {
    check(getA(small), i ? i - 1 : 1);
    setA(small, i);
    check(getA(small), i);
    check(getP3(mid), 3);
    check(getP70(big), i ? -(i - 1) : 70);
    setP70(big, -i);
    check(getP70(big), -i);
    check(getProto(small), 42);
    check(getProtoP9(viaProto), 9);
}

description(
"This tests that op_get_by_pname is compiled correctly for non-final objects."
);

function foo(o) {
    var result = 0;
    for (var n in o)
        result += o[n];
    return result;
}

var o = new Date();
var p = new Date();
var q = new Date();
var r = new Date();
var s = new Date();
o.a = 1;
o.b = 3;
o.c = 7;
p.a = 1;
p.b = 2;
p.c = 3;
p.d = 4;
q.a = 1;
q.b = 2;
q.c = 3;
q.d = 4;
q.e = 3457;
r.a = 1;
r.b = 2;
r.c = 3;
r.d = 4;
r.e = 91;
r.f = 12;
s.a = 1;
s.b = 2;
s.c = 3;
s.d = 4;
s.e = 91;
s.f = 12;
s.g = 69;

for (var i = 0; i < 100; ++i) {
    shouldBe("foo(o)", "11");
    shouldBe("foo(p)", "10");
    shouldBe("foo(q)", "3467");
    shouldBe("foo(r)", "113");
    shouldBe("foo(s)", "182");
}


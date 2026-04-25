function test(o, proto)
{
    var s = o.a + o.b + o.c + o.d + o.e + o.f;
    Object.create(proto);
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    s += o.a + o.b + o.c + o.d + o.e + o.f;
    return s;
}
noInline(test);

var o = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 };
var result = 0;
for (var i = 0; i < 2e6; ++i)
    result += test(o, null);
if (result !== 168 * 2e6)
    throw new Error("bad result: " + result);

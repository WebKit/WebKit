function test(o)
{
    return ("a" in o) + ("b" in o) + ("c" in o) + ("d" in o);
}
noInline(test);

var o = { a: 1, b: 2, c: 3, d: 4 };
var result = 0;
for (var i = 0; i < 1e4; ++i)
    result += test(o);

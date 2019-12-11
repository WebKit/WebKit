function foo()
{
    var a = [1, 2];
    var l = [...a, 42, ...a].length;
    if (l != 5)
        throw "Wrong length in foo: " + l;
}
noInline(foo);

function bar(...b)
{
    var l = [...b, 43, ...b].length;
    if (l != 7)
        throw "Wrong length in bar: " + l
}
noInline(bar);

function baz(arg0, ...c)
{
    var x = [...c, ...c];
    var l = [...x, ...x, ...x].length;
    if (l != 24)
        throw "Wrong length in baz: " + l
}
noInline(baz);

for (var i = 0; i < 10000; ++i) {
    foo();
    bar(1, 2, 3);
    baz(0, 1, 2, 3, 4);
}

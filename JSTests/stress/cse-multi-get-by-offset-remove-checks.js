function Cons1()
{
    this.e = 1;
    this.f = 2;
}

Cons1.prototype.g = 1;

function Cons2()
{
    this.f = 1;
    this.h = 2;
}

Cons2.prototype.g = 2;

function Cons3()
{
    this.d = 1;
    this.e = 2;
    this.f = 3;
}

Cons3.prototype = Cons2.prototype;

function foo(o, p, q)
{
    var x = 0, y = 0;
    if (p)
        x = o.f;
    if (q)
        y = o.f;
    return x + y;
}

for (var i = 0; i < 10000; ++i) {
    foo(new Cons1(), true, false);
    foo(new Cons2(), false, true);
    foo(new Cons3(), false, true);
}

function bar(o, p)
{
    return foo(o, true, p);
}

noInline(bar);

for (var i = 0; i < 100000; ++i)
    bar(new Cons1(), false);

var result = bar(new Cons1(), true);
if (result != 4)
    throw "Error: bad result: " + result;


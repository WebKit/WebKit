function foo(o, p) {
    possiblyDoBadThings(p);
    return o.f();
}
noInline(foo);

function Thingy() { }
Thingy.prototype.f = function() { return 42; }

function possiblyDoBadThings(p) {
    if (p)
        Thingy.prototype.f = function() { return 24; }
}
noInline(possiblyDoBadThings);

for (var i = 0; i < 100000; ++i) {
    var result = foo(new Thingy(), false);
    if (result != 42)
        throw "Error: bad result: " + result;
}

var result = foo(new Thingy(), true);
if (result != 24)
    throw "Error: bad result: " + result;

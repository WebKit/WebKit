function foo(x) {
    var tmp = x + 1;
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo(i);

var didCall = false;
var o = {
    valueOf: function() { didCall = true; }
};

foo(o);
if (!didCall)
    throw "Error: didn't call valueOf";

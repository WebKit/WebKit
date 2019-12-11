function foo() {
    var f = function() { return 42 };
    f.prototype.f = function() { return 43; };
    return f.prototype.f;
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo();


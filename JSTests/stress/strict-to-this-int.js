function foo(a, b) {
    var result = a + b;
    if (result)
        return (a + b) + result + this;
    else
        return this.f;
}

noInline(foo);

var x;
Number.prototype.valueOf = function() { return x; };

function test(this_, a, b, x_) {
    x = x_;
    var result = foo.call(this_, a, b);
    if (result != (a + b) * 2 + x_)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 100000; ++i)
    test(5, 1, 2, 100);

test(5, 2000000000, 2000000000, 1);
test(5, 1073741774, 1073741774, 1000);

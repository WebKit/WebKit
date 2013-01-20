function A() { }

A.prototype = {f:42};

function B() { }

B.prototype = new A();

function C() { }

C.prototype = new B();

function D() { }

D.prototype = new C();

function E() { }

E.prototype = new D();

function F() { }

F.prototype = new E();

function G() { }

G.prototype = new F();

function foo(o) {
    try {
        var result = 0;
        for (var i = 0; i < 1000; ++i)
            result += o.f;
        return result;
    } catch (e) {
        return 52;
    }
}

var result = 0;

for (var i = 0; i < 1000; ++i)
    result += foo(new G());

if (result != 42000000)
    throw "Error: bad result: " + result;


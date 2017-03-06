class A {
    get f() {
        return this._f;
    }
}

class B extends A {
    get f() {
        return super.f;
    }
}

(function() {
    var o = new B();

    o._f = 42;
    var result = 0;
    var n = 2000000;
    for (var i = 0; i < n; ++i)
        result += o.f;
    if (result != n * 42)
        throw "Error: bad result: " + result;
})();


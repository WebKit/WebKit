function Foo(f, g)
{
    this.f = f;
    this.g = g;
    Object.seal(this);
}

(function() {
    var result = 0;
    for (var i = 0; i < 10000; ++i) {
        var o = new Foo(i, i + 1);
        for (var j = 0; j < 1000; ++j)
            result += o.f * o.g;
    }
    
    if (result != 333333330000000)
        throw "Error: bad result: " + result;
})();

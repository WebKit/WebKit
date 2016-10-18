function Foo()
{
    this.f = 42;
}

function bar()
{
    return new Foo();
}

noInline(Foo);
noInline(bar);

(function() {
    for (var i = 0; i < 10000000; ++i) {
        var result = bar();
        if (result.f != 42)
            throw "Error: bad result: " + result;
    }
})();


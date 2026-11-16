var count = 0;

function setter(value) {
    Object.defineProperty(
        this, "f", {
        enumerable: true,
                configurable: true,
                writable: true,
                value: 32
                });
    var o = Object.create(this);
    var currentCount = count++;
    var str = "for (var i = " + currentCount + "; i < " + (100 + currentCount) + "; ++i)\n"
            + "    o.f\n";
    eval(str);
}

function foo(o) {
    o.f = 42;
}

noInline(foo);

for (var i = 0; i < 1000; ++i) {
    var o = {};
    o.__defineSetter__("f", setter);

    foo(o);
    if (o.f != 32)
        throw ("Error 1: "+o.f);

    foo(o);
    if (o.f != 42)
        throw ("Error 2: "+o.f);
}
var count = 0;

function setter(value) {
    Object.defineProperty(
        this, "f", {
        enumerable: true,
                configurable: true,
                writable: true,
                value: 42
                });
    var o = Object.create(this);
    var currentCount = count++;
   var str =
       "for (var i = " + currentCount + "; i < " + (100 + currentCount) + "; ++i)\n" +
       "    o.f\n";
   eval(str);
}

function foo(o) {
    o.f = 42;
}

noInline(foo);

for (var i = 0; i < 1000; ++i) {
    print("i = " + i);
    var o = {};
    o.__defineSetter__("f", setter);
    foo(o);
    foo(o);
}

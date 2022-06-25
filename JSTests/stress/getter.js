function foo(o) {
    return o.f + o.k * 1000;
}

noInline(foo);

for (var i = 0; i < 100; ++i) {
    var o = {g_: 5};
    o.__defineGetter__("f", function() { return 42 + this.g_; });
    o.__defineGetter__("g", function() { return 43 + this.g_; });
    o.__defineGetter__("h", function() { return 44 + this.g_; });
    o.__defineGetter__("i", function() { return 45 + this.g_; });
    o.__defineGetter__("j", function() { return 46 + this.g_; });
    o.__defineGetter__("k", function() { return 47 + this.g_; });
    var result = foo(o);
    if (result != (42 + 5) + 1000 * (47 + 5))
        throw "Error: bad result: " + result;
}

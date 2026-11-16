description(
"Tests that DFG getter caching does not break the world."
);

function foo(o) {
    return o.f;
}

for (var i = 0; i < 200; ++i) {
    var o = new Object();
    o.__defineGetter__("f", function(){ return i; });
    shouldBe("foo(o)", "" + i);
}



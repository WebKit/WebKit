description(
"Tests that DFG getter caching does not break the world if the getter throws an exception."
);

function foo(o) {
    return o.f;
}

function bar(o) {
    try {
        return "Returned result: " + foo(o);
    } catch (e) {
        return "Threw exception: " + e;
    }
}

for (var i = 0; i < 200; ++i) {
    var o = new Object();
    o.__defineGetter__("f", function(){
        if (i < 100)
            return i;
        else
            throw "Oh hi, I'm an exception!";
    });
    shouldBe("bar(o)", i < 100 ? "\"Returned result: " + i + "\"" : "\"Threw exception: Oh hi, I'm an exception!\"");
}



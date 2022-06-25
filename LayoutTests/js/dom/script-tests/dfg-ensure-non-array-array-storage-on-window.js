description(
"Tests that passing the global object to an array access that will arrayify to NonArrayWithArrayStorage doesn't break things."
);

function foo(array) {
    var result = 0;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}

function bar(array) {
    array[1] = 42;
}

noInline(foo);
noInline(bar);
silentTestPass = true;

var array = {};
array.length = 3;
array[0] = 1;
array[1] = 2;
array[2] = 3;
for (var i = 0; i < 200; i = dfgIncrement({f:[foo, bar], i:i + 1, n:100})) {
    shouldBe("foo(array)", "6");
    
    var otherArray = {};
    bar(otherArray);
    shouldBe("otherArray[1]", "42");
}

for (var i = 0; i < 1000; i = dfgIncrement({f:[foo, bar], i:i + 1, n:500, compiles:2})) {
    // Do strange things to ensure that the get_by_id on length goes polymorphic.
    var array = {};
    if (i % 2)
        array.x = 42;
    array.length = 3;
    array[0] = 1;
    array[2] = 3;
    array.__defineGetter__(1, function() { return 6; });

    shouldBe("foo(array)", "10");
    
    var otherArray = {};
    otherArray.__defineSetter__(0, function(value) { throw "error"; });
    bar(otherArray);
    shouldBe("otherArray[1]", "42");
}

var w = this;
w[0] = 1;
w.length = 1;
shouldThrowErrorName("w.__defineSetter__(1, function() {})", "TypeError");
shouldBe("foo(w)", "NaN");

// At this point we check to make sure that bar doesn't end up either creating array storage for
// the window proxy, or equally badly, storing to the already created array storage on the proxy
// (since foo() may have made the mistake of creating array storage). That's why we do the setter
// thingy, to detect that for index 1 we fall through the proxy to the real window object.
bar(w);

w.length = 2;
shouldBe("w[1]", "");
shouldBe("foo(w)", "NaN");


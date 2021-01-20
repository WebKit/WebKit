description(
"Tests that defining a setter on the Array prototype works."
);

var ouches = 0;
Array.prototype.__defineSetter__("3", function() { debug("Ouch!"); ouches++; });

function foo() {
    var result = [];
    result.length = 5;
    for (var i = 0; i < result.length; ++i)
        result[i] = i;
    return result;
}

for (var i = 0; i < 100; ++i)
    shouldBe("\"" + foo().join(",") + "\"", "\"0,1,2,,4\"");

shouldBe("ouches", "100");

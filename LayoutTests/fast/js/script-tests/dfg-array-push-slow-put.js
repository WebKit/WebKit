description(
"Tests that defining a setter on the Array prototype works with ArrayPush."
);

var ouches = 0;
Array.prototype.__defineSetter__("3", function() { debug("Ouch!"); ouches++; });

function foo() {
    var result = [];
    for (var i = 0; i < 5; ++i)
        result.push(i);
    return result;
}

for (var i = 0; i < 100; ++i)
    shouldBe("\"" + foo().join(",") + "\"", "\"0,1,2,,4\"");

shouldBe("ouches", "100");

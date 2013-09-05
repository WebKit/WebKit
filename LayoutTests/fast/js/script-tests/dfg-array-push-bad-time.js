description(
"Tests that defining a setter on the Array prototype and then using ArrayPush works even if it is done after arrays are allocated."
);

var ouches = 0;

function foo(haveABadTime) {
    var result = [];
    for (var i = 0; i < 5; ++i) {
        if (i == haveABadTime) {
            debug("Henceforth I will have a bad time.");
            Array.prototype.__defineSetter__("3", function() { debug("Ouch!"); ouches++; });
        }
        result.push(i);
    }
    return result;
}

var expected = "\"0,1,2,3,4\"";

silentTestPass = true;
noInline(foo);

for (var i = 0; i < 1000; i = dfgIncrement({f:foo, i:i + 1, n:900})) {
    var haveABadTime;
    if (i == 950) {
        haveABadTime = 2;
        expected = "\"0,1,2,,4\"";
    } else
        haveABadTime = -1;
    shouldBe("\"" + foo(haveABadTime).join(",") + "\"", expected);
}

shouldBe("ouches", "50");

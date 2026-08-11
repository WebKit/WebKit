description(
"Tests that defining a setter on the prototype of an object used for indexed storage works even if it is done after objects with indexed storage are allocated."
);

function Cons() {
}

var ouches = 0;

function foo(haveABadTime) {
    var result = new Cons();
    result.length = 5;
    for (var i = 0; i < result.length; ++i) {
        if (i == haveABadTime) {
            debug("Henceforth I will have a bad time.");
            Cons.prototype.__defineSetter__("3", function() { debug("Ouch!"); ouches++; });
        }
        result[i] = i;
    }
    return result;
}

var expected = "\"0,1,2,3,4\"";

for (var i = 0; i < 1000; ++i) {
    var haveABadTime;
    if (i == 950) {
        haveABadTime = 2;
        expected = "\"0,1,2,,4\"";
    } else
        haveABadTime = -1;
    shouldBe("\"" + Array.prototype.join.apply(foo(haveABadTime), [","]) + "\"", expected);
}

shouldBe("ouches", "50");

description(
"Tests that defining a setter on the Array prototype works with ArrayPush."
);

var ouches = 0;
Array.prototype.__defineSetter__("3", function() { ouches++; });

function foo() {
    var result = [];
    for (var i = 0; i < 5; ++i)
        result.push(i);
    return result;
}

var numRuns = dfgShouldBe(foo, "foo().join(\",\")", "\"0,1,2,,4\"");

shouldBe("ouches", "numRuns");

description(
"Test to make sure the eval code cache doesn't crash or give wrong results in odd situations."
);


var str = "(function () { return a; })";
var a = "first";
var first = eval(str)();
shouldBe("first", "'first'");

with ({a : "second"}) {
    var second = eval(str)();
}

shouldBe("second", "'second'");

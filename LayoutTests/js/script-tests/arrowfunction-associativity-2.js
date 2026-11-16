description('Tests for ES6 arrow function nested declaration');

debug("af = a => b => a")
var af = a => b => a;

shouldBe("af('ABC')('DEF')", "'ABC'");

var successfullyParsed = true;

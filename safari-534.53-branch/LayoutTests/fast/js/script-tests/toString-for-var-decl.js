description(
"This test checks for a couple of specific ways that bugs in toString() round trips have changed the meanings of functions with var declarations inside for loops."
);

function f1() { for (var j = 1 in []) {}  }
var f2 = function () { for (var j = 1; j < 10; ++j) {}  }
var f3 = function () { for (j = 1;j < 10; ++j) {}  }
var f4 = function () { for (var j;;) {}  }

var unevalf = function(x) { return '(' + x.toString() + ')'; }

shouldBe("unevalf(eval(unevalf(f1)))", "unevalf(f1)");
shouldBe("unevalf(eval(unevalf(f2)))", "unevalf(f2)");
shouldBe("unevalf(eval(unevalf(f3)))", "unevalf(f3)");
shouldBe("unevalf(eval(unevalf(f4)))", "unevalf(f4)");
shouldBe("unevalf(f2) != unevalf(f3)", "true");

var successfullyParsed = true;

//@ skip if $memoryLimited and $hostOS == "darwin"

description("Tests that we throw an error when passing a number of arguments beyond a certain threshold.");

function f() { return arguments.length; }
function fPrint() { debug(arguments.length); }
function g() { return f.apply(null, arguments); }
function h() { arguments; return f.apply(null, arguments); }
function i() { arguments[arguments.length] = 0; return f.apply(null, arguments); }
var bigArray = [];
for (var j = 0; j < 0x100001; j++)
    bigArray.push(null);

var args = "0,".repeat(0x100000) + '0';

shouldBe("f.apply(null)", "0");
shouldBe("f.apply(null, [])", "0");
shouldBe("f.apply(null, [1])", "1");
shouldBe("f.apply(null, new Array(10))", "10");
shouldBe("f.apply(null, new Array(1000))", "1000");
shouldBe("f.apply(null, new Array(65536))", "65536");
shouldThrow("f.apply(null, new Array(0x100001))");
shouldThrow("f.apply(null, new Array(0x100001))");
shouldThrow("f.apply(null, bigArray)");
shouldBe("g.apply(null)", "0");
shouldBe("g.apply(null, [])", "0");
shouldBe("g.apply(null, [1])", "1");
shouldBe("g.apply(null, new Array(10))", "10");
shouldBe("g.apply(null, new Array(1000))", "1000");
shouldThrow("g.apply(null, new Array(0x100001))");
shouldThrow("g.apply(null, new Array(0x100001))");
shouldThrow("g.apply(null, bigArray)");
shouldThrow("g(" + args + ")");
shouldBe("h.apply(null)", "0");
shouldBe("h.apply(null, [])", "0");
shouldBe("h.apply(null, [1])", "1");
shouldBe("h.apply(null, new Array(10))", "10");
shouldBe("h.apply(null, new Array(1000))", "1000");
shouldThrow("h.apply(null, new Array(0x100001))");
shouldThrow("h.apply(null, new Array(0x100001))");
shouldThrow("h.apply(null, bigArray)");
shouldBe("i.apply(null)", "0");
shouldBe("i.apply(null, [])", "0");
shouldBe("i.apply(null, [1])", "1");
shouldBe("i.apply(null, new Array(10))", "10");
shouldBe("i.apply(null, new Array(1000))", "1000");
shouldThrow("i.apply(null, new Array(0x100001))");
shouldThrow("i.apply(null, new Array(0x100001))");
shouldThrow("i.apply(null, bigArray)");

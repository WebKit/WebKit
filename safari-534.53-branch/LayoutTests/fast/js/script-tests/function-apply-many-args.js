description("Tests that we truncate arguments beyond a certain threshold.");

function f() { return arguments.length; }
function g() { return f.apply(null, arguments); }
function h() { arguments; return f.apply(null, arguments); }
function i() { arguments[arguments.length] = 0; return f.apply(null, arguments); }
var bigArray = [];
for (var j = 0; j < 100000; j++)
    bigArray.push(null);

shouldBe("f.apply(null)", "0");
shouldBe("f.apply(null, [])", "0");
shouldBe("f.apply(null, [1])", "1");
shouldBe("f.apply(null, new Array(10))", "10");
shouldBe("f.apply(null, new Array(1000))", "1000");
shouldBe("f.apply(null, new Array(65536))", "65536");
shouldBe("f.apply(null, new Array(65537))", "65536");
shouldBe("f.apply(null, new Array(65537))", "65536");
shouldBe("f.apply(null, bigArray)", "65536");
shouldBe("g.apply(null)", "0");
shouldBe("g.apply(null, [])", "0");
shouldBe("g.apply(null, [1])", "1");
shouldBe("g.apply(null, new Array(10))", "10");
shouldBe("g.apply(null, new Array(1000))", "1000");
shouldBe("g.apply(null, new Array(65536))", "65536");
shouldBe("g.apply(null, new Array(65537))", "65536");
shouldBe("g.apply(null, new Array(65537))", "65536");
shouldBe("g.apply(null, bigArray)", "65536");
shouldBe("h.apply(null)", "0");
shouldBe("h.apply(null, [])", "0");
shouldBe("h.apply(null, [1])", "1");
shouldBe("h.apply(null, new Array(10))", "10");
shouldBe("h.apply(null, new Array(1000))", "1000");
shouldBe("h.apply(null, new Array(65536))", "65536");
shouldBe("h.apply(null, new Array(65537))", "65536");
shouldBe("h.apply(null, new Array(65537))", "65536");
shouldBe("h.apply(null, bigArray)", "65536");
shouldBe("i.apply(null)", "0");
shouldBe("i.apply(null, [])", "0");
shouldBe("i.apply(null, [1])", "1");
shouldBe("i.apply(null, new Array(10))", "10");
shouldBe("i.apply(null, new Array(1000))", "1000");
shouldBe("i.apply(null, new Array(65536))", "65536");
shouldBe("i.apply(null, new Array(65537))", "65536");
shouldBe("i.apply(null, new Array(65537))", "65536");
shouldBe("i.apply(null, bigArray)", "65536");

var successfullyParsed = true;

description("This test makes sure we don't hang when setting cyclic prototype values: http://bugs.webkit.org/show_bug.cgi?id=6985")

var o1 = { p1: 1 };
var o2 = { p2: 2 };
o2.__proto__ = o1;
var o3 = { p3: 3 };
o3.__proto__ = o2;

o1.__proto__ = null;  // just for sanity's sake

shouldThrow("o1.__proto__ = o3");

var successfullyParsed = true;

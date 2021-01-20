description(
"This test checks whether various forms of typeof expression are allowed."
);

var x = 0;
var y = 0;

shouldBe('typeof x', "'number'");
shouldBe('typeof window.x', "'number'");
shouldBe('typeof window["x"]', "'number'");
shouldBe('typeof (x)', "'number'");
shouldBe('typeof (window.x)', "'number'");
shouldBe('typeof (window["x"])', "'number'");
shouldBe('(y, typeof x)', "'number'");
shouldBe('typeof ((x))', "'number'");
shouldBe('typeof ((window.x))', "'number'");
shouldBe('typeof ((window["x"]))', "'number'");
shouldBe('typeof (y, x)', "'number'");
shouldBe('typeof (true ? x : y)', "'number'");

shouldBe('typeof nonexistent', "'undefined'");
shouldBe('typeof window.nonexistent', "'undefined'");
shouldBe('typeof window["nonexistent"]', "'undefined'");
shouldBe('typeof (nonexistent)', "'undefined'");
shouldBe('typeof (window.nonexistent)', "'undefined'");
shouldBe('typeof (window["nonexistent"])', "'undefined'");

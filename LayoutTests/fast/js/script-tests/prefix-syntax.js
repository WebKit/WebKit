description(
"This test checks whether various forms of preincrement expression are allowed."
);

var x = 0;
var y = 0;

shouldBe('++x', '1');
shouldBe('++window.x', '2');
shouldBe('++window["x"]', '3');
shouldBe('++(x)', '4');
shouldBe('++(window.x)', '5');
shouldBe('++(window["x"])', '6');
shouldBe('(y, ++x)', '7');
shouldBe('++((x))', '8');
shouldBe('++((window.x))', '9');
shouldBe('++((window["x"]))', '10');

shouldThrow('++(y, x)');
shouldThrow('++(true ? x : y)');
shouldThrow('++++x');

var successfullyParsed = true;
description(
"This test checks whether various forms of postincrement expression are allowed."
);

var x = 0;
var y = 0;

shouldBe('x++', '0');
shouldBe('window.x++', '1');
shouldBe('window["x"]++', '2');
shouldBe('(x)++', '3');
shouldBe('(window.x)++', '4');
shouldBe('(window["x"])++', '5');
shouldBe('(y, x++)', '6');
shouldBe('((x))++', '7');
shouldBe('((window.x))++', '8');
shouldBe('((window["x"]))++', '9');

shouldThrow('(y, x)++');
shouldThrow('(true ? x : y)++');
shouldThrow('x++++');

x = 0;
x = x++;
shouldBe("x", "0");

y = 0;
y = y--;
shouldBe("y", "0");

var successfullyParsed = true;


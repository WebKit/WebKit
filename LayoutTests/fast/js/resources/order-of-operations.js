var leftRight;
function left() {
    leftRight = leftRight + "Left";
}

function right() {
    leftRight = leftRight + "Right";
}

shouldBe('(function(){ leftRight = ""; left() > right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() >= right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() < right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() <= right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() + right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() - right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() / right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() * right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() % right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() << right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() >> right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() >>> right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() || right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() && right(); return leftRight; })()', '"Left"');
shouldBe('(function(){ leftRight = ""; left() & right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() | right(); return leftRight; })()', '"LeftRight"');
shouldBe('(function(){ leftRight = ""; left() ^ right(); return leftRight; })()', '"LeftRight"');

var successfullyParsed = true;

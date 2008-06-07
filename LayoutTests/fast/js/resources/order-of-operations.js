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

function testEvaluationOfArguments()
{
    function throwPass()
    {
        throw "PASS";
    }
    
    var nonFunction = 42;
    
    try {
        nonFunction(throwPass());
    } catch (e) {
        return e == "PASS";
    }
}

shouldBeTrue("testEvaluationOfArguments()");

var successfullyParsed = true;

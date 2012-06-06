description(
"This test case checks variable resolution in the presence of both eval and with."
);

// Direct non-strict eval inside a with.

function freeVarInsideEvalAndWith(o, str)
{
    with (o)
    {
        return function () { return eval(str); }
    }
}

shouldBeTrue('freeVarInsideEvalAndWith({}, "true")()')
shouldBeFalse('freeVarInsideEvalAndWith({}, "false")()')
shouldBeTrue('freeVarInsideEvalAndWith({}, "var x = 10; x")() == 10')
shouldBeTrue('freeVarInsideEvalAndWith({}, "var x = 10; (function (){return x;})")()() == 10')

function localVarInsideEvalAndWith(o, str)
{
    with (o)
    {
        return eval(str);
    }
}

shouldBeTrue('localVarInsideEvalAndWith({}, "true")')
shouldBeFalse('localVarInsideEvalAndWith({}, "false")')
shouldBeTrue('localVarInsideEvalAndWith({}, "var x = true; x")')
shouldBeTrue('localVarInsideEvalAndWith({}, "var x = 10; (function (){return x;})")() == 10')

var y;
shouldBeTrue('localVarInsideEvalAndWith(y={x:false}, "var x = true; x && y.x")')

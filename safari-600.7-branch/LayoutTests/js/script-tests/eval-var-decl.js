description(
"This test case checks whether variables cause properties to be defined even before reaching the declaration statement in various cases."
);

shouldBeTrue('this.hasOwnProperty("foo")');
var foo = 3;

delete bar;
shouldBeTrue('this.hasOwnProperty("bar")');
var bar = 3;

var firstEvalResult = eval('var result = this.hasOwnProperty("y"); var y = 3; result');
shouldBeTrue("firstEvalResult");

var secondEvalResult = eval('delete x; var result = this.hasOwnProperty("x"); var x = 3; result');
shouldBeFalse("secondEvalResult");

var thirdEvalResult = false;
try {
    thirdEvalResult = (function(){ var x=false; try { throw ""; } catch (e) { eval("var x = true;"); } return x; })();
} catch (e) {
    thirdEvalResult = "Threw exception!";
}
shouldBeTrue("thirdEvalResult");

// Check that the correct this value is passed to a function called having been caught from a throw, where the catch block contains an eval (bug#).
function checkThis()
{
    "use strict";
    return this === undefined;
}
function testEvalInCatch()
{
    try {
        throw checkThis;
    } catch(e) {
        eval('');
        return e();
    }
    return false;
}
shouldBeTrue("testEvalInCatch()");

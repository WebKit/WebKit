description('This test makes sure stack unwinding works correctly when confronted with a 0-depth scope chain without an activation');
var result;
function runTest() {
    var test = "outer scope";
    with({test:"inner scope"})
       (function () { try { throw ""; } finally { result = test; shouldBe("result", '"inner scope"'); return;}})()
}
runTest();

var successfullyParsed = true;

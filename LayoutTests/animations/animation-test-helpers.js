if (window.layoutTestController) {
  layoutTestController.dumpAsText();
  layoutTestController.waitUntilDone();
}

result = "";

function isCloseEnough(actual, desired, tolerance)
{
    if (tolerance == undefined || tolerance == 0)
        tolerance = defaultTolerance;
    var diff = Math.abs(actual - desired);
    return diff < tolerance;
}

function checkExpectedValue(index)
{
    var property = expected[index][1];
    var id = expected[index][2];
    var expectedValue = expected[index][3];
  
    var computedStyle = window.getComputedStyle(document.getElementById(id)).getPropertyCSSValue(property);

    var computedValue = computedStyle.getFloatValue(CSSPrimitiveValue.CSS_NUMBER);
    if (isCloseEnough(computedValue, expectedValue))
        result += "PASS - " + id + " at " + expected[index][0] + " saw something close to: " + expectedValue + "<br>";
    else
        result += "FAIL - " + id + " at " + expected[index][0] + " expected: " + expectedValue + " but saw: " + computedValue + "<br>";
}

function checkFunctionWithParameter(i)
{
  return function() {
    checkExpectedValue(i);
  };
}

function setup()
{
    for (var i=0; i < expected.length; i++) {
        window.setTimeout(checkFunctionWithParameter(i), expected[i][0]);
    }
}

function cleanup()
{
    document.getElementById('result').innerHTML = result;
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}
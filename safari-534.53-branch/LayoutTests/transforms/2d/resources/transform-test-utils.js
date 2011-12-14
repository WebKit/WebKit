
// compare matrix() strings with some slop on really small values

function floatingPointEqual(a, b)
{
  const kEpsilon = 1e-4;
  return (Math.abs(a - b) < kEpsilon);
}

function compareMatrices(a, b)
{
  if (a == "none" && b == "none")
    return true;

  var matrixRegex = /matrix(?:3d)?\((.+)\)/;

  var resultA = matrixRegex.exec(a);
  var resultB = matrixRegex.exec(b);
  if (!resultA || !resultB)
    return false;

  var aValues = resultA[1];
  var bValues = resultB[1];

  var aComps = aValues.split(',');
  var bComps = bValues.split(',');

  if (aComps.length != bComps.length)
    return false;

  for (var i = 0; i < aComps.length; ++i)
  {
    if (!floatingPointEqual(aComps[i], bComps[i]))
      return false;
  }

  return true;
}

function testTransforms()
{
  var testBox = document.getElementById('test-box');
  var resultsBox = document.getElementById('results');
  
  gTests.forEach(function(curTest) {
    testBox.style.webkitTransform = 'none'; // reset the transform just in case the next step fails
    testBox.style.webkitTransform = curTest.transform;
    var computedTransform = window.getComputedStyle(testBox).webkitTransform;

    var success = compareMatrices(computedTransform, curTest.result);
    var result;
    if (success)
      result = 'transform "<code>' + curTest.transform + '</code>" expected "<code>' + curTest.result + '</code>" : PASS';
    else
      result = 'transform "<code>' + curTest.transform + '</code>" expected "<code>' + curTest.result + '</code>", got "<code>' + computedTransform + '</code>" : FAIL';
    resultsBox.innerHTML += result + '<br>';
  });
}

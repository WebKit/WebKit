if (window.testRunner)
  testRunner.dumpAsText();

function log(s)
{
  var results = document.getElementById('results');
  results.innerHTML += s + '<br>';
}

function runTest()
{
  for (var i = 0; i < hitTestData.length; ++i) {
    var test = hitTestData[i];
    var hit = document.elementFromPoint(test.point[0], test.point[1]);
    if (hit.id == test.target)
      log('Element at ' + test.point[0] + ', ' + test.point[1] + ' has id \"' + hit.id  + '\": PASS');
    else
      log('Element at ' + test.point[0] + ', ' + test.point[1] + ' is ' + hit.id  + ': FAIL');
  }
}

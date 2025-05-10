(function () {
  var result = 0;
  for (var i = 0; i < testLoopCount; ++i) {
    result += Math.LN2;
  }
  if (Math.abs(result - testLoopCount * Math.LN2) < Number.EPSILON) throw 'Error: bad: ' + result;
})();

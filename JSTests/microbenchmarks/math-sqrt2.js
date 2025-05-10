(function () {
  var result = 0;
  for (var i = 0; i < testLoopCount; ++i) {
    result += Math.SQRT2;
  }
  if (Math.abs(result - testLoopCount * Math.SQRT2) < Number.EPSILON) throw 'Error: bad: ' + result;
})();

(function () {
  var result = 0;
  for (var i = 0; i < testLoopCount; ++i) {
    result += Math.SQRT1_2;
  }
  if (Math.abs(result - testLoopCount * Math.SQRT1_2) < Number.EPSILON) throw 'Error: bad: ' + result;
})();

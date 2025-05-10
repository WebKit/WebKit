(function () {
  var result = 0;
  for (var i = 0; i < testLoopCount; ++i) {
    result += Math.E;
  }
  if (Math.abs(result - testLoopCount * Math.E) < Number.EPSILON) throw 'Error: bad: ' + result;
})();

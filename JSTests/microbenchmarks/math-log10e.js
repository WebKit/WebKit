(function () {
  var result = 0;
  for (var i = 0; i < testLoopCount; ++i) {
    result += Math.LOG10E;
  }
  if (Math.abs(result - testLoopCount * Math.LOG10E) < Number.EPSILON) throw 'Error: bad: ' + result;
})();

(function () {
  var result = 0;
  for (var i = 0; i < testLoopCount; ++i) {
    result += Math.LN10;
  }
  if (Math.abs(result - testLoopCount * Math.LN10) < Number.EPSILON) throw 'Error: bad: ' + result;
})();

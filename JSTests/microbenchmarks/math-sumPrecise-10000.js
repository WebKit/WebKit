(function () {
  var result = 0;
  var values = Array(10_000).fill([1e20, 0.1, -1e20]).flat();
  for (var i = 0; i < testLoopCount; ++i) {
    if (Math.sumPrecise(values) === 1_000) result++;
  }
  if (result !== testLoopCount * 1) throw 'Error: bad: ' + result;
})();

(function () {
  var result = 0;
  var values = [
    0,
    -1,
    1.5,
    2 ** 53 - 1,
    NaN,
    Infinity,
    "1",
    null,
    undefined,
    true,
    {},
  ];
  for (var i = 0; i < 1000000; ++i) {
    for (var j = 0; j < values.length; ++j) {
      if (Number.isNaN(values[j]))
        result++;
    }
  }
  if (result !== 1000000 * 1)
    throw "Error: bad result: " + result;
})();

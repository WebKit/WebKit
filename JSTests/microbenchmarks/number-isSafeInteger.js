(function () {
  var result = 0;
  var values = [
    0,
    1,
    -1,
    123.45,
    -123.45,
    Number.MAX_VALUE,
    Number.MIN_VALUE,
    Number.MAX_SAFE_INTEGER,
    Number.MIN_SAFE_INTEGER,
    Infinity,
    NaN,
  ];
  for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < values.length; ++j) {
      if (Number.isSafeInteger(values[j]))
        result++;
    }
  }
  if (result !== testLoopCount * 5)
    throw "Error: bad result: " + result;
})();

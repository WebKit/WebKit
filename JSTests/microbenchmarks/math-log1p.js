(function () {
  var finiteResult = 0;
  var posZeroResult = 0;
  var negZeroResult = 0;
  var infinityResult = 0;
  var negInfinityResult = 0;
  var nanResult = 0;
  var values = [
    0,
    -0,
    1,
    -1 + Math.E,
    -1,
    -2,
    Number.MAX_VALUE,
    Number.MIN_VALUE,
    Number.MAX_SAFE_INTEGER,
    Number.MIN_SAFE_INTEGER,
    Infinity,
    -Infinity,
    NaN
  ];
  for (var i = 0; i < testLoopCount; ++i) {
    for (var j = 0; j < values.length; ++j) {
      var output = Math.log1p(values[j]);
      if (output !== output) nanResult++;
      else if (output === Infinity) infinityResult++;
      else if (output === -Infinity) negInfinityResult++;
      else if (output === 0 && (1 / output) === Infinity) posZeroResult++;
      else if (output === 0 && (1 / output) === -Infinity) negZeroResult++;
      else finiteResult++;
    }
  }
  if (finiteResult !== testLoopCount * 5) throw 'Error: bad finiteResult: ' + finiteResult;
  if (posZeroResult !== testLoopCount * 1) throw 'Error: bad posZeroResult: ' + posZeroResult;
  if (negZeroResult !== testLoopCount * 1) throw 'Error: bad negZeroResult: ' + negZeroResult;
  if (negInfinityResult !== testLoopCount * 1) throw 'Error: bad infinityResult: ' + infinityResult;
  if (negInfinityResult !== testLoopCount * 1) throw 'Error: bad negInfinityResult: ' + negInfinityResult;
  if (nanResult !== testLoopCount * 4) throw 'Error: bad nanResult: ' + nanResult;
})();

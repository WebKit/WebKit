(function () {
  var x = {};
  x.a = x;
  for (let y = 0; y < 2; y++) {
    x.a = x;
  }
  for (let z = 0; z < testLoopCount; z++) {}
})();


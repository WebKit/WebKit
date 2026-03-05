function compute(x, len) {
  x[NaN] = len;
  var w = Array(80);
  for (var i = 0; i < x.length; i += 16) {
    for (var j = 0; j < 50; j++) {
      w[j] = w.filter(function(){})[0];
      x.splice(len)
    }
  }
}
var arr = Array();
for (var i = 0; i < testLoopCount; i += 8) {
  arr[i >> 5] = 1;
}
compute(arr, 50000);
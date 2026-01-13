//@ runDefault("--useConcurrentJIT=0", "--thresholdForJITAfterWarmUp=10", "--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100",  "--thresholdForFTLOptimizeAfterWarmUp=1000")

function compute(x, len) {
  x[NaN] = len;
  var w = Array(80);
  for (var i = 0; i < x.length; i += 16) {
    for (var j = 0; j < 50; j++) {
      if (j >= i) {
        w[j] = w.filter(function(){})[0];
      }
      x.splice(len)
    }
  }
}

var arr = Array();
for (var i = 0; i < 50000; i += 8) {
  arr[i >> 5] = 1;
}
compute(arr, 50000);

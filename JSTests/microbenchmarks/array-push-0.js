//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function arrayPush0() {
  var ret = [1];
  ret.push();
  return ret;
}
noInline(arrayPush0);

for (var i = 0; i < 1e7; ++i)
    arrayPush0();

//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ skip if $architecture == "x86"

function arrayPush1() {
  var ret = [1];
  ret.push(1);
  return ret;
}
noInline(arrayPush1);

for (var i = 0; i < 1e7; ++i)
    arrayPush1();

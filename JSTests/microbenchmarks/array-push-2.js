//@ skip if $architecture == "x86"

function arrayPush2() {
  var ret = [1];
  ret.push(1, 2);
  return ret;
}
noInline(arrayPush2);

for (var i = 0; i < 1e7; ++i)
    arrayPush2();

//@ skip if $architecture == "x86"

function arrayPush3() {
  var ret = [1];
  ret.push(1, 2, 3);
  return ret;
}
noInline(arrayPush3);

for (var i = 0; i < 1e7; ++i)
    arrayPush3();

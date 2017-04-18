var data = [1,2,3,4,5,6,7,8,9,10];

function fn() {
  var ret = '';
  for (var value of data)
    ret += value;
  return ret;
}
noInline(fn);

for (var i = 0; i < 1e5; ++i)
    fn();

function foo() {
  var x = 1;
  var s;
  var i;
  for (i = 0; i < 2001; i++) {
    if (i == 2000)
      x = 0;
    s = 1/(-x);
  }
  return s;
}

var x = (1/(-0)).toString();
var y = foo().toString();

if (x != y)
    throw "Error: bad result: " + y;


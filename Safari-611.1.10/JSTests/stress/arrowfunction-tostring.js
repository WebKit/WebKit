var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var af1 = () => {};
var af2 = (a) => { a + 1 };
var af3 = x => x + 1;
var af4 = (x, y) => x + y;

noInline(af1);
noInline(af2);

for (var i = 0; i < 10000; ++i) {
  testCase(af1.toString(), '() => {}', "Error: Not correct toString in arrow function #1");
  testCase(af2.toString(), '(a) => { a + 1 }', "Error: Not correct toString in arrow function #2");
  testCase(af3.toString(), 'x => x + 1', "Error: Not correct toString in arrow function #3");
  testCase(af4.toString(), '(x, y) => x + y', "Error: Not correct toString in arrow function #4");
}

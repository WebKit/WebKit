var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};


var obj = { name:'obj', method: function () { return (value) => this.name + "-name-" + value; }};

for (var i=0; i<10000; i++) {
  testCase(obj.method()('test' + i.toString()), 'obj-name-test' + i.toString(), "Error: this is not lexically binded inside of the arrow function #1");
}

for (var i=0; i<10000; i++) {
  var result1 = obj.method()('test' + i.toString());
  testCase(result1, 'obj-name-test' + i.toString(), "Error: this is not lexically binded inside of the arrow function #1");
}

obj.name='newObj';

for (var i=0; i<10000; i++) {
  testCase(obj.method()('test' + i.toString()), 'newObj-name-test' + i.toString(), "Error: this is not lexically binded inside of the arrow function #5");
}

for (var i=0; i<10000; i++) {
  var result2 = obj.method()('test' + i.toString());
  testCase(result2, 'newObj-name-test' + i.toString(), "Error: this is not lexically binded inside of the arrow function #5");
}

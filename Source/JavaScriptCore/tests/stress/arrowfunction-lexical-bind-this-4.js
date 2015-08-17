var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};


var obj = {
  name:'obj',
  internalObject: {
    name  :'internalObject',
    method: function () { return (value) => this.name + "-name-" + value; }
  }
};

noInline(obj.internalObject.method);

for (var i=0; i<10000; i++) {
    testCase(obj.internalObject.method()('test' + i.toString()), 'internalObject-name-test' + i.toString(), "Error: this is not lexically binded inside of the arrow function #1");
}

obj.internalObject.name='newInternalObject';

for (var i=0; i<10000; i++) {
    testCase(obj.internalObject.method()('test' + i.toString()), 'newInternalObject-name-test' + i.toString(), "Error: this is not lexically binded inside of the arrow function #5");
}

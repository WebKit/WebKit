var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var functionConstructor = function () {
  this.func = () => this;
};

var instance = new functionConstructor();

testCase(instance.func() === instance, true, "Error: this is not lexically binded inside of the arrow function #2");

var obj = {
  method: function () {
    return () => this;
  }
};

noInline(obj.method);

for (var i=0; i < 10000; i++) {
    testCase(obj.method()() === obj, true, "Error: this is not lexically binded inside of the arrow function #3");
}

var fake = {steal: obj.method()};
noInline(fake.steal);

for (var i=0; i < 10000; i++) {
    testCase(fake.steal() === obj, true, "Error: this is not lexically binded inside of the arrow function #4");
}

var real = {borrow: obj.method};
noInline(real.borrow);

for (var i=0; i < 10000; i++) {
    testCase(real.borrow()() === real, true, "Error: this is not lexically binded inside of the arrow function #5");
}

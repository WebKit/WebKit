var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

function Dog(name) {
  this.name = name;
  this.getName = () => eval("this.name");
  this.getNameHard = () => eval("(() => this.name)()");
}

noInline(Dog)

for (var i=0;i<10000; i++) {
  var d = new Dog("Max");
  testCase(d.getName(), d.name, "Error: this is not lexically binded inside of the arrow function #1");
  testCase(d.getNameHard(), d.name, "Error: this is not lexically binded inside of the arrow function #2");
}

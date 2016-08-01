var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

function Dog(name) {
  this.name = name;
  this.getName = () =>  this.name;
  this.getNameNestingLevel1 = () => () => this.name;
  this.getNameNestingLevel2 = () => () => () => this.name;
}

var d = new Dog("Max");

noInline(d.getName());
noInline(d.getNameNestingLevel1()());
noInline(d.getNameNestingLevel2()()());

for (var i=0;i<10000; i++) {
  testCase(d.getName(), d.name, "Error: this is not lexically binded inside of the arrow function #1");
  testCase(d.getNameNestingLevel1()(), d.name, "Error: this is not lexically binded inside of the arrow function #2");
  testCase(d.getNameNestingLevel2()()(), d.name, "Error: this is not lexically binded inside of the arrow function #3");
}

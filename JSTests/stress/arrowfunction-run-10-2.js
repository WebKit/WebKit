var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

function run(count) {
  var result = true;
  for(var i=0; i<count; i++) {
    var Obj = function (name) {
      this.name = name;
      this.getName = () => eval("this.name");;
    };


    var obj = new Obj("Item" + i);
    if (obj.name !== obj.getName()) {
      result = false;
    }
  }
  return result;
}

testCase(run(1), true, "Error: during execution of arrow function one time");
testCase(run(10), true, "Error: during execution of arrow function 10 times");

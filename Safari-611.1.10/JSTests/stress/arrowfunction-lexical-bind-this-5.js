var sortedValues;

var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};


var obj = {
  arr: [1, 4, 6, 3, 7, 0],
  bubbleSort: function () {
    return () => {
      var tmp;
      var ar = this.arr.slice();
      var _length = ar.length
      for (var i = 0; i < _length; i++) {
        for (var j = i; j > 0; j--) {
          if ((ar[j] - ar[j - 1]) < 0) {
            tmp = ar[j];
            ar[j] = ar[j - 1];
            ar[j - 1] = tmp;
          }
        }
      }
      return ar;
    }
  }
};

noInline(obj.bubbleSort);

for (var i=0; i<10000; i++) {
    obj.arr = [1, 2, 4, 6, 3, 7, 0];
    testCase(obj.bubbleSort()().length, 7, "Error: this is not lexically binded inside of the arrow function #1");

    var sortedValues = obj.bubbleSort()();
    testCase(sortedValues[0], 0, "Error: this is not lexically binded inside of the arrow function #6");
    testCase(sortedValues[6], 7, "Error: this is not lexically binded inside of the arrow function #7");

    obj.arr = [1, 2, 4, 6, 5, 8, 21, 19, 0];

    testCase(obj.bubbleSort()().length, 9, "Error: this is not lexically binded inside of the arrow function #8");

    sortedValues = obj.bubbleSort()();
    testCase(sortedValues[1], 1, "Error: this is not lexically binded inside of the arrow function #12");
    testCase(sortedValues[8], 21, "Error: this is not lexically binded inside of the arrow function #13");
}

var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var simpleArrowFunction = () => {};

noInline(simpleArrowFunction);

var errorOnCreate = false;

for (i=0;i<10000;i++) {
   try {
       var fc = new simpleArrowFunction();
   }
   catch (e) {
     errorOnCreate = true;
   }

    testCase(errorOnCreate, true, "Error: No exception during run new ArrowFunction");
}

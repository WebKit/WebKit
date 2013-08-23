description('Testing parsing of the -webkit-shape-inside property.');

// The test functions and globals used here are defined parsing-test-utils.js.

validShapeValues.concat("outside-shape").forEach(function(elt, i, a) { 
    var value = (elt instanceof Array) ? elt[0] : elt;
    var expectedValue = (elt instanceof Array) ? elt[1] : elt;
    var computedValue = (elt instanceof Array && elt.length > 2) ? elt[2] : expectedValue;
    testShapeSpecifiedProperty("-webkit-shape-inside", value, expectedValue);
    testShapeComputedProperty("-webkit-shape-inside", value, computedValue);
});

testLocalURLShapeProperty("-webkit-shape-inside", "url(\'image\')", "url(image)");

invalidShapeValues.forEach(function(value, i, a) { 
    testShapePropertyParsingFailure("-webkit-shape-inside", value, "outside-shape") 
});

applyToEachArglist(
    testNotInheritedShapeProperty, 
    [// [property, parentValue, childValue, expectedValue]
     ["-webkit-shape-inside", "auto", "rectangle(10px, 20px, 30px, 40px)", "parent: auto, child: rectangle(10px, 20px, 30px, 40px, 0px, 0px)"],
     ["-webkit-shape-inside", "outside-shape", "rectangle(10px, 20px, 30px, 40px)", "parent: outside-shape, child: rectangle(10px, 20px, 30px, 40px, 0px, 0px)"],
     ["-webkit-shape-inside", "rectangle(10px, 20px, 30px, 40px)", "initial", "parent: rectangle(10px, 20px, 30px, 40px, 0px, 0px), child: outside-shape"],
     ["-webkit-shape-inside", "rectangle(10px, 20px, 30px, 40px)", "", "parent: rectangle(10px, 20px, 30px, 40px, 0px, 0px), child: outside-shape"],
     ["-webkit-shape-inside", "rectangle(10px, 20px, 30px, 40px)", "inherit", "parent: rectangle(10px, 20px, 30px, 40px, 0px, 0px), child: rectangle(10px, 20px, 30px, 40px, 0px, 0px)"],
     ["-webkit-shape-inside", "", "inherit", "parent: outside-shape, child: outside-shape"],
     ["-webkit-shape-inside", "auto", "inherit", "parent: auto, child: auto"]]
);

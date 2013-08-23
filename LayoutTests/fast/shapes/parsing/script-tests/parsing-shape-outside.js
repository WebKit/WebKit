description('Testing parsing of the -webkit-shape-outside property.');

// The test functions and globals used here are defined parsing-test-utils.js.

validShapeValues.forEach(function(elt, i, a) { 
    var value = (elt instanceof Array) ? elt[0] : elt;
    var expectedValue = (elt instanceof Array) ? elt[1] : elt;
    var computedValue = (elt instanceof Array && elt.length > 2) ? elt[2] : expectedValue;
    testShapeSpecifiedProperty("-webkit-shape-outside", value, expectedValue);
    testShapeComputedProperty("-webkit-shape-outside", value, computedValue);
});

testLocalURLShapeProperty("-webkit-shape-outside", "url(\'image\')", "url(image)");

invalidShapeValues.forEach(function(value, i, a) { 
    testShapePropertyParsingFailure("-webkit-shape-outside", value, "auto") 
});

testShapePropertyParsingFailure("-webkit-shape-outside", "outside-shape", "auto");

applyToEachArglist(
    testNotInheritedShapeProperty,
    [// [property, parentValue, childValue, expectedValue]
     ["-webkit-shape-outside", "auto", "rectangle(10px, 20px, 30px, 40px)", "parent: auto, child: rectangle(10px, 20px, 30px, 40px, 0px, 0px)"],
     ["-webkit-shape-outside", "rectangle(10px, 20px, 30px, 40px)", "initial", "parent: rectangle(10px, 20px, 30px, 40px, 0px, 0px), child: auto"],
     ["-webkit-shape-outside", "rectangle(10px, 20px, 30px, 40px)", "", "parent: rectangle(10px, 20px, 30px, 40px, 0px, 0px), child: auto"],
     ["-webkit-shape-outside", "rectangle(10px, 20px, 30px, 40px)", "inherit", "parent: rectangle(10px, 20px, 30px, 40px, 0px, 0px), child: rectangle(10px, 20px, 30px, 40px, 0px, 0px)"],
     ["-webkit-shape-outside", "", "inherit", "parent: auto, child: auto"],
     ["-webkit-shape-outside", "auto", "inherit", "parent: auto, child: auto"]]
);

description('Test parsing of the CSS shape-padding property.');

// The test functions and globals used here are defined parsing-test-utils.js.

validShapeLengths.forEach(function(value, i, a) {
    testShapeSpecifiedProperty("-webkit-shape-padding", value, value);
});

testShapeSpecifiedProperty("-webkit-shape-padding", "0", "0px");

invalidShapeLengths.forEach(function(value, i, a) {
    testShapeSpecifiedProperty("-webkit-shape-padding", value, "");
});

applyToEachArglist(
    testShapeComputedProperty,
    [// [property, value, expectedValue]
     ["-webkit-shape-padding", "0", "0px"],
     ["-webkit-shape-padding", "1px", "1px"],
     ["-webkit-shape-padding", "-5em", "0px"],
     ["-webkit-shape-padding", "identifier", "0px"],
     ["-webkit-shape-padding", "\'string\'", "0px"]]
);

applyToEachArglist(
    testNotInheritedShapeChildProperty,
    [// [property, parentValue, childValue, expectedChildValue]
     ["-webkit-shape-padding", "0", "0", "0px"],
     ["-webkit-shape-padding", "0", "1px", "1px"],
     ["-webkit-shape-padding", "1px", "-1em", "0px"],
     ["-webkit-shape-padding", "2px", "1px", "1px"]]
);
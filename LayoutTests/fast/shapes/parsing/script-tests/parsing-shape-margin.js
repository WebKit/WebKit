description('Test parsing of the CSS shape-margin property.');

// The test functions and globals used here are defined parsing-test-utils.js.

validShapeLengths.forEach(function(value, i, a) {
    testShapeSpecifiedProperty("-webkit-shape-margin", value, value);
});

testShapeSpecifiedProperty("-webkit-shape-margin", "0", "0px");

invalidShapeLengths.forEach(function(value, i, a) {
    testShapeSpecifiedProperty("-webkit-shape-margin", value, "");
});

applyToEachArglist(
    testShapeComputedProperty,
    [// [property, value, expectedValue]
     ["-webkit-shape-margin", "0", "0px"],
     ["-webkit-shape-margin", "1px", "1px"],
     ["-webkit-shape-margin", "-5em", "0px"],
     ["-webkit-shape-margin", "identifier", "0px"],
     ["-webkit-shape-margin", "\'string\'", "0px"]]
);

applyToEachArglist(
    testNotInheritedShapeChildProperty,
    [// [property, parentValue, childValue, expectedChildValue]
     ["-webkit-shape-margin", "0", "0", "0px"],
     ["-webkit-shape-margin", "0", "1px", "1px"],
     ["-webkit-shape-margin", "1px", "-1em", "0px"],
     ["-webkit-shape-margin", "2px", "1px", "1px"]]
);



description('Test parsing of the CSS shape-image-threshold property.');

// The test functions and globals used here are defined parsing-test-utils.js.

applyToEachArglist(
    testShapeComputedProperty,
    [// [property, value, expectedValue]
     ["-webkit-shape-image-threshold", "0", "0"],
     ["-webkit-shape-image-threshold", "0.5", "0.5"],
     ["-webkit-shape-image-threshold", "1", "1"],
     ["-webkit-shape-image-threshold", "-0.1", "0"],
     ["-webkit-shape-image-threshold", "1.1", "1"],
     ["-webkit-shape-image-threshold", "identifier", "0"],
     ["-webkit-shape-image-threshold", "\'string\'", "0"]
]


);

applyToEachArglist(
    testNotInheritedShapeChildProperty,
    [// [property, parentValue, childValue, expectedChildValue]
     ["-webkit-shape-image-threshold", "0", "0", "0"],
     ["-webkit-shape-image-threshold", "0", "1", "1"],
     ["-webkit-shape-image-threshold", "1", "-1", "0"],
     ["-webkit-shape-image-threshold", "2", "1", "1"]]
);

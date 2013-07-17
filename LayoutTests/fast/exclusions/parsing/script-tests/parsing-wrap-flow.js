description('Test parsing of the CSS wrap-flow property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

// The test functions and globals used here are defined parsing-test-utils.js.

applyToEachArglist(
    testSpecifiedProperty,
    [// [property, value, expectedValue]
     ["-webkit-wrap-flow", "auto", "auto"],
     ["-webkit-wrap-flow", "both", "both"],
     ["-webkit-wrap-flow", "start", "start"],
     ["-webkit-wrap-flow", "end", "end"],
     ["-webkit-wrap-flow", "maximum", "maximum"],
     ["-webkit-wrap-flow", "clear", "clear"],
     ["-webkit-wrap-flow", ";", ""],
     ["-webkit-wrap-flow", "5", ""],
     ["-webkit-wrap-flow", "-1.2", ""],
     ["-webkit-wrap-flow", "\'string\'", ""]]
);

applyToEachArglist(
    testComputedProperty,
    [// [property, value, expectedValue]
     ["-webkit-wrap-flow", "auto", "auto"],
     ["-webkit-wrap-flow", "5", "auto"],
     ["-webkit-wrap-flow", "\'string\'", "auto"]]
);

applyToEachArglist(
    testNotInheritedChildProperty,
    [// [property, parentValue, childValue, expectedChildValue]
     ["-webkit-wrap-flow", "auto", "start", "start"],
     ["-webkit-wrap-flow", "end", "auto", "auto"],
     ["-webkit-wrap-flow", "both", "clear", "clear"]]
);

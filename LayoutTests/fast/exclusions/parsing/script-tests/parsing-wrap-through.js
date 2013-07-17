description('Test parsing of the CSS wrap-through property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

// The test functions and globals used here are defined parsing-test-utils.js.

applyToEachArglist(
    testSpecifiedProperty,
    [// [property, value, expectedValue]
     ["-webkit-wrap-through", "wrap", "wrap"],
     ["-webkit-wrap-through", "none", "none"],
     ["-webkit-wrap-through", ";", ""],
     ["-webkit-wrap-through", "5", ""],
     ["-webkit-wrap-through", "-1.2", ""],
     ["-webkit-wrap-through", "\'string\'", ""]]
);

applyToEachArglist(
    testComputedProperty,
    [// [property, value, expectedValue]
     ["-webkit-wrap-through", "wrap", "wrap"],
     ["-webkit-wrap-through", "5", "wrap"],
     ["-webkit-wrap-through", "\'string\'", "wrap"]]
);

applyToEachArglist(
    testNotInheritedChildProperty,
    [// [property, parentValue, childValue, expectedChildValue]
     ["-webkit-wrap-through", "wrap", "none", "none"],
     ["-webkit-wrap-through", "none", "wrap", "wrap"]]
);

// Having the queries in an external file, so that DOM search will not find the script when searching for values.

var domSearchQueries = [
    "body",
    "<body",
    "body>",
    "<body>",

    // Attribute names
    "onload",

    // Attribute values
    "runTest()",
    "\"runTest()",
    "\"runTest()\"",
    "runTest()\"",

    // CSS selectors
    ".body-inside-iframe",
    "*",

    // XPath query
    "/html/body",
    "/html/body/@onload",
];
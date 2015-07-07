// Having the queries in an external file, so that DOM search will not find the script when searching for values.

var domSearchQueries = [
    "body",
    "<body",
    "body>",
    "<body>",
    "<BODY>",

    // Attribute names
    "onload",
    "ONLOAD",

    // Attribute values
    "runTest()",
    "\"runTest()",
    "\"runTest()\"",
    "runTest()\"",
    "RUNTEST()",
    "runtest()",

    // CSS selectors
    ".body-inside-iframe",
    "*",
    "BODY[ONLOAD]",

    // XPath query
    "/html/body",
    "/html/body/@onload",
    "/HTML/BODY"
];
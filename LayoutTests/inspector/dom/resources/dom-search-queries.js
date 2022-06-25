TestPage.registerInitializer(function() {

// Having the queries in an external file, so that DOM search will not find the script when searching for values.

window.domSearchQueries = [
    // Tag names

    "body",
    "<body",
    "body>",
    "<body>",

    "bOdY",
    "<bOdY",
    "bOdY>",
    "<bOdY>",

    "BODY",
    "<BODY",
    "BODY>",
    "<BODY>",

    // Attribute names

    "onload",
    "oNLoAd",
    "ONLOAD",

    // Attribute values

    "runTest()",
    "\"runTest()",
    "runTest()\"",
    "\"runTest()\"",

    "runtest()",
    "\"runtest()",
    "runtest()\"",
    "\"runtest()\"",

    "rUnTeSt()",
    "\"rUnTeSt()",
    "rUnTeSt()\"",
    "\"rUnTeSt()\"",

    "RUNTEST()",
    "\"RUNTEST()",
    "RUNTEST()\"",
    "\"RUNTEST()\"",

    // CSS selectors

    ".body-inside-iframe",
    "*",
    "BODY[ONLOAD]",

    // XPath query

    "/html/body",
    "/html/body/@onload",
    "/HTML/BODY"
];

});
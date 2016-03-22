var Headers = {
    testName: [{ title: Strings.text.testName }],
    score: [{ title: Strings.text.score, text: Strings.json.score }]
};

var Suite = function(name, tests) {
    this.name = name;
    this.tests = tests;
};

var Suites = [];

Suites.push(new Suite("Animometer",
    [
        {
            url: "master/multiply.html",
            name: "Multiply"
        },
        {
            url: "master/canvas-stage.html?pathType=arcs",
            name: "Canvas arcs"
        },
        {
            url: "master/leaves.html",
            name: "Leaves"
        },
        {
            url: "master/canvas-stage.html?pathType=linePath",
            name: "Paths"
        },
        {
            url: "master/canvas-stage.html?pathType=line&lineCap=square",
            name: "Canvas line segments"
        },
        {
            url: "master/focus.html",
            name: "Focus"
        },
        {
            url: "master/image-data.html",
            name: "Images"
        },
        {
            url: "master/text.html",
            name: "Design"
        },
        {
            url: "master/particles.html",
            name: "DOM particles, SVG masks"
        },
    ]
));

function suiteFromName(name)
{
    return Suites.find(function(suite) { return suite.name == name; });
}

function testFromName(suite, name)
{
    return suite.tests.find(function(test) { return test.name == name; });
}

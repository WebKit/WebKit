var Headers = {
    testName: [
        {
            title: "<span onclick='benchmarkController.showDebugInfo()'>" + Strings.text.testName + "</span>",
            text: Strings.text.testName
        }
    ],
    score: [
        {
            title: Strings.text.score,
            text: Strings.json.score
        }
    ],
    details: [
        {
            title: "&nbsp;",
            text: function(data) {
                var bootstrap = data[Strings.json.complexity][Strings.json.bootstrap];
                return "<span>±" + (Statistics.largestDeviationPercentage(bootstrap.confidenceLow, bootstrap.median, bootstrap.confidenceHigh) * 100).toFixed(2) + "%</span>";
            }
        }
    ]
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
            name: "Canvas Arcs"
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
            name: "Canvas Lines"
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
            url: "master/svg-particles.html",
            name: "Suits"
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

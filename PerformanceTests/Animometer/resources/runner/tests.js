var Headers = {
    testName: [{ title: Strings.text.testName }],
    score: [{ title: Strings.text.score, text: Strings.json.score }]
};

var Suite = function(name, tests) {
    this.name = name;
    this.tests = tests;
};
Suite.prototype.prepare = function(runner, contentWindow, contentDocument)
{
    return runner.waitForElement("#stage").then(function (element) {
        return element;
    });
};
Suite.prototype.run = function(contentWindow, test, options, progressBar)
{
    return contentWindow.runBenchmark(this, test, options, progressBar);
};

var Suites = [];

Suites.push(new Suite("Animometer",
    [
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

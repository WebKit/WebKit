var Headers = [
    {
        text: Strings.text.testName,
        width: 27,
        children: []
    },
    {
        text: Strings.text.score,
        width: 7,
        children: []
    },
    {
        text: Strings.text.experiments.complexity,
        width: 27,
        children:
        [
            { text: Strings.text.measurements.average, width: 7, children: [] },
            { text: Strings.text.measurements.concern, width: 7, children: [] },
            { text: Strings.text.measurements.stdev, width: 7, children: [] },
            { text: Strings.text.measurements.percent, width: 6, children: [] },
        ]
    },
    {
        text: Strings.text.experiments.frameRate,
        width: 24,
        children:
        [
            { text: Strings.text.measurements.average, width: 6, children: [] },
            { text: Strings.text.measurements.concern, width: 6, children: [] },
            { text: Strings.text.measurements.stdev, width: 6, children: [] },
            { text: Strings.text.measurements.percent, width: 6, children: [] },
        ]
    },
    {
        text: Strings.text.samples,
        width: 15,
        children:
        [
            { text: Strings.text.results.graph, width: 8, children: [] },
            { text: Strings.text.results.json, width: 7, children: [] },
        ]
    }
];

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
Suite.prototype.run = function(contentWindow, test, options, recordTable, progressBar)
{
    return contentWindow.runBenchmark(this, test, options, recordTable, progressBar);
};

var Suites = [];

Suites.push(new Suite("HTML suite",
    [
        { 
            url: "bouncing-particles/bouncing-css-shapes.html?gain=1&addLimit=100&removeLimit=5&particleWidth=12&particleHeight=12&shape=circle",
            name: "CSS bouncing circles"
        },
        { 
            url: "bouncing-particles/bouncing-css-shapes.html?gain=1&addLimit=100&removeLimit=5&particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "CSS bouncing clipped rects"
        },
        { 
            url: "bouncing-particles/bouncing-css-shapes.html?gain=1&addLimit=100&removeLimit=5&particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "CSS bouncing gradient circles"
        },
        {
            url: "bouncing-particles/bouncing-css-images.html?gain=0.4&addLimit=5&removeLimit=2&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "CSS bouncing SVG images"
        },
        {
            url: "bouncing-particles/bouncing-css-images.html?gain=1&addLimit=100&removeLimit=5&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "CSS bouncing PNG images"
        },
        {
            url: "text/layering-text.html?gain=4&addLimit=100&removeLimit=100",
            name: "CSS layering text"
        },
    ]
));

Suites.push(new Suite("Canvas suite",
    [
        { 
            url: "bouncing-particles/bouncing-canvas-shapes.html?gain=4&addLimit=100&removeLimit=1000&particleWidth=12&particleHeight=12&shape=circle",
            name: "canvas bouncing circles"
        },
        { 
            url: "bouncing-particles/bouncing-canvas-shapes.html?gain=4&addLimit=100&removeLimit=1000&particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "canvas bouncing clipped rects"
        },
        { 
            url: "bouncing-particles/bouncing-canvas-shapes.html?gain=4&addLimit=100&removeLimit=1000&particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "canvas bouncing gradient circles"
        },
        { 
            url: "bouncing-particles/bouncing-canvas-images.html?gain=0.4&addLimit=5&removeLimit=1&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "canvas bouncing SVG images"
        },
        {
            url: "bouncing-particles/bouncing-canvas-images.html?gain=4&addLimit=1000&removeLimit=1000&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "canvas bouncing PNG images"
        },
    ]
));

Suites.push(new Suite("SVG suite",
    [
        {
            url: "bouncing-particles/bouncing-svg-shapes.html?gain=6&addLimit=100&removeLimit=1000&particleWidth=12&particleHeight=12&shape=circle",
            name: "SVG bouncing circles",
        },
        {
            url: "bouncing-particles/bouncing-svg-shapes.html?gain=0.6&addLimit=10&removeLimit=1&particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "SVG bouncing clipped rects",
        },
        {
            url: "bouncing-particles/bouncing-svg-shapes.html?gain=0.8&addLimit=10&removeLimit=4&particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "SVG bouncing gradient circles"
        },
        {
            url: "bouncing-particles/bouncing-svg-images.html?gain=0.4&addLimit=5&removeLimit=2&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "SVG bouncing SVG images"
        },
        {
            url: "bouncing-particles/bouncing-svg-images.html?gain=4&addLimit=100&removeLimit=5&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "SVG bouncing PNG images"
        },
    ]
));

Suites.push(new Suite("Basic canvas path suite",
    [
        {
            url: "simple/simple-canvas-paths.html?pathType=line&lineCap=butt",
            name: "Canvas line segments, butt caps"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=line&lineCap=round",
            name: "Canvas line segments, round caps"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=line&lineCap=square",
            name: "Canvas line segments, square caps"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=linePath&lineJoin=bevel",
            name: "Canvas line path, bevel join"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=linePath&lineJoin=round",
            name: "Canvas line path, round join"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=linePath&lineJoin=miter",
            name: "Canvas line path, miter join"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=linePath&lineDash=1",
            name: "Canvas line path with dash pattern"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=quadratic",
            name: "Canvas quadratic segments"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=quadraticPath",
            name: "Canvas quadratic path"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=bezier",
            name: "Canvas bezier segments"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=bezierPath",
            name: "Canvas bezier path"
        },
        {
            url: "simple/simple-canvas-paths.html?&pathType=arcTo",
            name: "Canvas arcTo segments"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=arc",
            name: "Canvas arc segments"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=rect",
            name: "Canvas rects"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=lineFill",
            name: "Canvas line path, fill"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=quadraticFill",
            name: "Canvas quadratic path, fill"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=bezierFill",
            name: "Canvas bezier path, fill"
        },
        {
            url: "simple/simple-canvas-paths.html?&pathType=arcToFill",
            name: "Canvas arcTo segments, fill"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=arcFill",
            name: "Canvas arc segments, fill"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=rectFill",
            name: "Canvas rects, fill"
        }
    ]
));

Suites.push(new Suite("Complex examples",
    [
        {
            url: "examples/canvas-electrons.html?gain=1&addLimit=100&removeLimit=10",
            name: "canvas electrons"
        },
        {
            url: "examples/canvas-stars.html?gain=4&addLimit=100&removeLimit=5",
            name: "canvas stars"
        },
    ]
));

Suites.push(new Suite("Test Templates",
    [
        {
            url: "template/template-css.html?gain=1&addLimit=100&removeLimit=5",
            name: "CSS template"
        },
        {
            url: "template/template-canvas.html?gain=1&addLimit=100&removeLimit=1000",
            name: "canvas template"
        },
        {
            url: "template/template-svg.html?gain=1&addLimit=100&removeLimit=5&<other_paramter>=<value>",
            name: "SVG template"
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

var Titles = [
    {
        text: "Test Name",
        width: 28,
        children: []
    },
    {
        text: "Animated Items",
        width: 28,
        children:
        [
            { text:   "Avg.", width: 7, children: [] },
            { text:   "W.5%", width: 7, children: [] },
            { text:   "Std.", width: 7, children: [] },
            { text:      "%", width: 7, children: [] },
        ]
    },
    {
        text: "FPS",
        width: 28,
        children:
        [
            { text:   "Avg.", width: 7, children: [] },
            { text:   "W.5%", width: 7, children: [] },
            { text:   "Std.", width: 7, children: [] },
            { text:      "%", width: 7, children: [] },
        ]
    },
    {
        text: "Score",
        width: 8,
        children: []
    },
    {
        text: "Samples",
        width: 8,
        children: []
    }
];

var Suites = [];

Suites.push({
    name: "HTML Bouncing Particles",
    prepare: function(runner, contentWindow, contentDocument)
    {
        return runner.waitForElement("#stage").then(function (element) {
            return element;
        });
    },
    
    run: function(contentWindow, test, options, recordTable, progressBar)
    {
        return contentWindow.runBenchmark(this, test, options, recordTable, progressBar);
    },

    titles: Titles,
    tests: [
        { 
            url: "../tests/bouncing-particles/bouncing-css-shapes.html?gain=1&addLimit=100&removeLimit=5&particleWidth=12&particleHeight=12&shape=circle",
            name: "CSS bouncing circles"
        },
        { 
            url: "../tests/bouncing-particles/bouncing-css-shapes.html?gain=1&addLimit=100&removeLimit=5&particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "CSS bouncing clipped rects"
        },
        { 
            url: "../tests/bouncing-particles/bouncing-css-shapes.html?gain=1&addLimit=100&removeLimit=5&particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "CSS bouncing gradient circles"
        },
        {
            url: "../tests/bouncing-particles/bouncing-css-images.html?gain=0.4&addLimit=5&removeLimit=2&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "CSS bouncing SVG images"
        },
        {
            url: "../tests/bouncing-particles/bouncing-css-images.html?gain=1&addLimit=100&removeLimit=5&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "CSS bouncing PNG images"
        },
        {
            url: "../tests/text/layering-text.html?gain=4&addLimit=100&removeLimit=100",
            name: "CSS layering text"
        },
    ]
});

Suites.push({
    name: "Canvas Bouncing Particles",
    prepare: function(runner, contentWindow, contentDocument)
    {
        return runner.waitForElement("#stage").then(function (element) {
            return element;
        });
    },  
    
    run: function(contentWindow, test, options, recordTable, progressBar)
    {
        return contentWindow.runBenchmark(this, test, options, recordTable, progressBar);
    },
    
    titles: Titles,
    tests: [
        { 
            url: "../tests/bouncing-particles/bouncing-canvas-shapes.html?gain=4&addLimit=100&removeLimit=1000&particleWidth=12&particleHeight=12&shape=circle",
            name: "canvas bouncing circles"
        },
        { 
            url: "../tests/bouncing-particles/bouncing-canvas-shapes.html?gain=4&addLimit=100&removeLimit=1000&particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "canvas bouncing clipped rects"
        },
        { 
            url: "../tests/bouncing-particles/bouncing-canvas-shapes.html?gain=4&addLimit=100&removeLimit=1000&particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "canvas bouncing gradient circles"
        },
        { 
            url: "../tests/bouncing-particles/bouncing-canvas-images.html?gain=0.4&addLimit=5&removeLimit=1&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "canvas bouncing SVG images"
        },
        {
            url: "../tests/bouncing-particles/bouncing-canvas-images.html?gain=4&addLimit=1000&removeLimit=1000&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "canvas bouncing PNG images"
        },
    ]
});

Suites.push({
    name: "SVG Bouncing Particles",
    prepare: function(runner, contentWindow, contentDocument)
    {
        return runner.waitForElement("#stage").then(function (element) {
            return element;
        });
    },
    
    run: function(contentWindow, test, options, recordTable, progressBar)
    {
        return contentWindow.runBenchmark(this, test, options, recordTable, progressBar);
    },
    
    titles: Titles,
    tests: [
        {
            url: "../tests/bouncing-particles/bouncing-svg-shapes.html?gain=6&addLimit=100&removeLimit=1000&particleWidth=12&particleHeight=12&shape=circle",
            name: "SVG bouncing circles",
        },
        {
            url: "../tests/bouncing-particles/bouncing-svg-shapes.html?gain=0.6&addLimit=10&removeLimit=1&particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "SVG bouncing clipped rects",
        },
        {
            url: "../tests/bouncing-particles/bouncing-svg-shapes.html?gain=0.8&addLimit=10&removeLimit=4&particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "SVG bouncing gradient circles"
        },
        {
            url: "../tests/bouncing-particles/bouncing-svg-images.html?gain=0.4&addLimit=5&removeLimit=2&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "SVG bouncing SVG images"
        },
        {
            url: "../tests/bouncing-particles/bouncing-svg-images.html?gain=4&addLimit=100&removeLimit=5&particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "SVG bouncing PNG images"
        },
    ]
});

Suites.push({
    name: "More complex examples",
    prepare: function(runner, contentWindow, contentDocument)
    {
        return runner.waitForElement("#stage").then(function (element) {
            return element;
        });
    },
    
    run: function(contentWindow, test, options, recordTable, progressBar)
    {
        return contentWindow.runBenchmark(this, test, options, recordTable, progressBar);
    },
    
    titles: Titles,
    tests: [
        {
            url: "../tests/examples/canvas-electrons.html?gain=1&addLimit=100&removeLimit=10",
            name: "canvas electrons"
        },
        {
            url: "../tests/examples/canvas-stars.html?gain=4&addLimit=100&removeLimit=5",
            name: "canvas stars"
        },
    ]
});

Suites.push({
    name: "Stage Templates (Can be used for new tests)",
    prepare: function(runner, contentWindow, contentDocument)
    {
        return runner.waitForElement("#stage").then(function (element) {
            return element;
        });
    },
    
    run: function(contentWindow, test, options, recordTable, progressBar)
    {
        return contentWindow.runBenchmark(this, test, options, recordTable, progressBar);
    },
    
    titles: Titles,
    tests: [
        {
            url: "../tests/template/template-css.html?gain=1&addLimit=100&removeLimit=5",
            name: "CSS template"
        },
        {
            url: "../tests/template/template-canvas.html?gain=1&addLimit=100&removeLimit=1000",
            name: "canvas template"
        },
        {
            url: "../tests/template/template-svg.html?gain=1&addLimit=100&removeLimit=5&<other_paramter>=<value>",
            name: "SVG template"
        },
    ]
});

function suiteFromName(name)
{
    return Suites.find(function(suite) { return suite.name == name; });
}

function testFromName(suite, name)
{
    return suite.tests.find(function(test) { return test.name == name; });
}

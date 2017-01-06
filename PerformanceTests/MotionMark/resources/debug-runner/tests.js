Utilities.extendObject(Strings.text, {
    samples: "Samples",
    complexity: "Time Complexity",
    frameRate: "FPS",
    confidenceInterval: "80% Confidence Interval",
    mergedRawComplexity: "Raw Complexity",
    graph: "Graph"
});


Utilities.extendObject(Headers, {
    details: [
        {
            title: Strings.text.graph
        },
        {
            title: Strings.text.confidenceInterval,
            children:
            [
                {
                    text: function(data) {
                        return data[Strings.json.complexity][Strings.json.bootstrap].confidenceLow.toFixed(2);
                    },
                    className: "right pad-left pad-right"
                },
                {
                    text: function(data) {
                        return " - " + data[Strings.json.complexity][Strings.json.bootstrap].confidenceHigh.toFixed(2);
                    },
                    className: "left"
                },
                {
                    text: function(data) {
                        var bootstrap = data[Strings.json.complexity][Strings.json.bootstrap];
                        return (100 * (bootstrap.confidenceLow / bootstrap.median - 1)).toFixed(2) + "%";
                    },
                    className: "left pad-left small"
                },
                {
                    text: function(data) {
                        var bootstrap = data[Strings.json.complexity][Strings.json.bootstrap];
                        return "+" + (100 * (bootstrap.confidenceHigh / bootstrap.median - 1)).toFixed(2) + "%";
                    },
                    className: "left pad-left small"
                }
            ]
        },
        {
            title: Strings.text.complexity,
            children:
            [
                {
                    text: function(data) {
                        return data[Strings.json.controller][Strings.json.measurements.average].toFixed(2);
                    },
                    className: "average"
                },
                {
                    text: function(data) {
                        return [
                            "± ",
                            data[Strings.json.controller][Strings.json.measurements.percent].toFixed(2),
                            "%"
                        ].join("");
                    },
                    className: function(data) {
                        var className = "stdev";

                        if (data[Strings.json.controller][Strings.json.measurements.percent] >= 10)
                            className += " noisy-results";
                        return className;
                    }
                }
            ]
        },
        {
            title: Strings.text.frameRate,
            children:
            [
                {
                    text: function(data) {
                        return data[Strings.json.frameLength][Strings.json.measurements.average].toFixed(2);
                    },
                    className: function(data, options) {
                        var className = "average";
                        if (Math.abs(data[Strings.json.frameLength][Strings.json.measurements.average] - options["frame-rate"]) >= 2)
                            className += " noisy-results";
                        return className;
                    }
                },
                {
                    text: function(data) {
                        var frameRateData = data[Strings.json.frameLength];
                        return [
                            "± ",
                            frameRateData[Strings.json.measurements.percent].toFixed(2),
                            "%"
                        ].join("");
                    },
                    className: function(data) {
                        var className = "stdev";

                        if (data[Strings.json.frameLength][Strings.json.measurements.percent] >= 10)
                            className += " noisy-results";
                        return className;
                    }
                }
            ]
        },
        {
            title: Strings.text.mergedRawComplexity,
            children:
            [
                {
                    text: function(data) {
                        return data[Strings.json.complexity][Strings.json.complexity].toFixed(2);
                    },
                    className: "average"
                },
                {
                    text: function(data) {
                        return [
                            "± ",
                            data[Strings.json.complexity][Strings.json.measurements.stdev].toFixed(2),
                            "ms"
                        ].join("");
                    },
                    className: "stdev"
                }
            ]
        }
    ]
})

///////////
// Suites

Suites.push(new Suite("HTML suite",
    [
        {
            url: "bouncing-particles/bouncing-css-shapes.html?particleWidth=12&particleHeight=12&shape=circle",
            name: "CSS bouncing circles"
        },
        {
            url: "bouncing-particles/bouncing-css-shapes.html?particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "CSS bouncing clipped rects"
        },
        {
            url: "bouncing-particles/bouncing-css-shapes.html?particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "CSS bouncing gradient circles"
        },
        {
            url: "bouncing-particles/bouncing-css-shapes.html?particleWidth=80&particleHeight=80&shape=circle&blend",
            name: "CSS bouncing blend circles"
        },
        {
            url: "bouncing-particles/bouncing-css-shapes.html?particleWidth=80&particleHeight=80&shape=circle&filter",
            name: "CSS bouncing filter circles"
        },
        {
            url: "bouncing-particles/bouncing-css-images.html?particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "CSS bouncing SVG images"
        },
        {
            url: "bouncing-particles/bouncing-tagged-images.html?particleWidth=100&particleHeight=100",
            name: "CSS bouncing tagged images"
        },
        {
            url: "dom/leaves.html",
            name: "Leaves 2.0"
        },
        {
            url: "dom/focus.html",
            name: "Focus 2.0"
        },
        {
            url: "dom/particles.html",
            name: "DOM particles, SVG masks"
        },
        {
            url: "dom/compositing-transforms.html?particleWidth=50&particleHeight=50&filters=yes&imageSrc=../resources/yin-yang.svg",
            name: "Composited Transforms"
        }
    ]
));

Suites.push(new Suite("Canvas suite",
    [
        {
            url: "bouncing-particles/bouncing-canvas-shapes.html?particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "canvas bouncing clipped rects"
        },
        {
            url: "bouncing-particles/bouncing-canvas-shapes.html?particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "canvas bouncing gradient circles"
        },
        {
            url: "bouncing-particles/bouncing-canvas-images.html?particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "canvas bouncing SVG images"
        },
        {
            url: "bouncing-particles/bouncing-canvas-images.html?particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "canvas bouncing PNG images"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=strokes",
            name: "Stroke shapes"
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=fills",
            name: "Fill shapes"
        },
        {
            url: "simple/tiled-canvas-image.html",
            name: "Canvas put/get image data"
        },
    ]
));

Suites.push(new Suite("SVG suite",
    [
        {
            url: "bouncing-particles/bouncing-svg-shapes.html?particleWidth=12&particleHeight=12&shape=circle",
            name: "SVG bouncing circles",
        },
        {
            url: "bouncing-particles/bouncing-svg-shapes.html?particleWidth=40&particleHeight=40&shape=rect&clip=star",
            name: "SVG bouncing clipped rects",
        },
        {
            url: "bouncing-particles/bouncing-svg-shapes.html?particleWidth=50&particleHeight=50&shape=circle&fill=gradient",
            name: "SVG bouncing gradient circles"
        },
        {
            url: "bouncing-particles/bouncing-svg-images.html?particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.svg",
            name: "SVG bouncing SVG images"
        },
        {
            url: "bouncing-particles/bouncing-svg-images.html?particleWidth=80&particleHeight=80&imageSrc=../resources/yin-yang.png",
            name: "SVG bouncing PNG images"
        },
    ]
));

Suites.push(new Suite("3D Graphics",
    [
        {
            url: "3d/webgl.html",
            name: "WebGL"
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
            url: "simple/simple-canvas-paths.html?pathType=ellipse",
            name: "Canvas ellipses"
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
        },
        {
            url: "simple/simple-canvas-paths.html?pathType=ellipseFill",
            name: "Canvas ellipses, fill"
        }
    ]
));

// Draws with a CanvasPattern or CanvasGradient style, changes the style object with
// pattern.setTransform() or gradient.addColorStop(), and draws again. fillStyle and strokeStyle
// hold the style object itself, so the second draw must use the changed object.

description("Changing a CanvasPattern or CanvasGradient after it is assigned to fillStyle or strokeStyle affects the following draws.");
window.jsTestIsAsync = true;

const width = 100;
const height = 50;
const font = "50px Ahem";

const red = [255, 0, 0, 255];
const lime = [0, 255, 0, 255];
const blue = [0, 0, 255, 255];

function inverted(color)
{
    return [255 - color[0], 255 - color[1], 255 - color[2], color[3]];
}

function createCanvas(canvasType)
{
    if (canvasType == "OffscreenCanvas")
        return new OffscreenCanvas(width, height);
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    return canvas;
}

// Canvas filters need the canvas element to be rendered.
function createAttachedCanvas(canvasType)
{
    const canvas = createCanvas(canvasType);
    if (canvasType == "HTMLCanvasElement")
        document.body.appendChild(canvas);
    return canvas;
}

function createContext(canvasType, filter)
{
    const context = createAttachedCanvas(canvasType).getContext("2d");
    context.lineWidth = 20;
    context.font = font;
    context.textBaseline = "top";
    context.filter = filter;
    return context;
}

// The left half of the tile is red, the right half lime. The pattern repeats, so translating it by
// half the tile width swaps the colors of the two halves of the canvas.
function createPatternStyle(context, canvasType)
{
    const tile = createCanvas(canvasType);
    const tileContext = tile.getContext("2d");
    tileContext.fillStyle = "red";
    tileContext.fillRect(0, 0, width / 2, height);
    tileContext.fillStyle = "lime";
    tileContext.fillRect(width / 2, 0, width / 2, height);
    const pattern = context.createPattern(tile, "repeat");
    return {
        style: pattern,
        change: () => pattern.setTransform(new DOMMatrix().translate(width / 2, 0)),
        colorsBefore: [red, lime],
        colorsAfter: [lime, red],
        changeName: "setTransform()",
    };
}

// The gradient is red until the two stops are added. Then the right half is lime.
function createGradientStyle(context)
{
    const gradient = context.createLinearGradient(0, 0, width, 0);
    gradient.addColorStop(0, "red");
    gradient.addColorStop(0.5, "red");
    return {
        style: gradient,
        change: () => {
            gradient.addColorStop(0.5, "lime");
            gradient.addColorStop(1, "lime");
        },
        colorsBefore: [red, red],
        colorsAfter: [red, lime],
        changeName: "addColorStop()",
    };
}

const fillOperations = {
    "fillRect()": context => context.fillRect(0, 0, width, height),
    "fill()": context => {
        context.beginPath();
        context.rect(0, 0, width, height);
        context.fill();
    },
    "fill(Path2D)": context => {
        const path = new Path2D();
        path.rect(0, 0, width, height);
        context.fill(path);
    },
    "fillText()": context => context.fillText("XX", 0, 0),
};

// The strokes cover the top edge of the canvas.
const strokeOperations = {
    "strokeRect()": context => context.strokeRect(0, 0, width, height),
    "stroke()": context => {
        context.beginPath();
        context.rect(0, 0, width, height);
        context.stroke();
    },
    "stroke(Path2D)": context => {
        const path = new Path2D();
        path.rect(0, 0, width, height);
        context.stroke(path);
    },
    "strokeText()": context => context.strokeText("XX", 0, 0),
};

function draw(context, operation)
{
    context.clearRect(0, 0, width, height);
    operation(context);
}

function colorsMatch(actual, expected)
{
    return actual.every((value, i) => Math.abs(value - expected[i]) <= 3);
}

// The name of the color if it is one of the colors of the test, so that the messages don't depend on
// the antialiasing and gradient interpolation of the platform.
function colorName(color)
{
    const colors = { red, lime, blue, cyan: inverted(red), magenta: inverted(lime), yellow: inverted(blue), transparent: [0, 0, 0, 0] };
    for (const [name, value] of Object.entries(colors)) {
        if (colorsMatch(color, value))
            return name;
    }
    return color.join();
}

// Throws if the pixels in the left and the right half don't match.
function checkColors(context, y, expectedColors, filter, when)
{
    for (const [i, x] of [width / 4, width * 3 / 4].entries()) {
        const actual = Array.from(context.getImageData(x, y, 1, 1).data);
        const expected = filter == "none" ? expectedColors[i] : inverted(expectedColors[i]);
        if (!colorsMatch(actual, expected))
            throw `${when}, pixel ${x},${y} is ${colorName(actual)}, expected ${colorName(expected)}`;
    }
}

// Each case gets t with:
// - contexts: two contexts with the filter of the case,
// - style: the style object, with change(), colorsBefore and colorsAfter,
// - createOtherStyle(): another style object of the same type,
// - setStyle(context, style), draw(context),
// - check(context, colors, when, filter): checks with the filter of the case, unless given.
const changes = {
    "before the first draw": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
    },
    "after a draw": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
    },
    "after a draw, then drawn twice": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, second draw");
    },
    "after a draw, then drawn without the filter and with it": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.style.change();
        const filter = t.contexts[0].filter;
        t.contexts[0].filter = "none";
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, without the filter", "none");
        t.contexts[0].filter = filter;
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, with the filter");
    },
    "after a draw, then drawn with the filter and without it": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, with the filter");
        t.contexts[0].filter = "none";
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, without the filter", "none");
    },
    "after a draw, then assigned again": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.style.change();
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
    },
    "after another style object of the same type changed": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.createOtherStyle().change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "after the other object changed");
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
    },
    "between save() and restore()": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.contexts[0].save();
        t.style.change();
        t.contexts[0].restore();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
    },
    "between save() and restore(), with a draw before restore()": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.contexts[0].save();
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, before restore()");
        t.contexts[0].restore();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, after restore()");
    },
    "between save() and restore(), with a state change and a draw before restore()": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.contexts[0].save();
        t.contexts[0].lineDashOffset = 1;
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, before restore()");
        t.contexts[0].restore();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, after restore()");
    },
    "between save() and restore(), while another style is set": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.contexts[0].save();
        t.setStyle(t.contexts[0], "blue");
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], [blue, blue], "with the other style");
        t.contexts[0].restore();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change");
    },
    "between beginLayer() and endLayer()": t => {
        t.setStyle(t.contexts[0], t.style.style);
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsBefore, "before the change");
        t.contexts[0].beginLayer();
        t.style.change();
        t.draw(t.contexts[0]);
        t.contexts[0].endLayer();
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, drawn in the layer");
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the change, drawn after endLayer()");
    },
    "after a draw, with the style used by two contexts": t => {
        for (const [i, context] of t.contexts.entries()) {
            t.setStyle(context, t.style.style);
            t.draw(context);
            t.check(context, t.style.colorsBefore, `before the change, context ${i + 1}`);
        }
        t.style.change();
        for (const [i, context] of t.contexts.entries()) {
            t.draw(context);
            t.check(context, t.style.colorsAfter, `after the change, context ${i + 1}`);
        }
    },
    "twice, with the style used by two contexts and one context drawing in between": t => {
        for (const [i, context] of t.contexts.entries()) {
            t.setStyle(context, t.style.style);
            t.draw(context);
            t.check(context, t.style.colorsBefore, `before the change, context ${i + 1}`);
        }
        t.style.change();
        t.draw(t.contexts[0]);
        t.check(t.contexts[0], t.style.colorsAfter, "after the first change, context 1");
        // Changing the object the same way again draws the same.
        t.style.change();
        for (const [i, context] of t.contexts.entries()) {
            t.draw(context);
            t.check(context, t.style.colorsAfter, `after the second change, context ${i + 1}`);
        }
    },
};

const targets = {
    "fillStyle": {
        setStyle: (context, style) => { context.fillStyle = style; },
        operations: fillOperations,
        y: height / 2,
    },
    "strokeStyle": {
        setStyle: (context, style) => { context.strokeStyle = style; },
        operations: strokeOperations,
        y: 5,
    },
};

function runCase(canvasType, filter, styleType, targetName, operationName, changeName)
{
    const target = targets[targetName];
    const contexts = [createContext(canvasType, filter), createContext(canvasType, filter)];
    const createStyle = () => styleType == "CanvasPattern" ? createPatternStyle(contexts[0], canvasType) : createGradientStyle(contexts[0]);
    const styleInfo = createStyle();
    const operation = target.operations[operationName];
    let failure = null;
    try {
        changes[changeName]({
            contexts,
            style: styleInfo,
            createOtherStyle: createStyle,
            setStyle: target.setStyle,
            draw: context => draw(context, operation),
            check: (context, colors, when, checkFilter = filter) => checkColors(context, target.y, colors, checkFilter, when),
        });
    } catch (e) {
        failure = String(e);
    }

    for (const context of contexts) {
        if (context.canvas instanceof HTMLCanvasElement)
            context.canvas.remove();
    }

    const name = `${canvasType}, filter ${filter}: ${styleType} ${targetName}, ${styleInfo.changeName} ${changeName}, ${operationName}`;
    if (failure)
        testFailed(`${name}: ${failure}`);
    else
        testPassed(name);
}

async function runTests()
{
    await document.fonts.load(font);
    for (const canvasType of ["HTMLCanvasElement", "OffscreenCanvas"]) {
        for (const filter of ["none", "invert(100%)"]) {
            for (const styleType of ["CanvasPattern", "CanvasGradient"]) {
                for (const targetName of Object.keys(targets)) {
                    for (const operationName of Object.keys(targets[targetName].operations)) {
                        for (const changeName of Object.keys(changes))
                            runCase(canvasType, filter, styleType, targetName, operationName, changeName);
                    }
                }
            }
        }
    }
    finishJSTest();
}

runTests();

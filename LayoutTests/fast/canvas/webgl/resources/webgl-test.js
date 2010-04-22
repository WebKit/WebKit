if (window.layoutTestController)
    layoutTestController.overridePreference("WebKitWebGLEnabled", "1");

function getShaderSource(file) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", file, false);
    xhr.send();
    return xhr.responseText;
}

function create3DContext(attrs) {
    var canvas = document.createElement("canvas");
    try {
        return canvas.getContext("experimental-webgl", attrs);
    } catch(e) {}
    return canvas.getContext("moz-webgl", attrs);
}

function createGLErrorWrapper(context, fname) {
    return function() {
        var rv = context[fname].apply(context, arguments);
        var err = context.getError();
        if (err != 0)
            throw "GL error " + err + " in " + fname;
        return rv;
    };
}

function create3DDebugContext(attrs) {
    var context = create3DContext(attrs);
    // Thanks to Ilmari Heikkinen for the idea on how to implement this so elegantly.
    var wrap = {};
    for (var i in context) {
        try {
            if (typeof context[i] == 'function') {
                wrap[i] = createGLErrorWrapper(context, i);
            } else {
                wrap[i] = context[i];
            }
        } catch (e) {
            // console.log("create3DDebugContext: Error accessing " + i);
        }
    }
    wrap.getError = function() {
        return context.getError();
    };
    return wrap;
}

function loadStandardProgram(context) {
    var program = context.createProgram();
    context.attachShader(program, loadStandardVertexShader(context));
    context.attachShader(program, loadStandardFragmentShader(context));
    context.linkProgram(program);
    return program;
}

function loadProgram(context, vertexShaderPath, fragmentShaderPath) {
    var program = context.createProgram();
    context.attachShader(program, loadVertexShader(context, vertexShaderPath));
    context.attachShader(program, loadFragmentShader(context, fragmentShaderPath));
    context.linkProgram(program);
    return program;
}

function loadStandardVertexShader(context) {
    return loadVertexShader(context, "resources/vertexShader.vert");
}

function loadVertexShader(context, path) {
    var vertexShader = context.createShader(context.VERTEX_SHADER);
    context.shaderSource(vertexShader, getShaderSource(path));
    context.compileShader(vertexShader);
    return vertexShader;
}

function loadStandardFragmentShader(context) {
    return loadFragmentShader(context, "resources/fragmentShader.frag");
}

function loadFragmentShader(context, path) {
    var fragmentShader = context.createShader(context.FRAGMENT_SHADER);
    context.shaderSource(fragmentShader, getShaderSource(path));
    context.compileShader(fragmentShader);
    return fragmentShader;
}

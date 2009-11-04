if (window.layoutTestController)
    layoutTestController.overridePreference("WebKitWebGLEnabled", "1");

function getShaderSource(file) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", file, false);
    xhr.send();
    return xhr.responseText;
}

function create3DContext() {
    var canvas = document.createElement("canvas");
    try {
        return canvas.getContext("webkit-3d");
    } catch(e) {}
    return canvas.getContext("moz-webgl");
}

function loadStandardProgram(context) {
    var program = context.createProgram();
    context.attachShader(program, loadStandardVertexShader(context));
    context.attachShader(program, loadStandardFragmentShader(context));
    context.linkProgram(program);
    return program;
}

function loadStandardVertexShader(context) {
    var vertexShader = context.createShader(context.VERTEX_SHADER);
    context.shaderSource(vertexShader, getShaderSource("resources/vertexShader.vert"));
    context.compileShader(vertexShader);
    return vertexShader;
}

function loadStandardFragmentShader(context) {
    var fragmentShader = context.createShader(context.FRAGMENT_SHADER);
    context.shaderSource(fragmentShader, getShaderSource("resources/fragmentShader.frag"));
    context.compileShader(fragmentShader);
    return fragmentShader;
}

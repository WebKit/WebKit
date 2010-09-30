// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

WebGLTestUtils = (function() {

/**
 * Wrapped logging function.
 * @param {string} msg The message to log.
 */
var log = function(msg) {
  if (window.console && window.console.log) {
    window.console.log(msg);
  }
};

/**
 * Wrapped logging function.
 * @param {string} msg The message to log.
 */
var error = function(msg) {
  if (window.console) {
    if (window.console.error) {
      window.console.error(msg);
    }
    else if (window.console.log) {
      window.console.log(msg);
    }
  }
};

/**
 * Turn off all logging.
 */
var loggingOff = function() {
  log = function() {};
  error = function() {};
};

/**
 * Converts a WebGL enum to a string
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} value The enum value.
 * @return {string} The enum as a string.
 */
var glEnumToString = function(gl, value) {
  for (var p in gl) {
    if (gl[p] == value) {
      return p;
    }
  }
  return "0x" + value.toString(16);
};

var lastError = "";

/**
 * Returns the last compiler/linker error.
 * @return {string} The last compiler/linker error.
 */
var getLastError = function() {
  return lastError;
};

/**
 * Whether a haystack ends with a needle.
 * @param {string} haystack String to search
 * @param {string} needle String to search for.
 * @param {boolean} True if haystack ends with needle.
 */
var endsWith = function(haystack, needle) {
  return haystack.substr(haystack.length - needle.length) === needle;
};

/**
 * Whether a haystack starts with a needle.
 * @param {string} haystack String to search
 * @param {string} needle String to search for.
 * @param {boolean} True if haystack starts with needle.
 */
var startsWith = function(haystack, needle) {
  return haystack.substr(0, needle.length) === needle;
};

/**
 * A vertex shader for a single texture.
 * @type {string}
 */
var simpleTextureVertexShader = '' +
  'attribute vec4 vPosition;\n' +
  'attribute vec2 texCoord0;\n' +
  'varying vec2 texCoord;\n' +
  'void main() {\n' +
  '    gl_Position = vPosition;\n' +
  '    texCoord = texCoord0;\n' +
  '}\n';

/**
 * A fragment shader for a single texture.
 * @type {string}
 */
var simpleTextureFragmentShader = '' +
  '#ifdef GL_ES\n' +
  'precision mediump float;\n' +
  '#endif\n' +
  'uniform sampler2D tex;\n' +
  'varying vec2 texCoord;\n' +
  'void main() {\n' +
  '    gl_FragColor = texture2D(tex, texCoord);\n' +
  '}\n';

/**
 * Creates a simple texture vertex shader.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @return {!WebGLShader}
 */
var setupSimpleTextureVertexShader = function(gl) {
    return loadShader(gl, simpleTextureVertexShader, gl.VERTEX_SHADER);
};

/**
 * Creates a simple texture fragment shader.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @return {!WebGLShader}
 */
var setupSimpleTextureFragmentShader = function(gl) {
    return loadShader(
        gl, simpleTextureFragmentShader, gl.FRAGMENT_SHADER);
};

/**
 * Creates a program, attaches shaders, binds attrib locations, links the
 * program and calls useProgram.
 * @param {!Array.<!WebGLShader>} shaders The shaders to attach .
 * @param {!Array.<string>} opt_attribs The attribs names.
 * @param {!Array.<number>} opt_locations The locations for the attribs.
 */
var setupProgram = function(gl, shaders, opt_attribs, opt_locations) {
  var program = gl.createProgram();
  for (var ii = 0; ii < shaders.length; ++ii) {
    gl.attachShader(program, shaders[ii]);
  }
  if (opt_attribs) {
    for (var ii = 0; ii < opt_attribs.length; ++ii) {
      gl.bindAttribLocation(
          program,
          opt_locations ? opt_locations[ii] : ii,
          opt_attribs[ii]);
    }
  }
  gl.linkProgram(program);

  // Check the link status
  var linked = gl.getProgramParameter(program, gl.LINK_STATUS);
  if (!linked) {
      // something went wrong with the link
      lastError = gl.getProgramInfoLog (program);
      error("Error in program linking:" + lastError);

      gl.deleteProgram(program);
      return null;
  }

  gl.useProgram(program);
  return program;
};

/**
 * Creates a simple texture program.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @param {number} opt_texcoordLocation The attrib location for texture coords.
 * @return {WebGLProgram}
 */
var setupSimpleTextureProgram = function(
    gl, opt_positionLocation, opt_texcoordLocation) {
  opt_positionLocation = opt_positionLocation || 0;
  opt_texcoordLocation = opt_texcoordLocation || 1;
  var vs = setupSimpleTextureVertexShader(gl);
  var fs = setupSimpleTextureFragmentShader(gl);
  if (!vs || !fs) {
    return null;
  }
  var program = setupProgram(
      gl,
      [vs, fs],
      ['vPosition', 'texCoord0'],
      [opt_positionLocation, opt_texcoordLocation]);
  if (!program) {
    gl.deleteShader(fs);
    gl.deleteShader(vs);
  }
  gl.useProgram(program);
  return program;
};

/**
 * Creates buffers for a textured unit quad and attaches them to vertex attribs.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @param {number} opt_texcoordLocation The attrib location for texture coords.
 * @return {!Array.<WebGLBuffer>} The buffer objects that were
 *      created.
 */
var setupUnitQuad = function(gl, opt_positionLocation, opt_texcoordLocation) {
  opt_positionLocation = opt_positionLocation || 0;
  opt_texcoordLocation = opt_texcoordLocation || 1;
  var objects = [];

  var vertexObject = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
       1.0,  1.0, 0.0,
      -1.0,  1.0, 0.0,
      -1.0, -1.0, 0.0,
       1.0,  1.0, 0.0,
      -1.0, -1.0, 0.0,
       1.0, -1.0, 0.0]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(opt_positionLocation);
  gl.vertexAttribPointer(opt_positionLocation, 3, gl.FLOAT, false, 0, 0);
  objects.push(vertexObject);

  var vertexObject = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
      1.0, 1.0,
      0.0, 1.0,
      0.0, 0.0,
      1.0, 1.0,
      0.0, 0.0,
      1.0, 0.0]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(opt_texcoordLocation);
  gl.vertexAttribPointer(opt_texcoordLocation, 2, gl.FLOAT, false, 0, 0);
  objects.push(vertexObject);
  return objects;
};

/**
 * Creates a program and buffers for rendering a textured quad.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @param {number} opt_texcoordLocation The attrib location for texture coords.
 * @return {!WebGLProgram}
 */
var setupTexturedQuad = function(
    gl, opt_positionLocation, opt_texcoordLocation) {
  var program = setupSimpleTextureProgram(
      gl, opt_positionLocation, opt_texcoordLocation);
  setupUnitQuad(gl, opt_positionLocation, opt_texcoordLocation);
  return program;
};

/**
 * Fills the given texture with a solid color
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!WebGLTexture} tex The texture to fill.
 * @param {number} width The width of the texture to create.
 * @param {number} height The height of the texture to create.
 * @param {!Array.<number>} color The color to fill with. A 4 element array
 *        where each element is in the range 0 to 255.
 * @param {number} opt_level The level of the texture to fill. Default = 0.
 */
var fillTexture = function(gl, tex, width, height, color, opt_level) {
  opt_level = opt_level || 0;
  var canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  var ctx2d = canvas.getContext('2d');
  ctx2d.fillStyle = "rgba(" + color[0] + "," + color[1] + "," + color[2] + "," + color[3] + ")";
  ctx2d.fillRect(0, 0, width, height);
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(
      gl.TEXTURE_2D, opt_level, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, canvas);
};

/**
 * Creates a textures and fills it with a solid color
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} width The width of the texture to create.
 * @param {number} height The height of the texture to create.
 * @param {!Array.<number>} color The color to fill with. A 4 element array
 *        where each element is in the range 0 to 255.
 * @return {!WebGLTexture}
 */
var createColoredTexture = function(gl, width, height, color) {
  var tex = gl.createTexture();
  fillTexture(gl, text, width, height, color);
  return tex;
};

/**
 * Draws a previously setup quad.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!Array.<number>} opt_color The color to fill clear with before
 *        drawing. A 4 element array where each element is in the range 0 to
 *        255. Default [255, 255, 255, 255]
 */
var drawQuad = function(gl, opt_color) {
  opt_color = opt_color || [255, 255, 255, 255];
  gl.clearColor(
      opt_color[0] / 255,
      opt_color[1] / 255,
      opt_color[2] / 255,
      opt_color[3] / 255);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  gl.drawArrays(gl.TRIANGLES, 0, 6);
};

/**
 * Checks that a portion of a canvas is 1 color.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} x left corner of region to check.
 * @param {number} y bottom corner of region to check.
 * @param {number} width width of region to check.
 * @param {number} height width of region to check.
 * @param {!Array.<number>} color The color to fill clear with before drawing. A
 *        4 element array where each element is in the range 0 to 255.
 * @param {string} msg Message to associate with success. Eg ("should be red").
 */
var checkCanvasRect = function(gl, x, y, width, height, color, msg) {
  var buf = new Uint8Array(width * height * 4);
  gl.readPixels(x, y, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
  for (var i = 0; i < width * height; ++i) {
    var offset = i * 4;
    for (var j = 0; j < color.length; ++j) {
      if (buf[offset + j] != color[j]) {
        testFailed(msg);
        var was = buf[offset + 0].toString();
        for (j = 1; j < color.length; ++j) {
          was += "," + buf[offset + j];
        }
        debug('expected: ' + color + ' was ' + was);
        return;
      }
    }
  }
  testPassed(msg);
};

/**
 * Checks that an entire canvas is 1 color.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!Array.<number>} color The color to fill clear with before drawing. A
 *        4 element array where each element is in the range 0 to 255.
 * @param {string} msg Message to associate with success. Eg ("should be red").
 */
var checkCanvas = function(gl, color, msg) {
  checkCanvasRect(gl, 0, 0, gl.canvas.width, gl.canvas.height, color, msg);
};

/**
 * Loads a texture, calls callback when finished.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} url URL of image to load
 * @param {function(!Image): void} callback Function that gets called after
 *        image has loaded
 * @return {!WebGLTexture} The created texture.
 */
var loadTexture = function(gl, url, callback) {
    var texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    var image = new Image();
    image.onload = function() {
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
        callback(image);
    };
    image.src = url;
    return texture;
};

/**
 * Creates a webgl context.
 * @param {!Canvas} opt_canvas The canvas tag to get context from. If one is not
 *     passed in one will be created.
 * @return {!WebGLContext} The created context.
 */
var create3DContext = function(opt_canvas) {
  opt_canvas = opt_canvas || document.createElement("canvas");
  var context = null;
  try {
    context = opt_canvas.getContext("experimental-webgl");
  } catch(e) {}
  if (!context) {
    try {
      context = opt_canvas.getContext("webkit-3d");
    } catch(e) {}
  }
  if (!context) {
    try {
      context = opt_canvas.getContext("moz-webgl");
    } catch(e) {}
  }
  if (!context) {
    testFailed("Unable to fetch WebGL rendering context for Canvas");
  }
  return context;
}

/**
 * Gets a GLError value as a string.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} err The webgl error as retrieved from gl.getError().
 * @return {string} the error as a string.
 */
var getGLErrorAsString = function(gl, err) {
  if (err === gl.NO_ERROR) {
    return "NO_ERROR";
  }
  for (var name in gl) {
    if (gl[name] === err) {
      return name;
    }
  }
  return err.toString();
};

/**
 * Wraps a WebGL function with a function that throws an exception if there is
 * an error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} fname Name of function to wrap.
 * @return {function} The wrapped function.
 */
var createGLErrorWrapper = function(context, fname) {
  return function() {
    var rv = context[fname].apply(context, arguments);
    var err = context.getError();
    if (err != 0)
      throw "GL error " + getGLErrorAsString(err) + " in " + fname;
    return rv;
  };
};

/**
 * Creates a WebGL context where all functions are wrapped to throw an exception
 * if there is an error.
 * @param {!Canvas} canvas The HTML canvas to get a context from.
 * @return {!Object} The wrapped context.
 */
function create3DContextWithWrapperThatThrowsOnGLError(canvas) {
  var context = create3DContext(canvas);
  var wrap = {};
  for (var i in context) {
    try {
      if (typeof context[i] == 'function') {
        wrap[i] = createGLErrorWrapper(context, i);
      } else {
        wrap[i] = context[i];
      }
    } catch (e) {
      error("createContextWrapperThatThrowsOnGLError: Error accessing " + i);
    }
  }
  wrap.getError = function() {
      return context.getError();
  };
  return wrap;
};

/**
 * Tests that an evaluated expression generates a specific GL error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} glError The expected gl error.
 * @param {string} evalSTr The string to evaluate.
 */
var shouldGenerateGLError = function(gl, glError, evalStr) {
  var exception;
  try {
    eval(evalStr);
  } catch (e) {
    exception = e;
  }
  if (exception) {
    testFailed(evalStr + " threw exception " + exception);
  } else {
    var err = gl.getError();
    if (err != glError) {
      testFailed(evalStr + " expected: " + getGLErrorAsString(gl, glError) + ". Was " + getGLErrorAsString(gl, err) + ".");
    } else {
      testPassed(evalStr + " was expected value: " + getGLErrorAsString(gl, glError) + ".");
    }
  }
};

/**
 * Tests that the first error GL returns is the specified error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} glError The expected gl error.
 * @param {string} opt_msg
 */
var glErrorShouldBe = function(gl, glError, opt_msg) {
  opt_msg = opt_msg || "";
  var err = gl.getError();
  if (err != glError) {
    testFailed("getError expected: " + getGLErrorAsString(gl, glError) +
               ". Was " + getGLErrorAsString(gl, err) + " : " + opt_msg);
  } else {
    testPassed("getError was expected value: " +
                getGLErrorAsString(gl, glError) + " : " + opt_msg);
  }
};

/**
 * Links a WebGL program, throws if there are errors.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!WebGLProgram} program The WebGLProgram to link.
 */
var linkProgram = function(gl, program) {
  // Link the program
  gl.linkProgram(program);

  // Check the link status
  var linked = gl.getProgramParameter(program, gl.LINK_STATUS);
  if (!linked) {
    // something went wrong with the link
    var error = gl.getProgramInfoLog (program);

    testFailed("Error in program linking:" + error);

    gl.deleteProgram(program);
    gl.deleteProgram(fragmentShader);
    gl.deleteProgram(vertexShader);
  }
};

/**
 * Sets up WebGL with shaders.
 * @param {string} canvasName The id of the canvas.
 * @param {string} vshader The id of the script tag that contains the vertex
 *     shader source.
 * @param {string} fshader The id of the script tag that contains the fragment
 *     shader source.
 * @param {!Array.<string>} attribs An array of attrib names used to bind
 *     attribs to the ordinal of the name in this array.
 * @param {!Array.<number>} opt_clearColor The color to cla
 * @return {!WebGLContext} The created WebGLContext.
 */
var setupWebGLWithShaders = function(
   canvasName, vshader, fshader, attribs) {
  var canvas = document.getElementById(canvasName);
  var gl = create3DContext(canvas);
  if (!gl) {
    testFailed("No WebGL context found");
  }

  // create our shaders
  var vertexShader = loadShaderFromScript(gl, vshader);
  var fragmentShader = loadShaderFromScript(gl, fshader);

  if (!vertexShader || !fragmentShader) {
    return null;
  }

  // Create the program object
  program = gl.createProgram();

  if (!program) {
    return null;
  }

  // Attach our two shaders to the program
  gl.attachShader (program, vertexShader);
  gl.attachShader (program, fragmentShader);

  // Bind attributes
  for (var i in attribs) {
    gl.bindAttribLocation (program, i, attribs[i]);
  }

  linkProgram(gl, program);

  gl.useProgram(program);

  gl.clearColor(0,0,0,1);
  gl.clearDepth(1);

  gl.enable(gl.DEPTH_TEST);
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

  gl.program = program;
  return gl;
};

/**
 * Gets a file from a file/URL
 * @param {string} file the URL of the file to get.
 * @return {string} The contents of the file.
 */
var readFile = function(file) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", file, false);
  xhr.send();
  return xhr.responseText.replace(/\r/g, "");
};

/**
 * Gets a file from a URL and parses it for filenames. IF a file name ends
 * in .txt recursively reads that file and adds it to the list.
 */
var readFileList = function(url) {
  var files = [];
  if (url.substr(url.length - 4) == '.txt') {
    var lines = readFile(url).split('\n');
    var prefix = '';
    var lastSlash = url.lastIndexOf('/');
    if (lastSlash >= 0) {
      prefix = url.substr(0, lastSlash + 1);
    }
    for (var ii = 0; ii < lines.length; ++ii) {
      var str = lines[ii].replace(/^\s\s*/, '').replace(/\s\s*$/, '');
      if (str.length > 4 &&
          str[0] != '#' &&
          str[0] != ";" &&
          str.substr(0, 2) != "//") {
        new_url = prefix + str;
        files = files.concat(readFileList(new_url));
      }
    }
  } else {
    files.push(url);
  }
  return files;
};

/**
 * Loads a shader.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} shaderSource The shader source.
 * @param {number} shaderType The type of shader.
 * @return {!WebGLShader} The created shader.
 */
var loadShader = function(gl, shaderSource, shaderType) {
  // Create the shader object
  var shader = gl.createShader(shaderType);
  if (shader == null) {
    error("*** Error: unable to create shader '"+shaderSource+"'");
    return null;
  }

  // Load the shader source
  gl.shaderSource(shader, shaderSource);

  // Compile the shader
  gl.compileShader(shader);

  // Check the compile status
  var compiled = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
  if (!compiled) {
    // Something went wrong during compilation; get the error
    lastError = gl.getShaderInfoLog(shader);
    error("*** Error compiling shader '" + shader + "':" + lastError);
    gl.deleteShader(shader);
    return null;
  }

  return shader;
}

/**
 * Loads a shader from a URL.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {file} file The URL of the shader source.
 * @param {number} type The type of shader.
 * @return {!WebGLShader} The created shader.
 */
var loadShaderFromFile = function(gl, file, type) {
  var shaderSource = readFile(file);
  return loadShader(gl, shaderSource, type);
};

/**
 * Loads a shader from a script tag.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} scriptId The id of the script tag.
 * @param {number} opt_shaderType The type of shader. If not passed in it will
 *     be derived from the type of the script tag.
 * @return {!WebGLShader} The created shader.
 */
var loadShaderFromScript = function(gl, scriptId, opt_shaderType) {
  var shaderSource = "";
  var shaderType;
  var shaderScript = document.getElementById(scriptId);
  if (!shaderScript) {
    throw("*** Error: unknown script element" + scriptId);
  }
  shaderSource = shaderScript.text;

  if (!opt_shaderType) {
    if (shaderScript.type == "x-shader/x-vertex") {
      shaderType = gl.VERTEX_SHADER;
    } else if (shaderScript.type == "x-shader/x-fragment") {
      shaderType = gl.FRAGMENT_SHADER;
    } else if (shaderType != gl.VERTEX_SHADER && shaderType != gl.FRAGMENT_SHADER) {
      throw("*** Error: unknown shader type");
      return null;
    }
  }

  return loadShader(
      gl, shaderSource, opt_shaderType ? opt_shaderType : shaderType);
};

var loadStandardProgram = function(gl) {
  var program = gl.createProgram();
  gl.attachShader(program, loadStandardVertexShader(gl));
  gl.attachShader(program, loadStandardFragmentShader(gl));
  linkProgram(gl, program);
  return program;
};

/**
 * Loads shaders from files, creates a program, attaches the shaders and links.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} vertexShaderPath The URL of the vertex shader.
 * @param {string} fragmentShaderPath The URL of the fragment shader.
 * @return {!WebGLProgram} The created program.
 */
var loadProgramFromFile = function(gl, vertexShaderPath, fragmentShaderPath) {
  var program = gl.createProgram();
  gl.attachShader(
      program,
      loadShaderFromFile(gl, vertexShaderPath, gl.VERTEX_SHADER));
  gl.attachShader(
      program,
      loadShaderFromFile(gl, fragmentShaderPath, gl.FRAGMENT_SHADER));
  linkProgram(gl, program);
  return program;
};

/**
 * Loads shaders from script tags, creates a program, attaches the shaders and
 * links.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} vertexScriptId The id of the script tag that contains the
 *        vertex shader.
 * @param {string} fragmentScriptId The id of the script tag that contains the
 *        fragment shader.
 * @return {!WebGLProgram} The created program.
 */
var loadProgramFromScript = function loadProgramFromScript(
  gl, vertexScriptId, fragmentScriptId) {
  var program = gl.createProgram();
  gl.attachShader(
      program,
      loadShaderFromScript(gl, vertexScriptId, gl.VERTEX_SHADER));
  gl.attachShader(
      program,
      loadShaderFromScript(gl, fragmentScriptId,  gl.FRAGMENT_SHADER));
  linkProgram(gl, program);
  return program;
};

/**
 * Loads shaders from script tags, creates a program, attaches the shaders and
 * links.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} vertexShader The vertex shader.
 * @param {string} fragmentShader The fragment shader.
 * @return {!WebGLProgram} The created program.
 */
var loadProgram = function(gl, vertexShader, fragmentShader) {
  var program = gl.createProgram();
  gl.attachShader(
      program,
      loadShader(gl, vertexShader, gl.VERTEX_SHADER));
  gl.attachShader(
      program,
      loadShader(gl, fragmentShader,  gl.FRAGMENT_SHADER));
  linkProgram(gl, program);
  return program;
};

var loadStandardVertexShader = function(gl) {
  return loadShaderFromFile(
      gl, "resources/vertexShader.vert", gl.VERTEX_SHADER);
};

var loadStandardFragmentShader = function(gl) {
  return loadShaderFromFile(
      gl, "resources/fragmentShader.frag", gl.FRAGMENT_SHADER);
};

/**
 * Loads an image asynchronously.
 * @param {string} url URL of image to load.
 * @param {!function(!Element): void} callback Function to call
 *     with loaded image.
 */
var loadImageAsync = function(url, callback) {
  var img = document.createElement('img');
  img.onload = function() {
    callback(img);
  };
  img.src = url;
};

/**
 * Loads an array of images.
 * @param {!Array.<string>} urls URLs of images to load.
 * @param {!function(!{string, img}): void} callback. Callback
 *     that gets passed map of urls to img tags.
 */
var loadImagesAsync = function(urls, callback) {
  var count = 1;
  var images = { };
  function countDown() {
    --count;
    if (count == 0) {
      callback(images);
    }
  }
  function imageLoaded(url) {
    return function(img) {
      images[url] = img;
      countDown();
    }
  }
  for (var ii = 0; ii < urls.length; ++ii) {
    ++count;
    loadImageAsync(urls[ii], imageLoaded(urls[ii]));
  }
  countDown();
};

return {
  create3DContext: create3DContext,
  create3DContextWithWrapperThatThrowsOnGLError:
    create3DContextWithWrapperThatThrowsOnGLError,
  checkCanvas: checkCanvas,
  checkCanvasRect: checkCanvasRect,
  createColoredTexture: createColoredTexture,
  drawQuad: drawQuad,
  endsWith: endsWith,
  getLastError: getLastError,
  glEnumToString: glEnumToString,
  glErrorShouldBe: glErrorShouldBe,
  fillTexture: fillTexture,
  loadImageAsync: loadImageAsync,
  loadImagesAsync: loadImagesAsync,
  loadProgram: loadProgram,
  loadProgramFromFile: loadProgramFromFile,
  loadProgramFromScript: loadProgramFromScript,
  loadShader: loadShader,
  loadShaderFromFile: loadShaderFromFile,
  loadShaderFromScript: loadShaderFromScript,
  loadStandardProgram: loadStandardProgram,
  loadStandardVertexShader: loadStandardVertexShader,
  loadStandardFragmentShader: loadStandardFragmentShader,
  loadTexture: loadTexture,
  log: log,
  loggingOff: loggingOff,
  error: error,
  setupProgram: setupProgram,
  setupSimpleTextureFragmentShader: setupSimpleTextureFragmentShader,
  setupSimpleTextureProgram: setupSimpleTextureProgram,
  setupSimpleTextureVertexShader: setupSimpleTextureVertexShader,
  setupTexturedQuad: setupTexturedQuad,
  setupUnitQuad: setupUnitQuad,
  setupWebGLWithShaders: setupWebGLWithShaders,
  startsWith: startsWith,
  shouldGenerateGLError: shouldGenerateGLError,
  readFile: readFile,
  readFileList: readFileList,

  none: false
};

}());



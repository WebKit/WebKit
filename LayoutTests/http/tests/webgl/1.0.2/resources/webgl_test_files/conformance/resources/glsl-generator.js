/*
** Copyright (c) 2012 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/
GLSLGenerator = (function() {

var vertexShaderTemplate = [
  "attribute vec4 aPosition;",
  "",
  "varying vec4 vColor;",
  "",
  "$(extra)",
  "$(emu)",
  "",
  "void main()",
  "{",
  "   gl_Position = aPosition;",
  "   vec2 texcoord = vec2(aPosition.xy * 0.5 + vec2(0.5, 0.5));",
  "   vec4 color = vec4(",
  "       texcoord,",
  "       texcoord.x * texcoord.y,",
  "       (1.0 - texcoord.x) * texcoord.y * 0.5 + 0.5);",
  "   $(test)",
  "}"
].join("\n");

var fragmentShaderTemplate = [
  "#if defined(GL_ES)",
  "precision mediump float;",
  "#endif",
  "",
  "varying vec4 vColor;",
  "",
  "$(extra)",
  "$(emu)",
  "",
  "void main()",
  "{",
  "   $(test)",
  "}"
].join("\n");

var baseVertexShader = [
  "attribute vec4 aPosition;",
  "",
  "varying vec4 vColor;",
  "",
  "void main()",
  "{",
  "   gl_Position = aPosition;",
  "   vec2 texcoord = vec2(aPosition.xy * 0.5 + vec2(0.5, 0.5));",
  "   vColor = vec4(",
  "       texcoord,",
  "       texcoord.x * texcoord.y,",
  "       (1.0 - texcoord.x) * texcoord.y * 0.5 + 0.5);",
  "}"
].join("\n");

var baseFragmentShader = [
  "#if defined(GL_ES)",
  "precision mediump float;",
  "#endif",
  "varying vec4 vColor;",
  "",
  "void main()",
  "{",
  "   gl_FragColor = vColor;",
  "}"
].join("\n");

var types = [
  { type: "float",
    code: [
      "float $(func)_emu($(args)) {",
      "  return $(func)_base($(baseArgs));",
      "}"].join("\n")
  },
  { type: "vec2",
    code: [
      "vec2 $(func)_emu($(args)) {",
      "  return vec2(",
      "      $(func)_base($(baseArgsX)),",
      "      $(func)_base($(baseArgsY)));",
      "}"].join("\n")
  },
  { type: "vec3",
    code: [
      "vec3 $(func)_emu($(args)) {",
      "  return vec3(",
      "      $(func)_base($(baseArgsX)),",
      "      $(func)_base($(baseArgsY)),",
      "      $(func)_base($(baseArgsZ)));",
      "}"].join("\n")
  },
  { type: "vec4",
    code: [
      "vec4 $(func)_emu($(args)) {",
      "  return vec4(",
      "      $(func)_base($(baseArgsX)),",
      "      $(func)_base($(baseArgsY)),",
      "      $(func)_base($(baseArgsZ)),",
      "      $(func)_base($(baseArgsW)));",
      "}"].join("\n")
  }
];

var bvecTypes = [
  { type: "bvec2",
    code: [
      "bvec2 $(func)_emu($(args)) {",
      "  return bvec2(",
      "      $(func)_base($(baseArgsX)),",
      "      $(func)_base($(baseArgsY)));",
      "}"].join("\n")
  },
  { type: "bvec3",
    code: [
      "bvec3 $(func)_emu($(args)) {",
      "  return bvec3(",
      "      $(func)_base($(baseArgsX)),",
      "      $(func)_base($(baseArgsY)),",
      "      $(func)_base($(baseArgsZ)));",
      "}"].join("\n")
  },
  { type: "bvec4",
    code: [
      "vec4 $(func)_emu($(args)) {",
      "  return bvec4(",
      "      $(func)_base($(baseArgsX)),",
      "      $(func)_base($(baseArgsY)),",
      "      $(func)_base($(baseArgsZ)),",
      "      $(func)_base($(baseArgsW)));",
      "}"].join("\n")
  }
];

var replaceRE = /\$\((\w+)\)/g;

var replaceParams = function(str) {
  var args = arguments;
  return str.replace(replaceRE, function(str, p1, offset, s) {
    for (var ii = 1; ii < args.length; ++ii) {
      if (args[ii][p1] !== undefined) {
        return args[ii][p1];
      }
    }
    throw "unknown string param '" + p1 + "'";
  });
};

var generateReferenceShader = function(
    shaderInfo, template, params, typeInfo, test) {
  var input = shaderInfo.input;
  var output = shaderInfo.output;
  var feature = params.feature;
  var testFunc = params.testFunc;
  var emuFunc = params.emuFunc || "";
  var extra = params.extra || '';
  var args = params.args || "$(type) value";
  var type = typeInfo.type;
  var typeCode = typeInfo.code;

  var baseArgs = params.baseArgs || "value$(field)";
  var baseArgsX = replaceParams(baseArgs, {field: ".x"});
  var baseArgsY = replaceParams(baseArgs, {field: ".y"});
  var baseArgsZ = replaceParams(baseArgs, {field: ".z"});
  var baseArgsW = replaceParams(baseArgs, {field: ".w"});
  var baseArgs = replaceParams(baseArgs, {field: ""});

  test = replaceParams(test, {
    input: input,
    output: output,
    func: feature + "_emu"
  });
  emuFunc = replaceParams(emuFunc, {
    func: feature
  });
  args = replaceParams(args, {
    type: type
  });
  typeCode = replaceParams(typeCode, {
    func: feature,
    type: type,
    args: args,
    baseArgs: baseArgs,
    baseArgsX: baseArgsX,
    baseArgsY: baseArgsY,
    baseArgsZ: baseArgsZ,
    baseArgsW: baseArgsW
  });
  var shader = replaceParams(template, {
    extra: extra,
    emu: emuFunc + "\n\n" + typeCode,
    test: test
  });
  return shader;
};

var generateTestShader = function(
    shaderInfo, template, params, test) {
  var input = shaderInfo.input;
  var output = shaderInfo.output;
  var feature = params.feature;
  var testFunc = params.testFunc;
  var extra = params.extra || '';

  test = replaceParams(test, {
    input: input,
    output: output,
    func: feature
  });
  var shader = replaceParams(template, {
    extra: extra,
    emu: '',
    test: test
  });
  return shader;
};

var runFeatureTest = function(params) {
  var wtu = WebGLTestUtils;
  var gridRes = params.gridRes;
  var vertexTolerance = params.tolerance || 0;
  var fragmentTolerance = vertexTolerance;
  if ('fragmentTolerance' in params)
    fragmentTolerance = params.fragmentTolerance || 0;

  description("Testing GLSL feature: " + params.feature);

  var width = 32;
  var height = 32;

  var console = document.getElementById("console");
  var canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  var gl = wtu.create3DContext(canvas);
  if (!gl) {
    testFailed("context does not exist");
    finishTest();
    return;
  }

  var canvas2d = document.createElement('canvas');
  canvas2d.width = width;
  canvas2d.height = height;
  var ctx = canvas2d.getContext("2d");
  var imgData = ctx.getImageData(0, 0, width, height);

  var shaderInfos = [
    { type: "vertex",
      input: "color",
      output: "vColor",
      vertexShaderTemplate: vertexShaderTemplate,
      fragmentShaderTemplate: baseFragmentShader,
      tolerance: vertexTolerance
    },
    { type: "fragment",
      input: "vColor",
      output: "gl_FragColor",
      vertexShaderTemplate: baseVertexShader,
      fragmentShaderTemplate: fragmentShaderTemplate,
      tolerance: fragmentTolerance
    }
  ];
  for (var ss = 0; ss < shaderInfos.length; ++ss) {
    var shaderInfo = shaderInfos[ss];
    var tests = params.tests;
    var testTypes = params.emuFuncs || (params.bvecTest ? bvecTypes : types);
    // Test vertex shaders
    for (var ii = 0; ii < tests.length; ++ii) {
      var type = testTypes[ii];
      if (params.simpleEmu) {
        type = {
          type: type.type,
          code: params.simpleEmu
        };
      }
      debug("");
      var str = replaceParams(params.testFunc, {
        func: params.feature,
        type: type.type,
        arg0: type.type
      });
      debug("Testing: " + str + " in " + shaderInfo.type + " shader");

      var referenceVertexShaderSource = generateReferenceShader(
          shaderInfo,
          shaderInfo.vertexShaderTemplate,
          params,
          type,
          tests[ii]);
      var referenceFragmentShaderSource = generateReferenceShader(
          shaderInfo,
          shaderInfo.fragmentShaderTemplate,
          params,
          type,
          tests[ii]);
      var testVertexShaderSource = generateTestShader(
          shaderInfo,
          shaderInfo.vertexShaderTemplate,
          params,
          tests[ii]);
      var testFragmentShaderSource = generateTestShader(
          shaderInfo,
          shaderInfo.fragmentShaderTemplate,
          params,
          tests[ii]);

      debug("");
      wtu.addShaderSource(
          console, "reference vertex shader", referenceVertexShaderSource);
      wtu.addShaderSource(
          console, "reference fragment shader", referenceFragmentShaderSource);
      wtu.addShaderSource(
          console, "test vertex shader", testVertexShaderSource);
      wtu.addShaderSource(
          console, "test fragment shader", testFragmentShaderSource);
      debug("");

      var refData = draw(
          canvas, referenceVertexShaderSource, referenceFragmentShaderSource);
      var refImg = wtu.makeImage(canvas);
      if (ss == 0) {
        var testData = draw(
            canvas, testVertexShaderSource, referenceFragmentShaderSource);
      } else {
        var testData = draw(
            canvas, referenceVertexShaderSource, testFragmentShaderSource);
      }
      var testImg = wtu.makeImage(canvas);

      reportResults(refData, refImg, testData, testImg, shaderInfo.tolerance);
    }
  }

  finishTest();

  function reportResults(refData, refImage, testData, testImage, tolerance) {
    var same = true;
    for (var yy = 0; yy < height; ++yy) {
      for (var xx = 0; xx < width; ++xx) {
        var offset = (yy * width + xx) * 4;
        var imgOffset = ((height - yy - 1) * width + xx) * 4;
        imgData.data[imgOffset + 0] = 0;
        imgData.data[imgOffset + 1] = 0;
        imgData.data[imgOffset + 2] = 0;
        imgData.data[imgOffset + 3] = 255;
        if (Math.abs(refData[offset + 0] - testData[offset + 0]) > tolerance ||
            Math.abs(refData[offset + 1] - testData[offset + 1]) > tolerance ||
            Math.abs(refData[offset + 2] - testData[offset + 2]) > tolerance ||
            Math.abs(refData[offset + 3] - testData[offset + 3]) > tolerance) {
          imgData.data[imgOffset] = 255;
          same = false;
        }
      }
    }

    var diffImg = null;
    if (!same) {
      ctx.putImageData(imgData, 0, 0);
      diffImg = wtu.makeImage(canvas2d);
    }

    var div = document.createElement("div");
    div.className = "testimages";
    wtu.insertImage(div, "ref", refImg);
    wtu.insertImage(div, "test", testImg);
    if (diffImg) {
      wtu.insertImage(div, "diff", diffImg);
    }
    div.appendChild(document.createElement('br'));


    console.appendChild(div);

    if (!same) {
      testFailed("images are different");
    } else {
      testPassed("images are the same");
    }

    console.appendChild(document.createElement('hr'));
  }

  function draw(canvas, vsSource, fsSource) {
    var program = wtu.loadProgram(gl, vsSource, fsSource, testFailed);

    var posLoc = gl.getAttribLocation(program, "aPosition");
    WebGLTestUtils.setupQuad(gl, gridRes, posLoc);

    gl.useProgram(program);
    gl.clearColor(0, 0, 1, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.drawElements(gl.TRIANGLES, gridRes * gridRes * 6, gl.UNSIGNED_SHORT, 0);
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "no errors from draw");

    var img = new Uint8Array(width * height * 4);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, img);
    return img;
  }

};

var runBasicTest = function(params) {
  var wtu = WebGLTestUtils;
  var gridRes = params.gridRes;
  var vertexTolerance = params.tolerance || 0;
  var fragmentTolerance = vertexTolerance;
  if ('fragmentTolerance' in params)
    fragmentTolerance = params.fragmentTolerance || 0;

  description("Testing : " + document.getElementsByTagName("title")[0].innerText);

  var width = 32;
  var height = 32;

  var console = document.getElementById("console");
  var canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  var gl = wtu.create3DContext(canvas);
  if (!gl) {
    testFailed("context does not exist");
    finishTest();
    return;
  }

  var canvas2d = document.createElement('canvas');
  canvas2d.width = width;
  canvas2d.height = height;
  var ctx = canvas2d.getContext("2d");
  var imgData = ctx.getImageData(0, 0, width, height);

  var shaderInfos = [
    { type: "vertex",
      input: "color",
      output: "vColor",
      vertexShaderTemplate: vertexShaderTemplate,
      fragmentShaderTemplate: baseFragmentShader,
      tolerance: vertexTolerance
    },
    { type: "fragment",
      input: "vColor",
      output: "gl_FragColor",
      vertexShaderTemplate: baseVertexShader,
      fragmentShaderTemplate: fragmentShaderTemplate,
      tolerance: fragmentTolerance
    }
  ];
  for (var ss = 0; ss < shaderInfos.length; ++ss) {
    var shaderInfo = shaderInfos[ss];
    var tests = params.tests;
//    var testTypes = params.emuFuncs || (params.bvecTest ? bvecTypes : types);
    // Test vertex shaders
    for (var ii = 0; ii < tests.length; ++ii) {
      var test = tests[ii];
      debug("");
      debug("Testing: " + test.name + " in " + shaderInfo.type + " shader");

      function genShader(shaderInfo, template, shader, subs) {
        shader = replaceParams(shader, subs, {
            input: shaderInfo.input,
            output: shaderInfo.output
          });
        shader = replaceParams(template, subs, {
            test: shader,
            emu: "",
            extra: ""
          });
        return shader;
      }

      var referenceVertexShaderSource = genShader(
          shaderInfo,
          shaderInfo.vertexShaderTemplate,
          test.reference.shader,
          test.reference.subs);
      var referenceFragmentShaderSource = genShader(
          shaderInfo,
          shaderInfo.fragmentShaderTemplate,
          test.reference.shader,
          test.reference.subs);
      var testVertexShaderSource = genShader(
          shaderInfo,
          shaderInfo.vertexShaderTemplate,
          test.test.shader,
          test.test.subs);
      var testFragmentShaderSource = genShader(
          shaderInfo,
          shaderInfo.fragmentShaderTemplate,
          test.test.shader,
          test.test.subs);

      debug("");
      wtu.addShaderSource(
          console, "reference vertex shader", referenceVertexShaderSource);
      wtu.addShaderSource(
          console, "reference fragment shader", referenceFragmentShaderSource);
      wtu.addShaderSource(
          console, "test vertex shader", testVertexShaderSource);
      wtu.addShaderSource(
          console, "test fragment shader", testFragmentShaderSource);
      debug("");

      var refData = draw(
          canvas, referenceVertexShaderSource, referenceFragmentShaderSource);
      var refImg = wtu.makeImage(canvas);
      if (ss == 0) {
        var testData = draw(
            canvas, testVertexShaderSource, referenceFragmentShaderSource);
      } else {
        var testData = draw(
            canvas, referenceVertexShaderSource, testFragmentShaderSource);
      }
      var testImg = wtu.makeImage(canvas);

      reportResults(refData, refImg, testData, testImg, shaderInfo.tolerance);
    }
  }

  finishTest();

  function reportResults(refData, refImage, testData, testImage, tolerance) {
    var same = true;
    for (var yy = 0; yy < height; ++yy) {
      for (var xx = 0; xx < width; ++xx) {
        var offset = (yy * width + xx) * 4;
        var imgOffset = ((height - yy - 1) * width + xx) * 4;
        imgData.data[imgOffset + 0] = 0;
        imgData.data[imgOffset + 1] = 0;
        imgData.data[imgOffset + 2] = 0;
        imgData.data[imgOffset + 3] = 255;
        if (Math.abs(refData[offset + 0] - testData[offset + 0]) > tolerance ||
            Math.abs(refData[offset + 1] - testData[offset + 1]) > tolerance ||
            Math.abs(refData[offset + 2] - testData[offset + 2]) > tolerance ||
            Math.abs(refData[offset + 3] - testData[offset + 3]) > tolerance) {
          imgData.data[imgOffset] = 255;
          same = false;
        }
      }
    }

    var diffImg = null;
    if (!same) {
      ctx.putImageData(imgData, 0, 0);
      diffImg = wtu.makeImage(canvas2d);
    }

    var div = document.createElement("div");
    div.className = "testimages";
    wtu.insertImage(div, "ref", refImg);
    wtu.insertImage(div, "test", testImg);
    if (diffImg) {
      wtu.insertImage(div, "diff", diffImg);
    }
    div.appendChild(document.createElement('br'));

    console.appendChild(div);

    if (!same) {
      testFailed("images are different");
    } else {
      testPassed("images are the same");
    }

    console.appendChild(document.createElement('hr'));
  }

  function draw(canvas, vsSource, fsSource) {
    var program = wtu.loadProgram(gl, vsSource, fsSource, testFailed);

    var posLoc = gl.getAttribLocation(program, "aPosition");
    WebGLTestUtils.setupQuad(gl, gridRes, posLoc);

    gl.useProgram(program);
    gl.clearColor(0, 0, 1, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.drawElements(gl.TRIANGLES, gridRes * gridRes * 6, gl.UNSIGNED_SHORT, 0);
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "no errors from draw");

    var img = new Uint8Array(width * height * 4);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, img);
    return img;
  }

};

var runReferenceImageTest = function(params) {
  var wtu = WebGLTestUtils;
  var gridRes = params.gridRes;
  var vertexTolerance = params.tolerance || 0;
  var fragmentTolerance = vertexTolerance;
  if ('fragmentTolerance' in params)
    fragmentTolerance = params.fragmentTolerance || 0;

  description("Testing GLSL feature: " + params.feature);

  var width = 32;
  var height = 32;

  var console = document.getElementById("console");
  var canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  var gl = wtu.create3DContext(canvas, { antialias: false });
  if (!gl) {
    testFailed("context does not exist");
    finishTest();
    return;
  }

  var canvas2d = document.createElement('canvas');
  canvas2d.width = width;
  canvas2d.height = height;
  var ctx = canvas2d.getContext("2d");
  var imgData = ctx.getImageData(0, 0, width, height);

  var shaderInfos = [
    { type: "vertex",
      input: "color",
      output: "vColor",
      vertexShaderTemplate: vertexShaderTemplate,
      fragmentShaderTemplate: baseFragmentShader,
      tolerance: vertexTolerance
    },
    { type: "fragment",
      input: "vColor",
      output: "gl_FragColor",
      vertexShaderTemplate: baseVertexShader,
      fragmentShaderTemplate: fragmentShaderTemplate,
      tolerance: fragmentTolerance
    }
  ];
  for (var ss = 0; ss < shaderInfos.length; ++ss) {
    var shaderInfo = shaderInfos[ss];
    var tests = params.tests;
    var testTypes = params.emuFuncs || (params.bvecTest ? bvecTypes : types);
    // Test vertex shaders
    for (var ii = 0; ii < tests.length; ++ii) {
      var type = testTypes[ii];
      var isVertex = (ss == 0);
      debug("");
      var str = replaceParams(params.testFunc, {
        func: params.feature,
        type: type.type,
        arg0: type.type
      });
      debug("Testing: " + str + " in " + shaderInfo.type + " shader");

      var referenceVertexShaderSource = generateReferenceShader(
          shaderInfo,
          shaderInfo.vertexShaderTemplate,
          params,
          type,
          tests[ii].source);
      var referenceFragmentShaderSource = generateReferenceShader(
          shaderInfo,
          shaderInfo.fragmentShaderTemplate,
          params,
          type,
          tests[ii].source);
      var testVertexShaderSource = generateTestShader(
          shaderInfo,
          shaderInfo.vertexShaderTemplate,
          params,
          tests[ii].source);
      var testFragmentShaderSource = generateTestShader(
          shaderInfo,
          shaderInfo.fragmentShaderTemplate,
          params,
          tests[ii].source);
      var referenceTexture = generateReferenceTexture(
          gl,
          tests[ii].generator,
          isVertex ? gridRes : width,
          isVertex ? gridRes : height,
          isVertex);

      debug("");
      wtu.addShaderSource(
          console, "test vertex shader", testVertexShaderSource);
      wtu.addShaderSource(
          console, "test fragment shader", testFragmentShaderSource);
      debug("");
      var refData = drawReferenceImage(canvas, referenceTexture, isVertex);
      var refImg = wtu.makeImage(canvas);
      if (isVertex) {
        var testData = draw(
            canvas, testVertexShaderSource, referenceFragmentShaderSource);
      } else {
        var testData = draw(
            canvas, referenceVertexShaderSource, testFragmentShaderSource);
      }
      var testImg = wtu.makeImage(canvas);
      var testTolerance = shaderInfo.tolerance;
      // Provide per-test tolerance so that we can increase it only for those desired.
      if ('tolerance' in tests[ii])
        testTolerance = tests[ii].tolerance || 0;
      reportResults(refData, refImg, testData, testImg, testTolerance);
    }
  }

  finishTest();

  function reportResults(refData, refImage, testData, testImage, tolerance) {
    var same = true;
    for (var yy = 0; yy < height; ++yy) {
      for (var xx = 0; xx < width; ++xx) {
        var offset = (yy * width + xx) * 4;
        var imgOffset = ((height - yy - 1) * width + xx) * 4;
        imgData.data[imgOffset + 0] = 0;
        imgData.data[imgOffset + 1] = 0;
        imgData.data[imgOffset + 2] = 0;
        imgData.data[imgOffset + 3] = 255;
        if (Math.abs(refData[offset + 0] - testData[offset + 0]) > tolerance ||
            Math.abs(refData[offset + 1] - testData[offset + 1]) > tolerance ||
            Math.abs(refData[offset + 2] - testData[offset + 2]) > tolerance ||
            Math.abs(refData[offset + 3] - testData[offset + 3]) > tolerance) {
          console.appendChild(document.createTextNode('at (' + xx + ',' + yy + '): ref=(' +
                                                      refData[offset + 0] + ',' +
                                                      refData[offset + 1] + ',' +
                                                      refData[offset + 2] + ',' +
                                                      refData[offset + 3] + ')  test=(' +
                                                      testData[offset + 0] + ',' +
                                                      testData[offset + 1] + ',' +
                                                      testData[offset + 2] + ',' +
                                                      testData[offset + 3] + ')'));
          console.appendChild(document.createElement('br'));          



          imgData.data[imgOffset] = 255;
          same = false;
        }
      }
    }

    var diffImg = null;
    if (!same) {
      ctx.putImageData(imgData, 0, 0);
      diffImg = wtu.makeImage(canvas2d);
    }

    var div = document.createElement("div");
    div.className = "testimages";
    wtu.insertImage(div, "ref", refImg);
    wtu.insertImage(div, "test", testImg);
    if (diffImg) {
      wtu.insertImage(div, "diff", diffImg);
    }
    div.appendChild(document.createElement('br'));

    console.appendChild(div);

    if (!same) {
      testFailed("images are different");
    } else {
      testPassed("images are the same");
    }

    console.appendChild(document.createElement('hr'));
  }

  function draw(canvas, vsSource, fsSource) {
    var program = wtu.loadProgram(gl, vsSource, fsSource, testFailed);

    var posLoc = gl.getAttribLocation(program, "aPosition");
    WebGLTestUtils.setupQuad(gl, gridRes, posLoc);

    gl.useProgram(program);
    gl.clearColor(0, 0, 1, 1);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.drawElements(gl.TRIANGLES, gridRes * gridRes * 6, gl.UNSIGNED_SHORT, 0);
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "no errors from draw");

    var img = new Uint8Array(width * height * 4);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, img);
    return img;
  }

  function drawReferenceImage(canvas, texture, isVertex) {
    var program;
    if (isVertex) {
      var halfTexel = 0.5 / (1.0 + gridRes);
      program = WebGLTestUtils.setupTexturedQuadWithTexCoords(
        gl, [halfTexel, halfTexel], [1.0 - halfTexel, 1.0 - halfTexel]);
    } else {
      program = WebGLTestUtils.setupTexturedQuad(gl);
    }

    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, texture);
    var texLoc = gl.getUniformLocation(program, "tex");
    gl.uniform1i(texLoc, 0);
    wtu.drawQuad(gl);
    wtu.glErrorShouldBe(gl, gl.NO_ERROR, "no errors from draw");

    var img = new Uint8Array(width * height * 4);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, img);
    return img;
  }

  /**
   * Creates and returns a texture containing the reference image for
   * the function being tested. Exactly how the function is evaluated,
   * and the size of the returned texture, depends on whether we are
   * testing a vertex or fragment shader. If a fragment shader, the
   * function is evaluated at the pixel centers. If a vertex shader,
   * the function is evaluated at the triangle's vertices, and the
   * resulting texture must be offset by half a texel during
   * rendering.
   *
   * @param {!WebGLRenderingContext} gl The WebGLRenderingContext to use to generate texture objects.
   * @param {!function(number,number,number,number): !Array.<number>} generator The reference image generator function.
   * @param {number} width The width of the texture to generate if testing a fragment shader; the grid resolution if testing a vertex shader.
   * @param {number} height The height of the texture to generate if testing a fragment shader; the grid resolution if testing a vertex shader.
   * @param {boolean} isVertex True if generating a reference image for a vertex shader; false if for a fragment shader.
   * @return {!WebGLTexture} The texture object that was generated.
   */
  function generateReferenceTexture(
    gl,
    generator,
    width,
    height,
    isVertex) {

    // Note: the math in this function must match that in the vertex and
    // fragment shader templates above.
    function computeTexCoord(x) {
      return x * 0.5 + 0.5;
    }

    function computeColor(texCoordX, texCoordY) {
      return [ texCoordX,
               texCoordY,
               texCoordX * texCoordY,
               (1.0 - texCoordX) * texCoordY * 0.5 + 0.5 ];
    }

    function clamp(value, minVal, maxVal) {
      return Math.max(minVal, Math.min(value, maxVal));
    }

    // Evaluates the function at clip coordinates (px,py), storing the
    // result in the array "pixel". Each channel's result is clamped
    // between 0 and 255.
    function evaluateAtClipCoords(px, py, pixel) {
      var tcx = computeTexCoord(px);
      var tcy = computeTexCoord(py);

      var color = computeColor(tcx, tcy);

      var output = generator(color[0], color[1], color[2], color[3]);

      // Multiply by 256 to get even distribution for all values between 0 and 1.
      // Use rounding rather than truncation to more closely match the GPU's behavior.
      pixel[0] = clamp(Math.round(256 * output[0]), 0, 255);
      pixel[1] = clamp(Math.round(256 * output[1]), 0, 255);
      pixel[2] = clamp(Math.round(256 * output[2]), 0, 255);
      pixel[3] = clamp(Math.round(256 * output[3]), 0, 255);
    }

    function fillFragmentReference() {
      var data = new Uint8Array(4 * width * height);

      var horizTexel = 1.0 / width;
      var vertTexel = 1.0 / height;
      var halfHorizTexel = 0.5 * horizTexel;
      var halfVertTexel = 0.5 * vertTexel;

      var pixel = new Array(4);

      for (var yi = 0; yi < height; ++yi) {
        for (var xi = 0; xi < width; ++xi) {
          // The function must be evaluated at pixel centers.

          // Compute desired position in clip space
          var px = -1.0 + 2.0 * (halfHorizTexel + xi * horizTexel);
          var py = -1.0 + 2.0 * (halfVertTexel + yi * vertTexel);

          evaluateAtClipCoords(px, py, pixel);
          var index = 4 * (width * yi + xi);
          data[index + 0] = pixel[0];
          data[index + 1] = pixel[1];
          data[index + 2] = pixel[2];
          data[index + 3] = pixel[3];
        }
      }

      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0,
                    gl.RGBA, gl.UNSIGNED_BYTE, data);
    }

    function fillVertexReference() {
      // We generate a texture which contains the evaluation of the
      // function at the vertices of the triangle mesh. It is expected
      // that the width and the height are identical, and equivalent
      // to the grid resolution.
      if (width != height) {
        throw "width and height must be equal";
      }

      var texSize = 1 + width;
      var data = new Uint8Array(4 * texSize * texSize);

      var step = 2.0 / width;

      var pixel = new Array(4);

      for (var yi = 0; yi < texSize; ++yi) {
        for (var xi = 0; xi < texSize; ++xi) {
          // The function is evaluated at the triangles' vertices.

          // Compute desired position in clip space
          var px = -1.0 + (xi * step);
          var py = -1.0 + (yi * step);

          evaluateAtClipCoords(px, py, pixel);
          var index = 4 * (texSize * yi + xi);
          data[index + 0] = pixel[0];
          data[index + 1] = pixel[1];
          data[index + 2] = pixel[2];
          data[index + 3] = pixel[3];
        }
      }

      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, texSize, texSize, 0,
                    gl.RGBA, gl.UNSIGNED_BYTE, data);
    }

    //----------------------------------------------------------------------
    // Body of generateReferenceTexture
    //

    var texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    if (isVertex) {
      fillVertexReference();
    } else {
      fillFragmentReference();
    }

    return texture;
  }
};

return {
  /**
   * runs a bunch of GLSL tests using the passed in parameters
   * The parameters are:
   *
   * feature:
   *    the name of the function being tested (eg, sin, dot,
   *    normalize)
   *
   * testFunc:
   *    The prototype of function to be tested not including the
   *    return type.
   *
   * emuFunc:
   *    A base function that can be used to generate emulation
   *    functions. Example for 'ceil'
   *
   *      float $(func)_base(float value) {
   *        float m = mod(value, 1.0);
   *        return m != 0.0 ? (value + 1.0 - m) : value;
   *      }
   *
   * args:
   *    The arguments to the function
   *
   * baseArgs: (optional)
   *    The arguments when a base function is used to create an
   *    emulation function. For example 'float sign_base(float v)'
   *    is used to implemenent vec2 sign_emu(vec2 v).
   *
   * simpleEmu:
   *    if supplied, the code that can be used to generate all
   *    functions for all types.
   *
   *    Example for 'normalize':
   *
   *        $(type) $(func)_emu($(args)) {
   *           return value / length(value);
   *        }
   *
   * gridRes: (optional)
   *    The resolution of the mesh to generate. The default is a
   *    1x1 grid but many vertex shaders need a higher resolution
   *    otherwise the only values passed in are the 4 corners
   *    which often have the same value.
   *
   * tests:
   *    The code for each test. It is assumed the tests are for
   *    float, vec2, vec3, vec4 in that order.
   *
   * tolerance: (optional)
   *    Allow some tolerance in the comparisons. The tolerance is applied to 
   *    both vertex and fragment shaders. The default tolerance is 0, meaning 
   *    the values have to be identical.
   *
   * fragmentTolerance: (optional)
   *    Specify a tolerance which only applies to fragment shaders. The 
   *    fragment-only tolerance will override the shared tolerance for 
   *    fragment shaders if both are specified. Fragment shaders usually
   *    use mediump float precision so they sometimes require higher tolerance
   *    than vertex shaders which use highp by default.
   */
  runFeatureTest: runFeatureTest,

  /*
   * Runs a bunch of GLSL tests using the passed in parameters
   *
   * The parameters are:
   *
   * tests:
   *    Array of tests. For each test the following parameters are expected
   *
   *    name:
   *       some description of the test
   *    reference:
   *       parameters for the reference shader (see below)
   *    test:
   *       parameters for the test shader (see below)
   *
   *    The parameter for the reference and test shaders are
   *
   *    shader: the GLSL for the shader
   *    subs: any substitutions you wish to define for the shader.
   *
   *    Each shader is created from a basic template that
   *    defines an input and an output. You can see the
   *    templates at the top of this file. The input and output
   *    change depending on whether or not we are generating
   *    a vertex or fragment shader.
   *
   *    All this code function does is a bunch of string substitutions.
   *    A substitution is defined by $(name). If name is found in
   *    the 'subs' parameter it is replaced. 4 special names exist.
   *
   *    'input' the input to your GLSL. Always a vec4. All change
   *    from 0 to 1 over the quad to be drawn.
   *
   *    'output' the output color. Also a vec4
   *
   *    'emu' a place to insert extra stuff
   *    'extra' a place to insert extra stuff.
   *
   *    You can think of the templates like this
   *
   *       $(extra)
   *       $(emu)
   *
   *       void main() {
   *          // do math to calculate input
   *          ...
   *
   *          $(shader)
   *       }
   *
   *    Your shader first has any subs you provided applied as well
   *    as 'input' and 'output'
   *
   *    It is then inserted into the template which is also provided
   *    with your subs.
   *
   * gridRes: (optional)
   *    The resolution of the mesh to generate. The default is a
   *    1x1 grid but many vertex shaders need a higher resolution
   *    otherwise the only values passed in are the 4 corners
   *    which often have the same value.
   *
   * tolerance: (optional)
   *    Allow some tolerance in the comparisons. The tolerance is applied to
   *    both vertex and fragment shaders. The default tolerance is 0, meaning
   *    the values have to be identical.
   *
   * fragmentTolerance: (optional)
   *    Specify a tolerance which only applies to fragment shaders. The
   *    fragment-only tolerance will override the shared tolerance for
   *    fragment shaders if both are specified. Fragment shaders usually
   *    use mediump float precision so they sometimes require higher tolerance
   *    than vertex shaders which use highp.
   */
  runBasicTest: runBasicTest,

  /**
   * Runs a bunch of GLSL tests using the passed in parameters. The
   * expected results are computed as a reference image in JavaScript
   * instead of on the GPU. The parameters are:
   *
   * feature:
   *    the name of the function being tested (eg, sin, dot,
   *    normalize)
   *
   * testFunc:
   *    The prototype of function to be tested not including the
   *    return type.
   *
   * args:
   *    The arguments to the function
   *
   * gridRes: (optional)
   *    The resolution of the mesh to generate. The default is a
   *    1x1 grid but many vertex shaders need a higher resolution
   *    otherwise the only values passed in are the 4 corners
   *    which often have the same value.
   *
   * tests:
   *    Array of tests. It is assumed the tests are for float, vec2,
   *    vec3, vec4 in that order. For each test the following
   *    parameters are expected:
   *
   *       source: the GLSL source code for the tests
   *
   *       generator: a JavaScript function taking four parameters
   *       which evaluates the same function as the GLSL source,
   *       returning its result as a newly allocated array.
   *
   *       tolerance: (optional) a per-test tolerance.
   *
   * extra: (optional)
   *    Extra GLSL code inserted at the top of each test's shader.
   * 
   * tolerance: (optional)
   *    Allow some tolerance in the comparisons. The tolerance is applied to 
   *    both vertex and fragment shaders. The default tolerance is 0, meaning 
   *    the values have to be identical.
   *
   * fragmentTolerance: (optional)
   *    Specify a tolerance which only applies to fragment shaders. The 
   *    fragment-only tolerance will override the shared tolerance for 
   *    fragment shaders if both are specified. Fragment shaders usually
   *    use mediump float precision so they sometimes require higher tolerance
   *    than vertex shaders which use highp.
   */
  runReferenceImageTest: runReferenceImageTest,

  none: false
};

}());


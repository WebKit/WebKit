/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/

// This test relies on the surrounding web page defining a variable
// "contextVersion" which indicates what version of WebGL it's running
// on -- 1 for WebGL 1.0, 2 for WebGL 2.0, etc.

"use strict";
description("This test ensures various WebGL functions fail when passed invalid OpenGL ES enums.");

debug("");
debug("Canvas.getContext");

function getDesktopGL(name) {
  shouldBeDefined(`desktopGL['${name}']`);
  return desktopGL[name];
}

var wtu = WebGLTestUtils;
var gl = wtu.create3DContext("canvas", undefined, contextVersion);
if (!gl) {
  testFailed("context does not exist");
} else {
  testPassed("context exists");

  debug("");
  debug("Checking gl enums.");

  var buffer = new ArrayBuffer(2);
  var buf = new Uint16Array(buffer);
  var tex = gl.createTexture();
  var program = wtu.createProgram(gl, wtu.loadStandardVertexShader(gl), wtu.loadStandardFragmentShader(gl));
  gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
  wtu.glErrorShouldBe(gl, gl.NO_ERROR);

  var tests = [
    "gl.disable(getDesktopGL('CLIP_PLANE0'))",
    "gl.disable(getDesktopGL('POINT_SPRITE'))",
    "gl.getBufferParameter(gl.ARRAY_BUFFER, getDesktopGL('PIXEL_PACK_BUFFER'))",
    "gl.hint(getDesktopGL('PERSPECTIVE_CORRECTION_HINT'), gl.FASTEST)",
    "gl.isEnabled(getDesktopGL('CLIP_PLANE0'))",
    "gl.isEnabled(getDesktopGL('POINT_SPRITE'))",
    "gl.pixelStorei(getDesktopGL('PACK_SWAP_BYTES'), 1)",
    "gl.getParameter(getDesktopGL('NUM_COMPRESSED_TEXTURE_FORMATS'))",
    "gl.getParameter(getDesktopGL('EXTENSIONS'))",
    "gl.getParameter(getDesktopGL('SHADER_COMPILER'))",
    "gl.getParameter(getDesktopGL('SHADER_BINARY_FORMATS'))",
    "gl.getParameter(getDesktopGL('NUM_SHADER_BINARY_FORMATS'))",
  ];

  if (contextVersion < 2) {
    tests = tests.concat([
      "gl.blendEquation(getDesktopGL('MIN'))",
      "gl.blendEquation(getDesktopGL('MAX'))",
      "gl.blendEquationSeparate(getDesktopGL('MIN'), gl.FUNC_ADD)",
      "gl.blendEquationSeparate(getDesktopGL('MAX'), gl.FUNC_ADD)",
      "gl.blendEquationSeparate(gl.FUNC_ADD, getDesktopGL('MIN'))",
      "gl.blendEquationSeparate(gl.FUNC_ADD, getDesktopGL('MAX'))",
      "gl.bufferData(gl.ARRAY_BUFFER, 16, getDesktopGL('STREAM_READ'))",
      "gl.bufferData(gl.ARRAY_BUFFER, 16, getDesktopGL('STREAM_COPY'))",
      "gl.bufferData(gl.ARRAY_BUFFER, 16, getDesktopGL('STATIC_READ'))",
      "gl.bufferData(gl.ARRAY_BUFFER, 16, getDesktopGL('STATIC_COPY'))",
      "gl.bufferData(gl.ARRAY_BUFFER, 16, getDesktopGL('DYNAMIC_READ'))",
      "gl.bufferData(gl.ARRAY_BUFFER, 16, getDesktopGL('DYNAMIC_COPY'))",
      "gl.bindTexture(getDesktopGL('TEXTURE_2D_ARRAY'), tex)",
      "gl.bindTexture(getDesktopGL('TEXTURE_3D'), tex)",
    ]);
  } else {
    tests = tests.concat([
      "gl.bindTexture(getDesktopGL('TEXTURE_RECTANGLE_EXT'), tex)",
      "gl.enable(getDesktopGL('PRIMITIVE_RESTART_FIXED_INDEX'))",
      "gl.getActiveUniforms(program, [0], getDesktopGL('UNIFORM_NAME_LENGTH'))",
      "gl.getProgramParameter(program, getDesktopGL('ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH'))",
      "gl.getProgramParameter(program, getDesktopGL('TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH'))",
      "gl.getProgramParameter(program, getDesktopGL('PROGRAM_BINARY_RETRIEVABLE_HINT'))",
      "gl.getProgramParameter(program, getDesktopGL('PROGRAM_BINARY_LENGTH'))",
      "gl.getParameter(program, getDesktopGL('NUM_PROGRAM_BINARY_FORMATS'))",
    ]);
  }

  for (var ii = 0; ii < tests.length; ++ii) {
    TestEval(tests[ii]);
    wtu.glErrorShouldBe(gl, gl.INVALID_ENUM, tests[ii] + " should return INVALID_ENUM.");
  }

  gl.bindTexture(gl.TEXTURE_2D, tex);
  wtu.glErrorShouldBe(gl, gl.NO_ERROR);

  tests = [
    "gl.getTexParameter(gl.TEXTURE_2D, getDesktopGL('GENERATE_MIPMAP'))",
    "gl.texParameteri(gl.TEXTURE_2D, getDesktopGL('GENERATE_MIPMAP'), 1)",
    "gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, getDesktopGL('CLAMP_TO_BORDER'))",
  ];

  if (contextVersion < 2) {
    tests = tests.concat([
      "gl.texParameteri(getDesktopGL('TEXTURE_2D_ARRAY'), gl.TEXTURE_MAG_FILTER, gl.NEAREST)",
      "gl.texParameteri(getDesktopGL('TEXTURE_3D'), gl.TEXTURE_MAG_FILTER, gl.NEAREST)",
    ]);
  } else {
    tests = tests.concat([
      "gl.texParameteri(getDesktopGL('TEXTURE_2D'), getDesktopGL('TEXTURE_SWIZZLE_R_EXT'), gl.RED)",
      "gl.texParameteri(getDesktopGL('TEXTURE_2D'), getDesktopGL('TEXTURE_SWIZZLE_G_EXT'), gl.RED)",
      "gl.texParameteri(getDesktopGL('TEXTURE_2D'), getDesktopGL('TEXTURE_SWIZZLE_B_EXT'), gl.RED)",
      "gl.texParameteri(getDesktopGL('TEXTURE_2D'), getDesktopGL('TEXTURE_SWIZZLE_A_EXT'), gl.RED)",
      "gl.texParameteri(getDesktopGL('TEXTURE_2D'), gl.TEXTURE_WRAP_R, getDesktopGL('CLAMP_TO_BORDER'))",
    ]);
  }

  for (var ii = 0; ii < tests.length; ++ii) {
    TestEval(tests[ii]);
    wtu.glErrorShouldBe(gl, gl.INVALID_ENUM, tests[ii] + " should return INVALID_ENUM.");
  }
  if (contextVersion >= 2) {
    var uniformBlockProgram = wtu.loadUniformBlockProgram(gl);
    gl.linkProgram(uniformBlockProgram);
    shouldBe('gl.getProgramParameter(uniformBlockProgram, gl.LINK_STATUS)', 'true');
    shouldBe('gl.getError()', 'gl.NO_ERROR');
    gl.getActiveUniformBlockParameter(uniformBlockProgram, 0, getDesktopGL('UNIFORM_BLOCK_NAME_LENGTH'));
    shouldBe('gl.getError()', 'gl.INVALID_ENUM');
  }
}

debug("");
var successfullyParsed = true;

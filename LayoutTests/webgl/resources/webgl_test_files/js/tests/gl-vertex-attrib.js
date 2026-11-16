/*
Copyright (c) 2019 The Khronos Group Inc.
Use of this source code is governed by an MIT-style license that can be
found in the LICENSE.txt file.
*/

// This test relies on the surrounding web page defining a variable
// "contextVersion" which indicates what version of WebGL it's running
// on -- 1 for WebGL 1.0, 2 for WebGL 2.0, etc.

"use strict";
description("This test ensures WebGL implementations vertexAttrib can be set and read.");

const DRNG = getDrng();

debug("");
debug("Canvas.getContext");

var wtu = WebGLTestUtils;
var gl = wtu.create3DContext("canvas", undefined, contextVersion);
if (!gl) {
  testFailed("context does not exist");
} else {
  testPassed("context exists");

  // -

  debug("");
  debug("# Checking round-tripping of easy values through gl.vertexAttrib[1-4]*");

  let numVertexAttribs = gl.getParameter(gl.MAX_VERTEX_ATTRIBS);
  for (var ii = 0; ii < numVertexAttribs; ++ii) {
    gl.vertexAttrib1fv(ii, [1]);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib1fv(ii, new Float32Array([-1]));
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '-1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib2fv(ii, [1, 2]);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '2');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib2fv(ii, new Float32Array([1, -2]));
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '-2');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib3fv(ii, [1, 2, 3]);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '2');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '3');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib3fv(ii, new Float32Array([1, -2, 3]));
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '-2');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '3');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib4fv(ii, [1, 2, 3, 4]);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '2');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '3');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '4');

    gl.vertexAttrib4fv(ii, new Float32Array([1, 2, -3, 4]));
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '2');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '-3');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '4');

    gl.vertexAttrib1f(ii, 5);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '5');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib2f(ii, 6, 7);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '6');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '7');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '0');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib3f(ii, 7, 8, 9);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '7');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '8');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '9');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '1');

    gl.vertexAttrib4f(ii, 6, 7, 8, 9);
    shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Float32Array');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '6');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '7');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '8');
    shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '9');

    if (contextVersion > 1) {
      gl.vertexAttribI4i(ii, -1, 0, 1, 2);
      shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Int32Array');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '-1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '0');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '2');

      gl.vertexAttribI4ui(ii, 0, 1, 2, 3);
      shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Uint32Array');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '0');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '2');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '3');

      gl.vertexAttribI4iv(ii, [-1, 0, 1, 2]);
      shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Int32Array');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '-1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '0');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '2');

      gl.vertexAttribI4iv(ii, new Int32Array([1, 0, -1, 2]));
      shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Int32Array');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '0');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '-1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '2');

      gl.vertexAttribI4uiv(ii, [0, 1, 2, 3]);
      shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Uint32Array');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '0');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '2');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '3');

      gl.vertexAttribI4uiv(ii, new Uint32Array([0, 2, 1, 3]));
      shouldBeType('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)', 'Uint32Array');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[0]', '0');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[1]', '2');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[2]', '1');
      shouldBe('gl.getVertexAttrib(' + ii + ', gl.CURRENT_VERTEX_ATTRIB)[3]', '3');
    }
  }
  wtu.glErrorShouldBe(gl, gl.NO_ERROR);

  debug("");
  debug("# Checking out-of-range vertexAttrib index");
  gl.getVertexAttrib(numVertexAttribs, gl.CURRENT_VERTEX_ATTRIB);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib1fv(numVertexAttribs, [1]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib1fv(numVertexAttribs, new Float32Array([-1]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib2fv(numVertexAttribs, [1, 2]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib2fv(numVertexAttribs, new Float32Array([1, -2]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib3fv(numVertexAttribs, [1, 2, 3]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib3fv(numVertexAttribs, new Float32Array([1, -2, 3]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib4fv(numVertexAttribs, [1, 2, 3, 4]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib4fv(numVertexAttribs, new Float32Array([1, 2, -3, 4]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib1f(numVertexAttribs, 5);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib2f(numVertexAttribs, 6, 7);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib3f(numVertexAttribs, 7, 8, 9);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib4f(numVertexAttribs, 6, 7, 8, 9);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  if (contextVersion > 1) {
    gl.vertexAttribI4i(numVertexAttribs, -1, 0, 1, 2);
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4ui(numVertexAttribs, 0, 1, 2, 3);
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4iv(numVertexAttribs, [-1, 0, 1, 2]);
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4iv(numVertexAttribs, new Int32Array([1, 0, -1, 2]));
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4uiv(numVertexAttribs, [0, 1, 2, 3]);
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4uiv(numVertexAttribs, new Uint32Array([0, 2, 1, 3]));
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);
  }

  debug("");
  debug("Checking invalid array lengths");
  numVertexAttribs = numVertexAttribs - 1;
  gl.vertexAttrib1fv(numVertexAttribs, []);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib1fv(numVertexAttribs, new Float32Array([]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib2fv(numVertexAttribs, [1]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib2fv(numVertexAttribs, new Float32Array([1]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib3fv(numVertexAttribs, [1, 2]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib3fv(numVertexAttribs, new Float32Array([1, -2]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib4fv(numVertexAttribs, [1, 2, 3]);
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  gl.vertexAttrib4fv(numVertexAttribs, new Float32Array([1, 2, -3]));
  wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

  if (contextVersion > 1) {
    gl.vertexAttribI4iv(numVertexAttribs, [-1, 0, 1]);
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4iv(numVertexAttribs, new Int32Array([1, 0, -1]));
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4uiv(numVertexAttribs, [0, 1, 2]);
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);

    gl.vertexAttribI4uiv(numVertexAttribs, new Uint32Array([0, 2, 1]));
    wtu.glErrorShouldBe(gl, gl.INVALID_VALUE);
  }

  // -

  debug("");
  debug("# Checking round-tripping of valid random values through gl.vertexAttrib[1-4]*");
  {
    const MAX_VERTEX_ATTRIBS = gl.getParameter(gl.MAX_VERTEX_ATTRIBS);

    let FUNCS = [
      { func_name: 'vertexAttrib1f' , val_count: 1, array_ctor: Float32Array },
      { func_name: 'vertexAttrib2f' , val_count: 2, array_ctor: Float32Array },
      { func_name: 'vertexAttrib3f' , val_count: 3, array_ctor: Float32Array },
      { func_name: 'vertexAttrib4f' , val_count: 4, array_ctor: Float32Array },
      { func_name: 'vertexAttrib1fv', val_count: 1, array_ctor: Float32Array },
      { func_name: 'vertexAttrib2fv', val_count: 2, array_ctor: Float32Array },
      { func_name: 'vertexAttrib3fv', val_count: 3, array_ctor: Float32Array },
      { func_name: 'vertexAttrib4fv', val_count: 4, array_ctor: Float32Array },
    ];
    if (contextVersion >= 2) {
      FUNCS.push(
        { func_name: 'vertexAttribI4i'  , val_count: 4, array_ctor: Int32Array },
        { func_name: 'vertexAttribI4iv' , val_count: 4, array_ctor: Int32Array },
        { func_name: 'vertexAttribI4ui' , val_count: 4, array_ctor: Uint32Array },
        { func_name: 'vertexAttribI4uiv', val_count: 4, array_ctor: Uint32Array },
      );
    }

    const TESTS = crossCombine(
      range(MAX_VERTEX_ATTRIBS).map(attrib_id => ({attrib_id})),
      FUNCS
    );
    debug(`TESTS.length: ${TESTS.length}`);

    TESTS.map((test, i) => {
      debug(``);
      debug(`## TESTS[${i}]`);
      //debug(`test: ${JSON.stringify(test)}`);

      const out_vals = new test.array_ctor(4);
      if (test.array_ctor == Float32Array) {
        // Nothing too hard: [-1.0M, +1.0M)
        for (const i in out_vals) {
          const f01 = DRNG.next_unorm();
          out_vals[i] = (2*f01 - 1) * 1_000_000;
        }
      } else {
        // Anything goes!
        for (const i in out_vals) {
          out_vals[i] = DRNG.next_u32();
        }
      }
      const in_vals = out_vals.slice(0, test.val_count);
      const DEFAULT_VALUES = [0, 0, 0, 1];
      for (const i in out_vals) {
        if (in_vals[i] === undefined) {
          out_vals[i] = DEFAULT_VALUES[i];
        }
      }

      let args = [test.attrib_id, ...in_vals];
      if (test.func_name.endsWith('v')) {
        args = [test.attrib_id, in_vals];
      }
      debug(`gl.${test.func_name}(${args.join(', ')})`);
      gl[test.func_name](...args);

      shouldBeType(`gl.getVertexAttrib(${test.attrib_id}, gl.CURRENT_VERTEX_ATTRIB)`, test.array_ctor.name);
      shouldBeString(`gl.getVertexAttrib(${test.attrib_id}, gl.CURRENT_VERTEX_ATTRIB)`, out_vals.toString());
    });
  }
  wtu.glErrorShouldBe(gl, gl.NO_ERROR);
}

debug("");
var successfullyParsed = true;

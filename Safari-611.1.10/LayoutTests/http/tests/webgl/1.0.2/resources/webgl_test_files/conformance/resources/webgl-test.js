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

function webglTestLog(msg) {
  if (window.console && window.console.log) {
    window.console.log(msg);
  }
  if (document.getElementById("console")) {
    var log = document.getElementById("console");
    log.innerHTML += msg + "<br>";
  }
}

function getGLErrorAsString(ctx, err) {
  if (err === ctx.NO_ERROR) {
    return "NO_ERROR";
  }
  for (var name in ctx) {
    if (ctx[name] === err) {
      return name;
    }
  }
  return "0x" + err.toString(16);
}

// Pass undefined for glError to test that it at least throws some error
function shouldGenerateGLError(ctx, glErrors, evalStr) {
  if (!glErrors.length) {
    glErrors = [glErrors];
  }
  var exception;
  try {
    eval(evalStr);
  } catch (e) {
    exception = e;
  }
  if (exception) {
    testFailed(evalStr + " threw exception " + exception);
  } else {
    var err = ctx.getError();
    var errStrs = [];
    for (var ii = 0; ii < glErrors.length; ++ii) {
      errStrs.push(getGLErrorAsString(ctx, glErrors[ii]));
    }
    var expected = errStrs.join(" or ");
    if (glErrors.indexOf(err) < 0) {
      testFailed(evalStr + " expected: " + expected + ". Was " + getGLErrorAsString(ctx, err) + ".");
    } else {
      var msg = (glErrors.length == 1) ? " generated expected GL error: " :
                                         " generated one of expected GL errors: ";
      testPassed(evalStr + msg + expected + ".");
    }
  }
}

/**
 * Tests that the first error GL returns is the specified error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number|!Array.<number>} glError The expected gl
 *        error. Multiple errors can be passed in using an
 *        array.
 * @param {string} opt_msg Optional additional message.
 */
function glErrorShouldBe(gl, glErrors, opt_msg) {
  if (!glErrors.length) {
    glErrors = [glErrors];
  }
  opt_msg = opt_msg || "";
  var err = gl.getError();
  var ndx = glErrors.indexOf(err);
  var errStrs = [];
  for (var ii = 0; ii < glErrors.length; ++ii) {
    errStrs.push(getGLErrorAsString(gl, glErrors[ii]));
  }
  var expected = errStrs.join(" or ");
  if (ndx < 0) {
    var msg = "getError expected" + ((glErrors.length > 1) ? " one of: " : ": ");
    testFailed(msg + expected +  ". Was " + getGLErrorAsString(gl, err) + " : " + opt_msg);
  } else {
    var msg = "getError was " + ((glErrors.length > 1) ? "one of: " : "expected value: ");
    testPassed(msg + expected + " : " + opt_msg);
  }
};




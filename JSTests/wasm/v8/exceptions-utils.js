// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This file is intended to be loaded by other tests to provide utility methods
// requiring natives syntax (and hence not suited for the mjsunit.js file).

function assertWasmThrows(instance, tag, values, code) {
  try {
    if (typeof code === 'function') {
      code();
    } else {
      eval(code);
    }
  } catch (e) {
    assertInstanceof(e, WebAssembly.Exception);
    assertTrue(e.is(tag));
    assertEquals(values.length, tag.type().parameters.length);
    for (let i = 0; i < values.length; ++i)
      assertEquals(values[i], e.getArg(tag, i));
    return;  // Success.
  }
  throw new MjsUnitAssertionError('Did not throw expected <' + tag +
                                  '> with values: ' + values);
}

// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-typedarray
description: >
  Error when a TypedArray is created from another TypedArray with the same
  element-type and SpeciesConstructor causes the "source" array to go
  out-of-bounds.
includes: [testTypedArray.js, compareArray.js]
features: [TypedArray, Symbol.species, resizable-arraybuffer]
---*/

// If the host chooses to throw as allowed by the specification, the observed
// behavior will be identical to the case where `ArrayBuffer.prototype.resize`
// has not been implemented. The following assertion prevents this test from
// passing in runtimes which have not implemented the method.
assert.sameValue(typeof ArrayBuffer.prototype.resize, 'function');

testWithTypedArrayConstructors(function(TA) {
  var BPE = TA.BYTES_PER_ELEMENT;
  var ab = new ArrayBuffer(BPE * 4, {maxByteLength: BPE * 5});
  var speciesConstructor = Object.defineProperty(function(){}.bind(), 'prototype', {
    get: function() {
      return null;
    }
  });
  var onGetSpecies;
  ab.constructor = Object.defineProperty({}, Symbol.species, {
    get: function() {
      onGetSpecies();
      return speciesConstructor;
    }
  });
  var source = new TA(ab, BPE);
  var expected = [10, 20, 30];

  source[0] = 10;
  source[1] = 20;
  source[2] = 30;

  onGetSpecies = function() {
    try {
      ab.resize(BPE * 5);
      expected = [10, 20, 30, 0];
    } catch (_) {}
  };

  assert.sameValue((new TA(source)).join(','), expected.join(','));
  assert(compareArray(new TA(source), expected), 'following grow');

  onGetSpecies = function() {
    try {
      ab.resize(BPE * 3);
      expected = [10, 20];
    } catch (_) {}
  };

  assert(compareArray(new TA(source), expected), 'following shrink (within bounds)');

  onGetSpecies = function() {
    try {
      ab.resize(BPE);
      expected = [];
    } catch (_) {}
  };

  assert(compareArray(new TA(source), expected), 'following shrink (on boundary)');

  // `assert.throws` cannot be used in this case because the expected error
  // is derived only after the constructor is invoked.
  var expectedError;
  var actualError;
  onGetSpecies = function() {
    try {
      ab.resize(0);
      expectedError = TypeError;
    } catch (_) {
      expectedError = Test262Error;
    }
  };
  try {
    new TA(source);
    throw new Test262Error('the operation completed successfully');
  } catch (caught) {
    actualError = caught;
  }

  assert.sameValue(actualError.constructor, expectedError);
});

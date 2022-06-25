// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.findlast
description: Instance buffer can be resized during iteration
includes: [testTypedArray.js, compareArray.js]
features: [TypedArray, resizable-arraybuffer]
---*/

// If the host chooses to throw as allowed by the specification, the observed
// behavior will be identical to the case where `ArrayBuffer.prototype.resize`
// has not been implemented. The following assertion prevents this test from
// passing in runtimes which have not implemented the method.
assert.sameValue(typeof ArrayBuffer.prototype.resize, 'function');

testWithTypedArrayConstructors(function(TA) {
  var BPE = TA.BYTES_PER_ELEMENT;
  var buffer = new ArrayBuffer(BPE * 3, {maxByteLength: BPE * 4});
  var sample = new TA(buffer);
  var secondElement, expectedElements, expectedIndices, expectedArrays;
  var elements, indices, arrays, result;

  elements = [];
  indices = [];
  arrays = [];
  result = sample.findLast(function(element, index, array) {
    if (elements.length === 0) {
      try {
        buffer.resize(BPE);
        secondElement = undefined;
        expectedElements = [0];
        expectedIndices = [0];
        expectedArrays = [sample];
      } catch (_) {
        secondElement = 0;
        expectedElements = [0, 0, 0];
        expectedIndices = [2, 1, 0];
        expectedArrays = [sample, sample, sample];
      }
    }

    elements.push(element);
    indices.push(index);
    arrays.push(array);
    return false;
  });

  assert.compareArray(elements, [0, secondElement, 0], 'elements (shrink)');
  assert.compareArray(indices, [2, 1, 0], 'indices (shrink)');
  assert.compareArray(arrays, [sample, sample, sample], 'arrays (shrink)');
  assert.sameValue(result, undefined, 'result (shrink)');

  elements = [];
  indices = [];
  arrays = [];
  result = sample.findLast(function(element, index, array) {
    if (elements.length === 0) {
      try {
        buffer.resize(4 * BPE);
      } catch (_) {}
    }

    elements.push(element);
    indices.push(index);
    arrays.push(array);
    return false;
  });

  assert.compareArray(elements, expectedElements, 'elements (grow)');
  assert.compareArray(indices, expectedIndices, 'indices (grow)');
  assert.compareArray(arrays, expectedArrays, 'arrays (grow)');
  assert.sameValue(result, undefined, 'result (grow)');
});

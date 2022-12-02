//@ requireOptions("--useResizableArrayBuffer=1", "--useAtomicsWaitAsync=1")
// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rab-gsab --allow-natives-syntax

"use strict";

load("./resources/v8-mjsunit.js", "caller relative");
load("./resources/v8-typedarray-helpers.js", "caller relative");

(function AtomicsWait() {
  const gsab = CreateGrowableSharedArrayBuffer(16, 32);
  const i32a = new Int32Array(gsab);

  const workerScript = `
      $262.agent.receiveBroadcast(function(buffer) {
        const i32a = new Int32Array(buffer, 0);
        try {
          const result = Atomics.wait(i32a, 0, 0, 5000);
          if (result !== 'ok')
            throw new Error(result);
          $262.agent.report("PASS");
        } catch (e) {
          $262.agent.report("FAIL");
        } finally {
          $262.agent.leaving();
        }
      });
    `;

  $.agent.start(workerScript);
  $262.agent.broadcast(gsab);

  // Spin until the worker is waiting on the futex.
  while (waiterListSize(i32a, 0) != 1) {}

  Atomics.notify(i32a, 0, 1);
  while (true) {
      let report = $262.agent.getReport();
      if (report === null) {
          $262.agent.sleep(1);
          continue;
      }
      assertEquals(report, "PASS");
      break;
  }
})();

(function AtomicsWaitAfterGrowing() {
  // Test that Atomics.wait works on indices that are valid only after growing.
  const gsab = CreateGrowableSharedArrayBuffer(4 * 4, 8 * 4);
  const i32a = new Int32Array(gsab);
  gsab.grow(6 * 4);
  const index = 5;

  const workerScript = `
      $262.agent.receiveBroadcast(function(buffer) {
        const i32a = new Int32Array(buffer, 0);
        try {
          const result = Atomics.wait(i32a, 5, 0, 5000);
          if (result !== 'ok')
            throw new Error(result);
          $262.agent.report("PASS");
        } catch (e) {
          $262.agent.report("FAIL");
        } finally {
          $262.agent.leaving();
        }
      });
    `;

  $.agent.start(workerScript);
  $262.agent.broadcast(gsab);

  // Spin until the worker is waiting on the futex.
  while (waiterListSize(i32a, index) != 1) {}

  Atomics.notify(i32a, index, 1);
  while (true) {
      let report = $262.agent.getReport();
      if (report === null) {
          $262.agent.sleep(1);
          continue;
      }
      assertEquals(report, "PASS");
      break;
  }
})();

(function AtomicsWaitAsync() {
  for (let ctor of [Int32Array, BigInt64Array, MyBigInt64Array]) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab, 0);

    const initialValue = false;  // Can be converted to both Number and BigInt.
    const result = Atomics.waitAsync(lengthTracking, 0, initialValue);

    let wokeUp = false;
    result.value.then(
     (value) => { assertEquals("ok", value); wokeUp = true; },
     () => { assertUnreachable(); });

    assertEquals(true, result.async);

    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);

    const notifyReturnValue = Atomics.notify(lengthTracking, 0, 1);
    assertEquals(1, notifyReturnValue);

    function continuation() {
      assertTrue(wokeUp);
    }

    setTimeout(continuation, 0);
  }
})();

(function AtomicsWaitAsyncAfterGrowing() {
  for (let ctor of [Int32Array, BigInt64Array, MyBigInt64Array]) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab, 0);
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    const index = 5;

    const initialValue = false;  // Can be converted to both Number and BigInt.
    const result = Atomics.waitAsync(lengthTracking, index, initialValue);

    let wokeUp = false;
    result.value.then(
     (value) => { assertEquals("ok", value); wokeUp = true; },
     () => { assertUnreachable(); });

    assertEquals(true, result.async);

    const notifyReturnValue = Atomics.notify(lengthTracking, index, 1);
    assertEquals(1, notifyReturnValue);

    function continuation() {
      assertTrue(wokeUp);
    }

    setTimeout(continuation, 0);
  }
})();

(function AtomicsWaitFailWithWrongArrayTypes() {
  const gsab = CreateGrowableSharedArrayBuffer(400, 800);

  const i8a = new Int8Array(gsab);
  const i16a = new Int16Array(gsab);
  const ui8a = new Uint8Array(gsab);
  const ui8ca = new Uint8ClampedArray(gsab);
  const ui16a = new Uint16Array(gsab);
  const ui32a = new Uint32Array(gsab);
  const f32a = new Float32Array(gsab);
  const f64a = new Float64Array(gsab);
  const myui8 = new MyUint8Array(gsab);
  const bui64 = new BigUint64Array(gsab);

  [i8a, i16a, ui8a, ui8ca, ui16a, ui32a, f32a, f64a, myui8, bui64].forEach(
      function(ta) {
        // Can be converted both to Number and BigInt.
        const exampleValue = false;
        assertThrows(() => { Atomics.wait(ta, 0, exampleValue); },
                     TypeError);
        assertThrows(() => { Atomics.notify(ta, 0, 1); },
                     TypeError);
        assertThrows(() => { Atomics.waitAsync(ta, 0, exampleValue); },
                     TypeError);
      });
})();

(function TestAtomics() {
  for (let ctor of intCtors) {
    const gsab = CreateGrowableSharedArrayBuffer(4 * ctor.BYTES_PER_ELEMENT,
                                                 8 * ctor.BYTES_PER_ELEMENT);
    const lengthTracking = new ctor(gsab, 0);
    TestAtomicsOperations(lengthTracking, 0);

    AssertAtomicsOperationsThrow(lengthTracking, 4, RangeError);
    gsab.grow(6 * ctor.BYTES_PER_ELEMENT);
    TestAtomicsOperations(lengthTracking, 4);
  }
})();

(function AtomicsFailWithNonIntegerArray() {
  const gsab = CreateGrowableSharedArrayBuffer(400, 800);

  const ui8ca = new Uint8ClampedArray(gsab);
  const f32a = new Float32Array(gsab);
  const f64a = new Float64Array(gsab);
  const mf32a = new MyFloat32Array(gsab);

  [ui8ca, f32a, f64a, mf32a].forEach((ta) => {
      AssertAtomicsOperationsThrow(ta, 0, TypeError); });
})();

/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description =
  'Test out-of-memory conditions creating large mappable/mappedAtCreation buffers.';
import { kUnitCaseParamsBuilder } from '../../../../common/framework/params_builder.js';
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { kBufferUsages } from '../../../capability_info.js';
import { GPUTest } from '../../../gpu_test.js';
import { kMaxSafeMultipleOf8 } from '../../../util/math.js';

const oomAndSizeParams = kUnitCaseParamsBuilder
  .combine('oom', [false, true])
  .expand('size', ({ oom }) => {
    return oom
      ? [
          kMaxSafeMultipleOf8,
          0x20_0000_0000, // 128 GB
        ]
      : [16];
  });

export const g = makeTestGroup(GPUTest);

g.test('mapAsync')
  .desc(
    `Test creating a large mappable buffer should produce an out-of-memory error if allocation fails.
  - The resulting buffer is an error buffer, so mapAsync rejects and produces a validation error.
  - Calling getMappedRange should throw an OperationError because the buffer is not in the mapped state.
  - unmap() doesn't throw an error even if mapping failed, and otherwise should detach the ArrayBuffer.
`
  )
  .params(
    oomAndSizeParams //
      .beginSubcases()
      .combine('write', [false, true])
      .combine('unmapBeforeResolve', [false, true])
  )
  .fn(async t => {
    const { oom, write, size, unmapBeforeResolve } = t.params;

    const buffer = t.expectGPUError(
      'out-of-memory',
      () =>
        t.device.createBuffer({
          size,
          usage: write ? GPUBufferUsage.MAP_WRITE : GPUBufferUsage.MAP_READ,
        }),
      oom
    );

    let promise;
    // Should be a validation error since the buffer is invalid.
    // Unmap abort error shouldn't cause a validation error.
    t.expectValidationError(() => {
      promise = buffer.mapAsync(write ? GPUMapMode.WRITE : GPUMapMode.READ);
    }, oom);

    if (oom) {
      if (unmapBeforeResolve) {
        // Should reject with abort error because buffer will be unmapped
        // before validation check finishes.
        t.shouldReject('AbortError', promise);
      } else {
        // Should also reject in addition to the validation error.
        t.shouldReject('OperationError', promise);

        // Wait for validation error before unmap to ensure validation check
        // ends before unmap.
        try {
          await promise;
          throw new Error('The promise should be rejected.');
        } catch {
          // Should cause an exception because the promise should be rejected.
        }
      }

      // Should throw an OperationError because the buffer is not mapped.
      // Note: not a RangeError because the state of the buffer is checked first.
      t.shouldThrow('OperationError', () => {
        buffer.getMappedRange();
      });

      // Should't be a validation error even if the buffer failed to be mapped.
      buffer.unmap();
    } else {
      await promise;
      const arraybuffer = buffer.getMappedRange();
      t.expect(arraybuffer.byteLength === size);
      buffer.unmap();
      t.expect(arraybuffer.byteLength === 0, 'Mapping should be detached');
    }
  });

g.test('mappedAtCreation')
  .desc(
    `Test creating a very large buffer mappedAtCreation buffer should throw a RangeError only
     because such a large allocation cannot be created when we initialize an active buffer mapping.
`
  )
  .params(
    oomAndSizeParams //
      .beginSubcases()
      .combine('usage', kBufferUsages)
  )
  .fn(t => {
    const { oom, usage, size } = t.params;

    const f = () => t.device.createBuffer({ mappedAtCreation: true, size, usage });

    if (oom) {
      // getMappedRange is normally valid on OOM buffers, but this one fails because the
      // (default) range is too large to create the returned ArrayBuffer.
      t.shouldThrow('RangeError', f);
    } else {
      const buffer = f();
      const mapping = buffer.getMappedRange();
      t.expect(mapping.byteLength === size, 'Mapping should be successful');
      buffer.unmap();
      t.expect(mapping.byteLength === 0, 'Mapping should be detached');
    }
  });

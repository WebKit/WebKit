/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Memory Synchronization Tests for multiple buffers: read before write, read after write, and write after write.

- Create multiple src buffers and initialize it to 0, wait on the fence to ensure the data is initialized.
Write Op: write a value (say 1) into the src buffer via render pass, copmute pass, copy, write buffer, etc.
Read Op: read the value from the src buffer and write it to dst buffer via render pass (vertex, index, indirect input, uniform, storage), compute pass, copy etc.
Wait on another fence, then call expectContents to verify the dst buffer value.
  - x= write op: {storage buffer in {compute, render, render-via-bundle}, t2b copy dst, b2b copy dst, writeBuffer}
  - x= read op: {index buffer, vertex buffer, indirect buffer (draw, draw indexed, dispatch), uniform buffer, {readonly, readwrite} storage buffer in {compute, render, render-via-bundle}, b2b copy src, b2t copy src}
  - x= read-write sequence: {read then write, write then read, write then write}
  - x= op context: {queue, command-encoder, compute-pass-encoder, render-pass-encoder, render-bundle-encoder}, x= op boundary: {queue-op, command-buffer, pass, execute-bundles, render-bundle}
    - Not every context/boundary combinations are valid. We have the checkOpsValidForContext func to do the filtering.
  - If two writes are in the same passes, render result has loose guarantees.

TODO: Tests with more than one buffer to try to stress implementations a little bit more.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';

import { BufferSyncTest } from './buffer_sync_test.js';

export const g = makeTestGroup(BufferSyncTest);

g.test('rw')
  .desc(
    `
    Perform a 'read' operations on multiple buffers, followed by a 'write' operation.
    Operations are separated by a 'boundary' (pass, encoder, queue-op, etc.).
    Test that the results are synchronized.
    The read should not see the contents written by the subsequent write.`
  )
  .unimplemented();

g.test('wr')
  .desc(
    `
    Perform a 'write' operation on on multiple buffers, followed by a 'read' operation.
    Operations are separated by a 'boundary' (pass, encoder, queue-op, etc.).
    Test that the results are synchronized.
    The read should see exactly the contents written by the previous write.`
  )
  .unimplemented();

g.test('ww')
  .desc(
    `
    Perform a 'first' write operation on multiple buffers, followed by a 'second' write operation.
    Operations are separated by a 'boundary' (pass, encoder, queue-op, etc.).
    Test that the results are synchronized.
    The second write should overwrite the contents of the first.`
  )
  .unimplemented();

/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Atomically read, and and store value.

 * Load the original value pointed to by atomic_ptr.
 * Obtains a new value by anding with the value v.
 * Store the new value using atomic_ptr.

Returns the original value stored in the atomic object.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('stage')
  .specURL('https://www.w3.org/TR/WGSL/#atomic-rmw')
  .desc(
    `
Atomic built-in functions must not be used in a vertex shader stage.
`
  )
  .params(u => u.combine('stage', ['fragment', 'vertex', 'compute']))
  .unimplemented();

g.test('and')
  .specURL('https://www.w3.org/TR/WGSL/#atomic-rmw')
  .desc(
    `
SC is storage or workgroup
T is i32 or u32

fn atomicAnd(atomic_ptr: ptr<SC, atomic<T>, read_write>, v: T) -> T
`
  )
  .params(u => u.combine('SC', ['storage', 'uniform']).combine('T', ['i32', 'u32']))
  .unimplemented();

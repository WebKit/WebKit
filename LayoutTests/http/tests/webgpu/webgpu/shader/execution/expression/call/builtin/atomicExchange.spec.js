/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Atomically stores the value v in the atomic object pointed to atomic_ptr and returns the original value stored in the atomic object.
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

g.test('exchange')
  .specURL('https://www.w3.org/TR/WGSL/#atomic-rmw')
  .desc(
    `
SC is storage or workgroup
T is i32 or u32

fn atomicExchange(atomic_ptr: ptr<SC, atomic<T>, read_write>, v: T) -> T
`
  )
  .params(u => u.combine('SC', ['storage', 'uniform']).combine('T', ['i32', 'u32']))
  .unimplemented();

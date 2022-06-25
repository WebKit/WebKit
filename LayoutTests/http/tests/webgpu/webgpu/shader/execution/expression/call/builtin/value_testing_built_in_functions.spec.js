/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `WGSL execution test. Section: Value-testing built-in functions`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';

export const g = makeTestGroup(GPUTest);

g.test('value_testing_builtin_functions,runtime_sized_array_length')
  .uniqueId('8089b54fa4eeaa0b')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#value-testing-builtin-functions')
  .desc(
    `
runtime-sized array length:
e: ptr<storage,array<T>> arrayLength(e): u32 Returns the number of elements in the runtime-sized array. (OpArrayLength, but the implementation has to trace back to get the pointer to the enclosing struct.)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u => u.combine('placeHolder1', ['placeHolder2', 'placeHolder3']))
  .unimplemented();

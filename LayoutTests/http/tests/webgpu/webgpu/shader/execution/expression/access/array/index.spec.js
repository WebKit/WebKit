/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution Tests for array indexing expressions
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import { Type, array, f32 } from '../../../../../util/conversion.js';

import { allInputSources, basicExpressionBuilder, run } from '../../expression.js';

export const g = makeTestGroup(GPUTest);

g.test('concrete_scalar').
specURL('https://www.w3.org/TR/WGSL/#array-access-expr').
desc(`Test indexing of an array of concrete scalars`).
params((u) =>
u.
combine(
  'inputSource',
  // 'uniform' address space requires array stride to be multiple of 16 bytes
  allInputSources.filter((s) => s !== 'uniform')
).
combine('elementType', ['i32', 'u32', 'f32', 'f16']).
combine('indexType', ['i32', 'u32'])
).
beforeAllSubcases((t) => {
  if (t.params.elementType === 'f16') {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn(async (t) => {
  const elementType = Type[t.params.elementType];
  const indexType = Type[t.params.indexType];
  const cases = [
  {
    input: [
    array(
      /* 0 */elementType.create(10),
      /* 1 */elementType.create(11),
      /* 2 */elementType.create(12)
    ),
    indexType.create(0)],

    expected: elementType.create(10)
  },
  {
    input: [
    array(
      /* 0 */elementType.create(20),
      /* 1 */elementType.create(21),
      /* 2 */elementType.create(22)
    ),
    indexType.create(1)],

    expected: elementType.create(21)
  },
  {
    input: [
    array(
      /* 0 */elementType.create(30),
      /* 1 */elementType.create(31),
      /* 2 */elementType.create(32)
    ),
    indexType.create(2)],

    expected: elementType.create(32)
  }];

  await run(
    t,
    basicExpressionBuilder((ops) => `${ops[0]}[${ops[1]}]`),
    [Type.array(3, elementType), indexType],
    elementType,
    t.params,
    cases
  );
});

g.test('abstract_scalar').
specURL('https://www.w3.org/TR/WGSL/#array-access-expr').
desc(`Test indexing of an array of scalars`).
params((u) =>
u.
combine('elementType', ['abstract-int', 'abstract-float']).
combine('indexType', ['i32', 'u32'])
).
fn(async (t) => {
  const elementType = Type[t.params.elementType];
  const indexType = Type[t.params.indexType];
  const cases = [
  {
    input: [
    array(
      /* 0 */elementType.create(0x10_00000000),
      /* 1 */elementType.create(0x11_00000000),
      /* 2 */elementType.create(0x12_00000000)
    ),
    indexType.create(0)],

    expected: f32(0x10)
  },
  {
    input: [
    array(
      /* 0 */elementType.create(0x20_00000000),
      /* 1 */elementType.create(0x21_00000000),
      /* 2 */elementType.create(0x22_00000000)
    ),
    indexType.create(1)],

    expected: f32(0x21)
  },
  {
    input: [
    array(
      /* 0 */elementType.create(0x30_00000000),
      /* 1 */elementType.create(0x31_00000000),
      /* 2 */elementType.create(0x32_00000000)
    ),
    indexType.create(2)],

    expected: f32(0x32)
  }];

  await run(
    t,
    basicExpressionBuilder((ops) => `${ops[0]}[${ops[1]}] / 0x100000000`),
    [Type.array(3, elementType), indexType],
    Type.f32,
    { inputSource: 'const' },
    cases
  );
});

g.test('vector').
specURL('https://www.w3.org/TR/WGSL/#array-access-expr').
desc(`Test indexing of an array of vectors`).
params((u) =>
u.
combine('inputSource', allInputSources).
expand('elementType', (t) =>
t.inputSource === 'uniform' ?
['vec4i', 'vec4u', 'vec4f'] :
['vec4i', 'vec4u', 'vec4f', 'vec4h']
).
combine('indexType', ['i32', 'u32'])
).
beforeAllSubcases((t) => {
  if (t.params.elementType === 'vec4h') {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn(async (t) => {
  const elementType = Type[t.params.elementType];
  const indexType = Type[t.params.indexType];
  const cases = [
  {
    input: [
    array(
      /* 0 */elementType.create([0x10, 0x11, 0x12, 0x13]),
      /* 1 */elementType.create([0x14, 0x15, 0x16, 0x17]),
      /* 2 */elementType.create([0x18, 0x19, 0x1a, 0x1b])
    ),
    indexType.create(0)],

    expected: elementType.create([0x10, 0x11, 0x12, 0x13])
  },
  {
    input: [
    array(
      /* 0 */elementType.create([0x20, 0x21, 0x22, 0x23]),
      /* 1 */elementType.create([0x24, 0x25, 0x26, 0x27]),
      /* 2 */elementType.create([0x28, 0x29, 0x2a, 0x2b])
    ),
    indexType.create(1)],

    expected: elementType.create([0x24, 0x25, 0x26, 0x27])
  },
  {
    input: [
    array(
      /* 0 */elementType.create([0x30, 0x31, 0x32, 0x33]),
      /* 1 */elementType.create([0x34, 0x35, 0x36, 0x37]),
      /* 2 */elementType.create([0x38, 0x39, 0x3a, 0x3b])
    ),
    indexType.create(2)],

    expected: elementType.create([0x38, 0x39, 0x3a, 0x3b])
  }];

  await run(
    t,
    basicExpressionBuilder((ops) => `${ops[0]}[${ops[1]}]`),
    [Type.array(3, elementType), indexType],
    elementType,
    t.params,
    cases
  );
});
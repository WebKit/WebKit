/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const builtin = 'atan2';export const description = `
Validation tests for the ${builtin}() builtin.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';
import {
  VectorValue,
  kFloatScalarsAndVectors,
  kConcreteIntegerScalarsAndVectors,
  scalarTypeOf,
  Type } from
'../../../../../util/conversion.js';
import { isRepresentable } from '../../../../../util/floating_point.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

import {
  fullRangeForType,
  kConstantAndOverrideStages,
  sparseMinusThreePiToThreePiRangeForType,
  stageSupportsType,
  unique,
  validateConstOrOverrideBuiltinEval } from
'./const_override_validation.js';

export const g = makeTestGroup(ShaderValidationTest);

const kValuesTypes = objectsToRecord(kFloatScalarsAndVectors);

g.test('values').
desc(
  `
Validates that constant evaluation and override evaluation of ${builtin}() rejects invalid values
`
).
params((u) =>
u.
combine('stage', kConstantAndOverrideStages).
combine('type', keysOf(kValuesTypes)).
filter((u) => stageSupportsType(u.stage, kValuesTypes[u.type])).
beginSubcases().
expand('y', (u) =>
unique(
  sparseMinusThreePiToThreePiRangeForType(kValuesTypes[u.type]),
  fullRangeForType(kValuesTypes[u.type], 4)
)
).
expand('x', (u) =>
unique(
  sparseMinusThreePiToThreePiRangeForType(kValuesTypes[u.type]),
  fullRangeForType(kValuesTypes[u.type], 4)
)
)
).
beforeAllSubcases((t) => {
  if (scalarTypeOf(kValuesTypes[t.params.type]) === Type.f16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const type = kValuesTypes[t.params.type];
  const expectedResult = isRepresentable(
    Math.abs(Math.atan2(Number(t.params.x), Number(t.params.y))),
    scalarTypeOf(type)
  );
  validateConstOrOverrideBuiltinEval(
    t,
    builtin,
    expectedResult,
    [type.create(t.params.y), type.create(t.params.x)],
    t.params.stage
  );
});

const kIntegerArgumentTypes = objectsToRecord([Type.f32, ...kConcreteIntegerScalarsAndVectors]);

g.test('integer_argument_y').
desc(
  `
Validates that scalar and vector integer arguments are rejected by ${builtin}()
`
).
params((u) => u.combine('type', keysOf(kIntegerArgumentTypes))).
fn((t) => {
  const yTy = kIntegerArgumentTypes[t.params.type];
  const xTy = yTy instanceof VectorValue ? Type.vec(yTy.size, Type.f32) : Type.f32;
  validateConstOrOverrideBuiltinEval(
    t,
    builtin,
    /* expectedResult */yTy === Type.f32,
    [yTy.create(1), xTy.create(1)],
    'constant'
  );
});

g.test('integer_argument_x').
desc(
  `
Validates that scalar and vector integer arguments are rejected by ${builtin}()
`
).
params((u) => u.combine('type', keysOf(kIntegerArgumentTypes))).
fn((t) => {
  const xTy = kIntegerArgumentTypes[t.params.type];
  const yTy = xTy instanceof VectorValue ? Type.vec(xTy.size, Type.f32) : Type.f32;
  validateConstOrOverrideBuiltinEval(
    t,
    builtin,
    /* expectedResult */xTy === Type.f32,
    [yTy.create(1), xTy.create(1)],
    'constant'
  );
});
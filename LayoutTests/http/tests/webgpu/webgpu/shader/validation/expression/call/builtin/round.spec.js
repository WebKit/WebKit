/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const builtin = 'round';export const description = `
Validation tests for the ${builtin}() builtin.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';
import {
  Type,
  kConcreteIntegerScalarsAndVectors,
  kConvertableToFloatScalarsAndVectors,
  scalarTypeOf } from
'../../../../../util/conversion.js';
import { fpTraitsFor } from '../../../../../util/floating_point.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

import {
  fullRangeForType,
  kConstantAndOverrideStages,
  stageSupportsType,
  unique,
  validateConstOrOverrideBuiltinEval } from
'./const_override_validation.js';

export const g = makeTestGroup(ShaderValidationTest);

const kValuesTypes = objectsToRecord(kConvertableToFloatScalarsAndVectors);

g.test('values').
desc(
  `
Validates that constant evaluation and override evaluation of ${builtin}() inputs rejects invalid values
`
).
params((u) =>
u.
combine('stage', kConstantAndOverrideStages).
combine('type', keysOf(kValuesTypes)).
filter((u) => stageSupportsType(u.stage, kValuesTypes[u.type])).
beginSubcases().
expand('value', (u) => {
  if (scalarTypeOf(kValuesTypes[u.type]).kind === 'abstract-int') {
    return fullRangeForType(kValuesTypes[u.type]);
  } else {
    const constants = fpTraitsFor(scalarTypeOf(kValuesTypes[u.type])).constants();
    return unique(fullRangeForType(kValuesTypes[u.type]), [
    constants.negative.min + 0.1,
    constants.positive.max - 0.1]
    );
  }
})
).
beforeAllSubcases((t) => {
  if (scalarTypeOf(kValuesTypes[t.params.type]) === Type.f16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const expectedResult = true; // Result should always be representable by the type
  validateConstOrOverrideBuiltinEval(
    t,
    builtin,
    expectedResult,
    [kValuesTypes[t.params.type].create(t.params.value)],
    t.params.stage
  );
});

const kIntegerArgumentTypes = objectsToRecord([Type.f32, ...kConcreteIntegerScalarsAndVectors]);

g.test('integer_argument').
desc(
  `
Validates that scalar and vector integer arguments are rejected by ${builtin}()
`
).
params((u) => u.combine('type', keysOf(kIntegerArgumentTypes))).
fn((t) => {
  const type = kIntegerArgumentTypes[t.params.type];
  validateConstOrOverrideBuiltinEval(
    t,
    builtin,
    /* expectedResult */type === Type.f32,
    [type.create(1)],
    'constant'
  );
});
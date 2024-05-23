/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const builtin = 'ldexp';export const description = `
Validation tests for the ${builtin}() builtin.
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';
import {
  Type,
  scalarTypeOf,

  kConvertableToFloatScalarsAndVectors,
  kConcreteSignedIntegerScalarsAndVectors } from

'../../../../../util/conversion.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

import {
  ConstantOrOverrideValueChecker,
  fullRangeForType,
  kConstantAndOverrideStages,
  stageSupportsType,
  validateConstOrOverrideBuiltinEval } from
'./const_override_validation.js';

export const g = makeTestGroup(ShaderValidationTest);

const kValidArgumentTypesA = objectsToRecord(kConvertableToFloatScalarsAndVectors);
const kValidArgumentTypesB = objectsToRecord([
Type.abstractInt,
Type.vec(2, Type.abstractInt),
Type.vec(3, Type.abstractInt),
Type.vec(4, Type.abstractInt),
...kConcreteSignedIntegerScalarsAndVectors]
);

function supportedSecondArgTypes(typeAKey) {
  const typeA = kValidArgumentTypesA[typeAKey];

  switch (typeA.width) {
    case 1:
      return objectsToRecord([Type.abstractInt, Type.i32]);
    default:
      return objectsToRecord([
      Type.vec(typeA.width, Type.abstractInt),
      Type.vec(typeA.width, Type.i32)]
      );
  }
}

function biasForType(type) {
  switch (type) {
    case Type.f16:
      return 15;
    case Type.f32:
      return 127;
    case Type.abstractFloat:
    case Type.abstractInt:
      return 1023;
    default:
      throw new Error(`Invalid bias type ${type}`);
  }
}

function biasRange(type) {
  const bias = biasForType(scalarTypeOf(type));
  return [-bias - 2, -bias, Math.floor(-bias * 0.5), 0, Math.floor(bias * 0.5), bias, bias + 2];
}

g.test('values').
desc(
  `
Validates that constant evaluation and override evaluation of ${builtin}() never errors
`
).
params((u) =>
u.
combine('stage', kConstantAndOverrideStages).
combine('typeA', keysOf(kValidArgumentTypesA)).
expand('typeB', (u) => keysOf(supportedSecondArgTypes(u.typeA))).
filter((u) => stageSupportsType(u.stage, kValidArgumentTypesA[u.typeA])).
filter((u) => stageSupportsType(u.stage, kValidArgumentTypesB[u.typeB])).
beginSubcases().
expand('a', (u) => fullRangeForType(kValidArgumentTypesA[u.typeA], 5)).
expand('b', (u) => biasRange(kValidArgumentTypesA[u.typeA]))
).
beforeAllSubcases((t) => {
  if (scalarTypeOf(kValidArgumentTypesA[t.params.typeA]) === Type.f16) {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const typeA = kValidArgumentTypesA[t.params.typeA];
  const bias = biasForType(scalarTypeOf(typeA));
  const scalarTypeA = scalarTypeOf(typeA);
  const vCheck = new ConstantOrOverrideValueChecker(t, scalarTypeA);

  const typeB = kValidArgumentTypesB[t.params.typeB];

  const validExponent = Number(t.params.b) <= bias + 1;

  // Should be invalid if the calculations result in values that exceed the
  // maximum representable float value for the given type.
  const a = Number(t.params.a);
  const b = Number(t.params.b);
  vCheck.checkedResult(a * Math.pow(2, b));

  // Validates ldexp(a, b) or ldexp(vecN(a), vecN(b));
  validateConstOrOverrideBuiltinEval(
    t,
    builtin,
    validExponent && vCheck.allChecksPassed(),
    [typeA.create(t.params.a), typeB.create(t.params.b)],
    t.params.stage
  );
});

const kArgCases = {
  good: '(vec3(0), vec3(1))',
  bad_no_parens: '',
  // Bad number of args
  bad_0args: '()',
  bad_1arg: '(vec3(0))',
  bad_3arg: '(vec3(0), vec3(1), vec3(2))',
  // Bad value combinations
  bad_vec_scalar: '(vec3(0), 1)',
  bad_scalar_vec: '(0, vec3(1))',
  bad_vec_sizes: '(vec3(0), vec2(1))',
  // Bad value for arg 0
  bad_0bool: '(false, vec3(1))',
  bad_0array: '(array(1.1,2.2), vec3(1))',
  bad_0struct: '(modf(2.2), vec3(1))',
  bad_0int: '(0i, 1i)',
  bad_0uint: '(0u, 1i)',
  bad_0vec2i: '(vec2i(0), vec2i(1))',
  bad_0vec3i: '(vec3i(0), vec3i(1))',
  bad_0vec4i: '(vec4i(0), vec4i(1))',
  bad_0vec2u: '(vec2u(0), vec2i(1))',
  bad_0vec3u: '(vec3u(0), vec3i(1))',
  bad_0vec4u: '(vec4u(0), vec4i(1))',
  // Bad value type for arg 1
  bad_1bool: '(vec3(0), true)',
  bad_1array: '(vec3(0), array(1.1,2.2))',
  bad_1struct: '(vec3(0), modf(2.2))',
  bad_1f32: '(0f, 1f)',
  bad_1f16: '(0f, 1h)',
  bad_1uint: '(0f, 1u)',
  bad_1vec2f: '(vec2f(0), vec2f(1))',
  bad_1vec3f: '(vec3f(0), vec3f(1))',
  bad_1vec4f: '(vec4f(0), vec4f(1))',
  bad_1vec2h: '(vec2f(0), vec2h(1))',
  bad_1vec3h: '(vec3f(0), vec3h(1))',
  bad_1vec4h: '(vec4f(0), vec4h(1))',
  bad_1vec2u: '(vec2f(0), vec2u(1))',
  bad_1vec3u: '(vec3f(0), vec3u(1))',
  bad_1vec4u: '(vec4f(0), vec4u(1))'
};

g.test('args').
desc(`Test compilation failure of ${builtin} with variously shaped and typed arguments`).
params((u) => u.combine('arg', keysOf(kArgCases))).
fn((t) => {
  t.expectCompileResult(
    t.params.arg === 'good',
    `const c = ${builtin}${kArgCases[t.params.arg]};`
  );
});

g.test('must_use').
desc(`Result of ${builtin} must be used`).
params((u) => u.combine('use', [true, false])).
fn((t) => {
  const use_it = t.params.use ? '_ = ' : '';
  t.expectCompileResult(t.params.use, `fn f() { ${use_it}${builtin}${kArgCases['good']}; }`);
});
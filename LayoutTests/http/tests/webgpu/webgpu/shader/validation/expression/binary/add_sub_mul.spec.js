/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for add/sub/mul expressions.
`;import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../common/util/data_tables.js';
import {
  kAllScalarsAndVectors,
  kConvertableToFloatScalar,
  scalarTypeOf,
  Type,
  VectorType } from
'../../../../util/conversion.js';
import { ShaderValidationTest } from '../../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

// A list of operators tested in this file.
const kOperators = {
  add: { op: '+' },
  sub: { op: '-' },
  mul: { op: '*' }
};

// A list of scalar and vector types.
const kScalarAndVectorTypes = objectsToRecord(kAllScalarsAndVectors);

g.test('scalar_vector').
desc(
  `
  Validates that scalar and vector expressions are only accepted for compatible numeric types.
  `
).
params((u) =>
u.
combine('op', keysOf(kOperators)).
combine('lhs', keysOf(kScalarAndVectorTypes)).
combine(
  'rhs',
  // Skip vec3 and vec4 on the RHS to keep the number of subcases down.
  // vec3 + vec3 and vec4 + vec4 is tested in execution tests.
  keysOf(kScalarAndVectorTypes).filter(
    (value) => !(value.startsWith('vec3') || value.startsWith('vec4'))
  )
).
beginSubcases()
).
beforeAllSubcases((t) => {
  if (
  scalarTypeOf(kScalarAndVectorTypes[t.params.lhs]) === Type.f16 ||
  scalarTypeOf(kScalarAndVectorTypes[t.params.rhs]) === Type.f16)
  {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  const op = kOperators[t.params.op];
  const lhs = kScalarAndVectorTypes[t.params.lhs];
  const rhs = kScalarAndVectorTypes[t.params.rhs];
  const lhsElement = scalarTypeOf(lhs);
  const rhsElement = scalarTypeOf(rhs);
  const hasF16 = lhsElement === Type.f16 || rhsElement === Type.f16;
  const code = `
${hasF16 ? 'enable f16;' : ''}
const lhs = ${lhs.create(0).wgsl()};
const rhs = ${rhs.create(0).wgsl()};
const foo = lhs ${op.op} rhs;
`;

  let elementsCompatible = lhsElement === rhsElement;
  const elementTypes = [lhsElement, rhsElement];

  // Booleans are not allowed for arithmetic expressions.
  if (elementTypes.includes(Type.bool)) {
    elementsCompatible = false;

    // AbstractInt is allowed with everything but booleans which are already checked above.
  } else if (elementTypes.includes(Type.abstractInt)) {
    elementsCompatible = true;

    // AbstractFloat is allowed with AbstractInt (checked above) or float types
  } else if (elementTypes.includes(Type.abstractFloat)) {
    elementsCompatible = elementTypes.every((e) => kConvertableToFloatScalar.includes(e));
  }

  // Determine if the full type is compatible. The only invalid case is mixed vector sizes.
  let valid = elementsCompatible;
  if (lhs instanceof VectorType && rhs instanceof VectorType) {
    valid = valid && lhs.width === rhs.width;
  }

  t.expectCompileResult(valid, code);
});







const kInvalidTypes = {
  array: {
    expr: 'arr',
    control: (e) => `${e}[0]`
  },

  ptr: {
    expr: '(&u)',
    control: (e) => `*${e}`
  },

  atomic: {
    expr: 'a',
    control: (e) => `atomicLoad(&${e})`
  },

  texture: {
    expr: 't',
    control: (e) => `i32(textureLoad(${e}, vec2(), 0).x)`
  },

  sampler: {
    expr: 's',
    control: (e) => `i32(textureSampleLevel(t, ${e}, vec2(), 0).x)`
  },

  struct: {
    expr: 'str',
    control: (e) => `${e}.u`
  }
};

g.test('scalar_vector_out_of_range').
desc(
  `
  TODO: Check with a couple values in f32 or f16 that would result in out of range values on the type.
  `
).
unimplemented();

g.test('invalid_type_with_itself').
desc(
  `
  Validates that expressions are never accepted for non-scalar, non-vector, and non-matrix types.
  `
).
params((u) =>
u.
combine('op', keysOf(kOperators)).
combine('type', keysOf(kInvalidTypes)).
combine('control', [true, false]).
beginSubcases()
).
fn((t) => {
  const op = kOperators[t.params.op];
  const type = kInvalidTypes[t.params.type];
  const expr = t.params.control ? type.control(type.expr) : type.expr;
  const code = `
@group(0) @binding(0) var t : texture_2d<f32>;
@group(0) @binding(1) var s : sampler;
@group(0) @binding(2) var<storage, read_write> a : atomic<i32>;

struct S { u : u32 }

var<private> u : u32;
var<private> m : mat2x2f;
var<private> arr : array<i32, 4>;
var<private> str : S;

@compute @workgroup_size(1)
fn main() {
  let foo = ${expr} ${op.op} ${expr};
}
`;

  t.expectCompileResult(t.params.control, code);
});
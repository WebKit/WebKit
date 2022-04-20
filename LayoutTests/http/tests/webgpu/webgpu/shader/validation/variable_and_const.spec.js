/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Positive and negative validation tests for variable and const.

TODO: Find a better way to test arrays than using a single arbitrary size. [1]
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';

import { ShaderValidationTest } from './shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTestTypes = [
  'f32',
  'i32',
  'u32',
  'bool',
  'vec2<f32>',
  'vec2<i32>',
  'vec2<u32>',
  'vec2<bool>',
  'vec3<f32>',
  'vec3<i32>',
  'vec3<u32>',
  'vec3<bool>',
  'vec4<f32>',
  'vec4<i32>',
  'vec4<u32>',
  'vec4<bool>',
  'mat2x2<f32>',
  'mat2x3<f32>',
  'mat2x4<f32>',
  'mat3x2<f32>',
  'mat3x3<f32>',
  'mat3x4<f32>',
  'mat4x2<f32>',
  'mat4x3<f32>',
  'mat4x4<f32>',
  // [1]: 12 is a random number here. find a solution to replace it.
  'array<f32, 12>',
  'array<i32, 12>',
  'array<u32, 12>',
  'array<bool, 12>',
];

g.test('initializer_type')
  .desc(
    `
  If present, the initializer's type must match the store type of the variable.
  Testing scalars, vectors, and matrices of every dimension and type.
  TODO: add test for: structs - arrays of vectors and matrices - arrays of different length
`
  )
  .params(u =>
    u
      .combine('variableOrConstant', ['var', 'let'])
      .beginSubcases()
      .combine('lhsType', kTestTypes)
      .combine('rhsType', kTestTypes)
  )
  .fn(t => {
    const { variableOrConstant, lhsType, rhsType } = t.params;

    const code = `
      @stage(fragment)
      fn main() {
        ${variableOrConstant} a : ${lhsType} = ${rhsType}();
      }
    `;

    const expectation = lhsType === rhsType;
    t.expectCompileResult(expectation, code);
  });

g.test('io_shareable_type')
  .desc(
    `
  The following types are IO-shareable:
  - numeric scalar types
  - numeric vector types
  - Matrix Types
  - Array Types if its element type is IO-shareable, and the array is not runtime-sized
  - Structure Types if all its members are IO-shareable

  As a result these are not IO-shareable:
  - boolean
  - vector of booleans
  - array of booleans
  - matrix of booleans
  - array runtime sized -> cannot be used outside of a struct, so no cts for this
  - struct with bool component
  - struct with runtime array

  Control case: 'private' is used to make sure when only the storage class changes, the shader
  becomes invalid and nothing else is wrong.
  TODO: add test for structs:
  - struct with bool component
  - struct with runtime array`
  )
  .params(u => u.combine('storageClass', ['in', 'out', 'private']).combine('type', kTestTypes))
  .fn(t => {
    const { storageClass, type } = t.params;

    let code;
    if (`${storageClass}` === 'in') {
      code = `
        struct MyInputs {
          @location(0) @interpolate(flat) a : ${type}
        };

        @stage(fragment)
        fn main(inputs : MyInputs) {
        }
      `;
    } else if (`${storageClass}` === 'out') {
      code = `
        struct MyOutputs {
          @location(0) a : ${type}
        };

        @stage(fragment)
        fn main() -> MyOutputs {
          return MyOutputs();
        }
      `;
    } else {
      code = `
      var<${storageClass}> a : ${type} = ${type}();

      @stage(fragment)
      fn main() {
      }
      `;
    }

    const expectation =
      storageClass === 'private' ||
      (type.indexOf('bool') === -1 && !type.startsWith('mat') && !type.startsWith('array'));
    t.expectCompileResult(expectation, code);
  });

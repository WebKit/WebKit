/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `Validation tests for @align`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kValidAlign = new Set([
  '',
  '@align(1)',
  '@align(4)',
  '@align(4i)',
  '@align(4u)',
  '@align(0x4)',
  '@align(4,)',
  '@align(u_val)',
  '@align(i_val)',
  '@align(i_val + 4 - 6)',
  '@align(1073741824)',
  '@\talign\t(4)',
  '@/^comment^/align/^comment^/(4)',
]);

const kInvalidAlign = new Set([
  '@malign(4)',
  '@align()',
  '@align 4)',
  '@align(4',
  '@align(4, 2)',
  '@align(4,)',
  '@align(3)', // Not a power of 2
  '@align(f_val)',
  '@align(1.0)',
  '@align(4f)',
  '@align(4h)',
  '@align',
  '@align(0)',
  '@align(-4)',
  '@align(2147483646)', // Not a power of 2
  '@align(2147483648)', // Larger then max i32
]);

g.test('align_parsing')
  .desc(`Test that @align is parsed correctly.`)
  .params(u => u.combine('align', new Set([...kValidAlign, ...kInvalidAlign])))
  .fn(t => {
    const v = t.params.align.replace(/\^/g, '*');
    const code = `
const i_val: i32 = 4;
const u_val: u32 = 4;
const f_val: f32 = 4.2;
struct B {
  ${v} a: i32,
}

@group(0) @binding(0)
var<uniform> uniform_buffer: B;

@fragment
fn main() -> @location(0) vec4<f32> {
  return vec4<f32>(.4, .2, .3, .1);
}`;
    t.expectCompileResult(kValidAlign.has(t.params.align), code);
  });

g.test('align_required_alignment')
  .desc('Test that the align with an invalid size is an error')
  .params(u =>
    u
      .combine('address_space', ['storage', 'uniform'])
      // These test a few cases:
      //  * 1 -- Invalid, alignment smaller then all the required alignments
      //  * alignment -- Valid, the required alignment
      //  * 32 -- Valid, an alignment larger then the required alignment.
      .combine('align', [1, 2, 'alignment', 32])
      .combine('type', [
        { name: 'i32', storage: 4, uniform: 4 },
        { name: 'u32', storage: 4, uniform: 4 },
        { name: 'f32', storage: 4, uniform: 4 },
        { name: 'f16', storage: 2, uniform: 2 },
        { name: 'atomic<i32>', storage: 4, uniform: 4 },
        { name: 'vec2<i32>', storage: 8, uniform: 8 },
        { name: 'vec2<f16>', storage: 4, uniform: 4 },
        { name: 'vec3<u32>', storage: 16, uniform: 16 },
        { name: 'vec3<f16>', storage: 8, uniform: 8 },
        { name: 'vec4<f32>', storage: 16, uniform: 16 },
        { name: 'vec4<f16>', storage: 8, uniform: 8 },
        { name: 'mat2x2<f32>', storage: 8, uniform: 8 },
        { name: 'mat3x2<f32>', storage: 8, uniform: 8 },
        { name: 'mat4x2<f32>', storage: 8, uniform: 8 },
        { name: 'mat2x2<f16>', storage: 4, uniform: 4 },
        { name: 'mat3x2<f16>', storage: 4, uniform: 4 },
        { name: 'mat4x2<f16>', storage: 4, uniform: 4 },
        { name: 'mat2x3<f32>', storage: 16, uniform: 16 },
        { name: 'mat3x3<f32>', storage: 16, uniform: 16 },
        { name: 'mat4x3<f32>', storage: 16, uniform: 16 },
        { name: 'mat2x3<f16>', storage: 8, uniform: 8 },
        { name: 'mat3x3<f16>', storage: 8, uniform: 8 },
        { name: 'mat4x3<f16>', storage: 8, uniform: 8 },
        { name: 'mat2x4<f32>', storage: 16, uniform: 16 },
        { name: 'mat3x4<f32>', storage: 16, uniform: 16 },
        { name: 'mat4x4<f32>', storage: 16, uniform: 16 },
        { name: 'mat2x4<f16>', storage: 8, uniform: 8 },
        { name: 'mat3x4<f16>', storage: 8, uniform: 8 },
        { name: 'mat4x4<f16>', storage: 8, uniform: 8 },
        { name: 'array<vec2<i32>, 2>', storage: 8, uniform: 16 },
        { name: 'array<vec4<i32>, 2>', storage: 8, uniform: 16 },
        { name: 'S', storage: 8, uniform: 16 },
      ])
      .beginSubcases()
  )
  .beforeAllSubcases(t => {
    if (t.params.type.name.includes('f16')) {
      t.selectDeviceOrSkipTestCase('shader-f16');
    }
  })
  .fn(t => {
    // While this would fail validation, it doesn't fail for any reasons related to alignment.
    // Atomics are not allowed in uniform address space as they have to be read_write.
    if (t.params.address_space === 'uniform' && t.params.type.name.startsWith('atomic')) {
      t.skip('No atomics in uniform address space');
    }

    let code = '';
    if (t.params.type.name.includes('f16')) {
      code += 'enable f16;\n';
    }

    // Testing the struct case, generate the structf
    if (t.params.type.name === 'S') {
      code += `struct S {
        a: mat4x2<f32>,          // Align 8
        b: array<vec${
          t.params.address_space === 'storage' ? 2 : 4
        }<i32>, 2>,  // Storage align 8, uniform 16
      }
      `;
    }

    let align = t.params.align;
    if (t.params.align === 'alignment') {
      // Alignment value listed in the spec
      if (t.params.address_space === 'storage') {
        align = `${t.params.type.storage}`;
      } else {
        align = `${t.params.type.uniform}`;
      }
    }

    let address_space = 'uniform';
    if (t.params.address_space === 'storage') {
      // atomics require read_write, not just the default of read
      address_space = 'storage, read_write';
    }

    code += `struct MyStruct {
      @align(${align}) a: ${t.params.type.name},
    }

    @group(0) @binding(0)
    var<${address_space}> a : MyStruct;`;

    code += `
    @fragment
    fn main() -> @location(0) vec4<f32> {
      return vec4<f32>(.4, .2, .3, .1);
    }`;

    const fails =
      // An alignment of 1 is never valid as it is smaller then all required alignments.
      t.params.align === 1 ||
      // Except for f16, 2 should also fail as being too small.
      (t.params.align === 2 && t.params.type.name !== 'f16') ||
      // An array of `vec2` in uniform will not validate because, while the alignment on the array
      // itself is fine, the `vec2` element inside the array will have the wrong alignment. Uniform
      // requires that inner vec2 to have an align 16 which can only be done by specifying `vec4`
      // instead.
      (t.params.address_space === 'uniform' && t.params.type.name.startsWith('array<vec2'));

    t.expectCompileResult(!fails, code);
  });

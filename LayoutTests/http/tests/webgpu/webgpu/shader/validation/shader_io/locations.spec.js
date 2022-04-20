/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `Validation tests for entry point user-defined IO`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

import { generateShader } from './util.js';

export const g = makeTestGroup(ShaderValidationTest);

// List of types to test against.
const kTestTypes = [
  { type: 'bool', _valid: false },
  { type: 'u32', _valid: true },
  { type: 'i32', _valid: true },
  { type: 'f32', _valid: true },
  { type: 'vec2<bool>', _valid: false },
  { type: 'vec2<u32>', _valid: true },
  { type: 'vec2<i32>', _valid: true },
  { type: 'vec2<f32>', _valid: true },
  { type: 'vec3<bool>', _valid: false },
  { type: 'vec3<u32>', _valid: true },
  { type: 'vec3<i32>', _valid: true },
  { type: 'vec3<f32>', _valid: true },
  { type: 'vec4<bool>', _valid: false },
  { type: 'vec4<u32>', _valid: true },
  { type: 'vec4<i32>', _valid: true },
  { type: 'vec4<f32>', _valid: true },
  { type: 'mat2x2<f32>', _valid: false },
  { type: 'mat2x3<f32>', _valid: false },
  { type: 'mat2x4<f32>', _valid: false },
  { type: 'mat3x2<f32>', _valid: false },
  { type: 'mat3x3<f32>', _valid: false },
  { type: 'mat3x4<f32>', _valid: false },
  { type: 'mat4x2<f32>', _valid: false },
  { type: 'mat4x3<f32>', _valid: false },
  { type: 'mat4x4<f32>', _valid: false },
  { type: 'atomic<u32>', _valid: false },
  { type: 'atomic<i32>', _valid: false },
  { type: 'array<bool,4>', _valid: false },
  { type: 'array<u32,4>', _valid: false },
  { type: 'array<i32,4>', _valid: false },
  { type: 'array<f32,4>', _valid: false },
  { type: 'MyStruct', _valid: false },
];

g.test('stage_inout')
  .desc(`Test validation of user-defined IO stage and in/out usage`)
  .params(u =>
    u
      .combine('use_struct', [true, false])
      .combine('target_stage', ['vertex', 'fragment', 'compute'])
      .combine('target_io', ['in', 'out'])
      .beginSubcases()
  )
  .fn(t => {
    const code = generateShader({
      attribute: '@location(0)',
      type: 'f32',
      stage: t.params.target_stage,
      io: t.params.target_io,
      use_struct: t.params.use_struct,
    });

    // Expect to fail for compute shaders or when used as a non-struct vertex output (since the
    // position built-in must also be specified).
    const expectation =
      t.params.target_stage === 'fragment' ||
      (t.params.target_stage === 'vertex' && (t.params.target_io === 'in' || t.params.use_struct));
    t.expectCompileResult(expectation, code);
  });

g.test('type')
  .desc(`Test validation of user-defined IO types`)
  .params(u => u.combine('use_struct', [true, false]).combineWithParams(kTestTypes).beginSubcases())
  .fn(t => {
    let code = '';

    if (t.params.type === 'MyStruct') {
      // Generate a struct that contains a valid type.
      code += 'struct MyStruct {\n';
      code += `  value : f32\n`;
      code += '};\n\n';
    }

    code += generateShader({
      attribute: '@location(0) @interpolate(flat)',
      type: t.params.type,
      stage: 'fragment',
      io: 'in',
      use_struct: t.params.use_struct,
    });

    // Expect to pass iff a valid type is used.
    t.expectCompileResult(t.params._valid, code);
  });

g.test('nesting')
  .desc(`Test validation of nested user-defined IO`)
  .params(u =>
    u
      .combine('target_stage', ['vertex', 'fragment', ''])
      .combine('target_io', ['in', 'out'])
      .beginSubcases()
  )
  .fn(t => {
    let code = '';

    // Generate a struct that contains a valid type.
    code += 'struct Inner {\n';
    code += `  @location(0) value : f32\n`;
    code += '};\n\n';
    code += 'struct Outer {\n';
    code += `  inner : Inner\n`;
    code += '};\n\n';

    code += generateShader({
      attribute: '',
      type: 'Outer',
      stage: t.params.target_stage,
      io: t.params.target_io,
      use_struct: false,
    });

    // Expect to pass only if the struct is not used for entry point IO.
    t.expectCompileResult(t.params.target_stage === '', code);
  });

g.test('duplicates')
  .desc(`Test that duplicated user-defined IO attributes are validated.`)
  .params(u =>
    u
      // Place two @location(0) attributes onto the entry point function.
      // The function:
      // - has two non-struct parameters (`p1` and `p2`)
      // - has two struct parameters each with two members (`s1{a,b}` and `s2{a,b}`)
      // - returns a struct with two members (`ra` and `rb`)
      // By default, all of these user-defined IO variables will have unique location attributes.
      .combine('first', ['p1', 's1a', 's2a', 'ra'])
      .combine('second', ['p2', 's1b', 's2b', 'rb'])
      .beginSubcases()
  )
  .fn(t => {
    const p1 = t.params.first === 'p1' ? '0' : '1';
    const p2 = t.params.second === 'p2' ? '0' : '2';
    const s1a = t.params.first === 's1a' ? '0' : '3';
    const s1b = t.params.second === 's1b' ? '0' : '4';
    const s2a = t.params.first === 's2a' ? '0' : '5';
    const s2b = t.params.second === 's2b' ? '0' : '6';
    const ra = t.params.first === 'ra' ? '0' : '1';
    const rb = t.params.second === 'rb' ? '0' : '2';
    const code = `
    struct S1 {
      @location(${s1a}) a : f32,
      @location(${s1b}) b : f32,
    };
    struct S2 {
      @location(${s2a}) a : f32,
      @location(${s2b}) b : f32,
    };
    struct R {
      @location(${ra}) a : f32,
      @location(${rb}) b : f32,
    };
    @stage(fragment)
    fn main(@location(${p1}) p1 : f32,
            @location(${p2}) p2 : f32,
            s1 : S1,
            s2 : S2,
            ) -> R {
      return R();
    }
    `;

    // The test should fail if both @location(0) attributes are on the input parameters or
    // structures, or it they are both on the output struct. Otherwise it should pass.
    const firstIsRet = t.params.first === 'ra';
    const secondIsRet = t.params.second === 'rb';
    const expectation = firstIsRet !== secondIsRet;
    t.expectCompileResult(expectation, code);
  });

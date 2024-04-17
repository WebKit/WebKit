/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/const kFn = 'unpack4xI8';export const description = `Validate ${kFn}`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf } from '../../../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

const kFeature = 'packed_4x8_integer_dot_product';
const kArgCases = {
  good_u32: '(1u)',
  good_aint: '(1)',
  bad_0args: '()',
  bad_2args: '(1u,2u)',
  bad_i32: '(1i)',
  bad_f32: '(1f)',
  bad_f16: '(1h)',
  bad_bool: '(false)',
  bad_vec2u: '(vec2u())',
  bad_vec3u: '(vec3u())',
  bad_vec4u: '(vec4u())',
  bad_array: '(array(1))',
  bad_struct: '(modf(1.1))'
};
const kGoodArgs = kArgCases['good_u32'];
const kReturnType = 'vec4i';

export const g = makeTestGroup(ShaderValidationTest);

g.test('unsupported').
desc(`Test absence of ${kFn} when ${kFeature} is not supported.`).
params((u) => u.combine('requires', [false, true])).
fn((t) => {
  t.skipIfLanguageFeatureSupported(kFeature);
  const preamble = t.params.requires ? `requires ${kFeature}; ` : '';
  const code = `${preamble}const c = ${kFn}${kGoodArgs};`;
  t.expectCompileResult(false, code);
});

g.test('supported').
desc(`Test presence of ${kFn} when ${kFeature} is supported.`).
params((u) => u.combine('requires', [false, true])).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported(kFeature);
  const preamble = t.params.requires ? `requires ${kFeature}; ` : '';
  const code = `${preamble}const c = ${kFn}${kGoodArgs};`;
  t.expectCompileResult(true, code);
});

g.test('args').
desc(`Test compilation failure of ${kFn} with various numbers of and types of arguments`).
params((u) => u.combine('arg', keysOf(kArgCases))).
beforeAllSubcases((t) => {
  if (t.params.arg === 'bad_f16') {
    t.selectDeviceOrSkipTestCase('shader-f16');
  }
}).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported(kFeature);

  let code = '';
  if (t.params.arg === 'bad_f16') {
    code += 'enable f16;\n';
  }
  code += `const c = ${kFn}${kArgCases[t.params.arg]};`;

  t.expectCompileResult(t.params.arg.startsWith('good'), code);
});

g.test('return').
desc(`Test ${kFn} return value type ${kReturnType}`).
params((u) => u.combine('type', ['vec4u', 'vec4i', 'vec4f', 'vec3i', 'vec2i', 'i32'])).
fn((t) => {
  t.expectCompileResult(
    t.params.type === kReturnType,
    `const c: ${t.params.type} = ${kFn}${kGoodArgs};`
  );
});

g.test('must_use').
desc(`Result of ${kFn} must be used`).
params((u) => u.combine('use', [true, false])).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported(kFeature);
  const use_it = t.params.use ? '_ = ' : '';
  t.expectCompileResult(t.params.use, `fn f() { ${use_it}${kFn}${kGoodArgs}; }`);
});
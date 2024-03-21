/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `Validate unpack4xU8`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf } from '../../../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

const kFeature = 'packed_4x8_integer_dot_product';
const kFn = 'unpack4xU8';
const kArgCases = {
  good: '(1u)',
  bad_0args: '()',
  bad_2args: '(1u,2u)',
  bad_0i32: '(1i)',
  bad_0f32: '(1f)',
  bad_0bool: '(false)',
  bad_0vec2u: '(vec2u())',
  bad_0vec3u: '(vec3u())',
  bad_0vec4u: '(vec4u())',
  bad_0array: '(array(1))',
  bad_0struct: '(modf(1.1))'
};
const kGoodArgs = kArgCases['good'];

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
fn((t) => {
  t.skipIfLanguageFeatureNotSupported(kFeature);
  t.expectCompileResult(t.params.arg === 'good', `const c = ${kFn}${kArgCases[t.params.arg]};`);
});

g.test('must_use').
desc(`Result of ${kFn} must be used`).
params((u) => u.combine('use', [true, false])).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported(kFeature);
  const use_it = t.params.use ? '_ = ' : '';
  t.expectCompileResult(t.params.use, `fn f() { ${use_it}${kFn}${kGoodArgs}; }`);
});
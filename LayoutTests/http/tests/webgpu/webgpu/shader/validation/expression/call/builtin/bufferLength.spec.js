/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for bufferLength
`;import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { keysOf, objectsToRecord } from '../../../../../../common/util/data_tables.js';
import { Type, elementTypeOf, kAllScalarsAndVectors } from '../../../../../util/conversion.js';
import { ShaderValidationTest } from '../../../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('must_use').
desc('Tests that the builtin has the @must_use attribute').
params((u) => u.combine('must_use', [true, false])).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const wgsl = `
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  ${t.params.must_use ? '_ = ' : ''}bufferLength(&v);
}`;

  t.expectCompileResult(t.params.must_use, wgsl);
});

const kTypes = objectsToRecord(kAllScalarsAndVectors);

g.test('return_type').
desc('Validates return type').
params((u) =>
u.combine('type', keysOf(kTypes)).filter((t) => {
  const type = kTypes[t.type];
  const eleType = elementTypeOf(type);
  return eleType !== Type.abstractInt && eleType !== Type.abstractFloat;
})
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const type = kTypes[t.params.type];
  let enables = ``;
  if (type.requiresF16()) {
    enables = `enable f16;`;
  }
  const wgsl = `
${enables}
@group(0) @binding(0) var<storage> v : buffer;
@compute @workgroup_size(1)
fn main() {
  let res : ${type.toString()} = bufferLength(&v);
}`;

  t.expectCompileResult(type === Type.u32, wgsl);
});

g.test('data_type').
desc('Validates the input parameter type').
params((u) =>
u.
combine('type', [
'unsized_ro_storage',
'unsized_storage',
'sized_ro_storage',
'sized_storage',
'sized_uniform',
'sized_workgroup',
'override_workgroup']
).
beginSubcases().
combine('ptr', [false, true])
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const wgsl = `
@group(0) @binding(0) var<storage> unsized_ro_storage : buffer;
@group(0) @binding(1) var<storage, read_write> unsized_storage : buffer;
@group(0) @binding(2) var<storage> sized_ro_storage : buffer<128>;
@group(0) @binding(3) var<storage, read_write> sized_storage : buffer<128>;
@group(0) @binding(4) var<uniform> sized_uniform : buffer<128>;
var<workgroup> sized_workgroup : buffer<128>;
override o : u32;
var<workgroup> override_workgroup : buffer<o>;

@compute @workgroup_size(1)
fn main() {
  _ = bufferLength(${t.params.ptr ? '&' : ''}${t.params.type});
}`;

  t.expectCompileResult(t.params.ptr, wgsl);
});

const kInvalidCases = {
  baseline: `&b`,
  no_args: `bufferLength()`,
  two_args: `bufferLength(&b, 0)`,
  scalar: `bufferLength(0u)`,
  ptr_to_scalar: `bufferLength(&v[0])`,
  ptr_to_array: `bufferLength(&v)`
};

g.test('invalid_cases').
desc('Test a smattering of invalid bufferLength calls').
params((u) => u.combine('test', keysOf(kInvalidCases))).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const code = `
var<workgroup> v : array<u32, 4>;
var<workgroup> b : buffer<128>;
@compute @workgroup_size(1)
fn main() {
  _ = ${kInvalidCases[t.params.test]};
}`;

  t.expectCompileResult(t.params.test === 'baseline', code);
});
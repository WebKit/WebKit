/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Validation tests for buffer types
`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kParseCases = {
  unsized: {
    code: `alias T = buffer;`,
    valid: true
  },
  literal: {
    code: `alias T = buffer<16>;`,
    valid: true
  },
  literal_negative: {
    code: `alias T = buffer<-1>;`,
    valid: false
  },
  literal_zero: {
    code: `alias T = buffer<0>;`,
    valid: false
  },
  const: {
    code: `const x = 16; alias T = buffer<x>;`,
    valid: true
  },
  const_negative: {
    code: `const x = -1; alias T = buffer<x>;`,
    valid: false
  },
  const_zero: {
    code: `const x = 0; alias T = buffer<x>;`,
    valid: false
  },
  const_function: {
    code: `const x = 16; const y = 32; alias T = buffer<min(x, y)>;`,
    valid: true
  },
  override: {
    code: `override x = 16; alias T = buffer<x>;`,
    valid: true
  },
  override_negative: {
    code: `override x = -1; alias T = buffer<x>;`,
    valid: true
  },
  override_zero: {
    code: `override x = 0; alias T = buffer<x>;`,
    valid: true
  },
  override_function: {
    code: `override x = 16; override y = 32; alias T = buffer<max(x, y)>;`,
    valid: true
  },
  empty_template: {
    code: `alias T = buffer<>;`,
    valid: false
  },
  missing_rparen: {
    code: `alias T = buffer<16;`,
    valid: false
  },
  subtype: {
    code: `alias T = buffer<u32>;`,
    valid: false
  },
  pointer_workgroup_unsized_buffer: {
    code: `alias T = buffer; alias PT = ptr<workgroup, T>;`,
    valid: true
  },
  pointer_uniform_unsized_buffer: {
    code: `alias T = buffer; alias PT = ptr<uniform, T>;`,
    valid: true
  },
  pointer_storage_unsized_buffer: {
    code: `alias T = buffer; alias PT = ptr<storage, T>;`,
    valid: true
  },
  pointer_workgroup_sized_buffer: {
    code: `alias T = buffer<32>; alias PT = ptr<workgroup, T>;`,
    valid: true
  },
  pointer_uniform_sized_buffer: {
    code: `alias T = buffer<32>; alias PT = ptr<uniform, T>;`,
    valid: true
  },
  pointer_storage_sized_buffer: {
    code: `alias T = buffer<32>; alias PT = ptr<storage, T>;`,
    valid: true
  },
  pointer_workgroup_override_sized_buffer: {
    code: `override o : i32; alias T = buffer<o>; alias PT = ptr<workgroup, T>;`,
    valid: true
  },
  pointer_uniform_override_sized_buffer: {
    code: `override o : i32; alias T = buffer<o>; alias PT = ptr<uniform, T>;`,
    valid: false
  },
  pointer_storage_override_sized_buffer: {
    code: `override o : i32; alias T = buffer<o>; alias PT = ptr<storage, T>;`,
    valid: false
  },
  buffer_3bytes: {
    code: `alias T = buffer<3>;`,
    valid: false
  },
  buffer_2bytes_f16: {
    code: `enable f16;\nalias T = buffer<2>;`,
    valid: true
  },
  buffer_2bytes_no_f16: {
    code: `alias T = buffer<2>;`,
    valid: false
  }
};

g.test('parse').
desc('Test buffer type parsing').
params((u) => u.combine('case', keysOf(kParseCases))).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const testcase = kParseCases[t.params.case];
  t.expectCompileResult(testcase.valid, testcase.code);
});

g.test('address_space').
desc('Test buffer type validity for each address space').
params((u) =>
u.
combine('case', keysOf(kParseCases)).
filter((t) => {
  return kParseCases[t.case].valid && !t.case.includes('pointer');
}).
beginSubcases().
combine('aspace', [
'function',
'private',
'storage',
'uniform',
'workgroup',
'immediate']
)
).
fn((t) => {
  t.skipIfLanguageFeatureNotSupported('buffer_view');
  const testcase = kParseCases[t.params.case];

  let mvar = '';
  let fvar = '';
  switch (t.params.aspace) {
    case 'function':
      fvar = `var v : T;`;
      break;
    case 'private':
    case 'workgroup':
    case 'immediate':
      mvar = `var<${t.params.aspace}> v : T;`;
      break;
    case 'storage':
    case 'uniform':
      mvar = `@group(0) @binding(0) var<${t.params.aspace}> v : T;`;
      break;
  }

  const wgsl = `
${testcase.code}
${mvar}
@compute @workgroup_size(1)
fn main() {
  ${fvar}
}`;

  let expected =
  t.params.aspace !== 'function' &&
  t.params.aspace !== 'private' &&
  t.params.aspace !== 'immediate';
  if (t.params.case === 'unsized') {
    expected = t.params.aspace === 'storage';
  } else if (t.params.case.includes('override')) {
    expected = t.params.aspace === 'workgroup';
  }
  t.expectCompileResult(expected, wgsl);
});
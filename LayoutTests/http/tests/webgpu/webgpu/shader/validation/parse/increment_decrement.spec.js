/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `Validation parser tests for increment and decrement statements`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);










const kTests = {
  var: { wgsl: 'a++;', pass: true },
  vector: { wgsl: 'v++;', pass: false },
  paren_var_paren: { wgsl: '(a)++;', pass: true },
  star_and_var: { wgsl: '*&a++;', pass: true },
  paren_star_and_var_paren: { wgsl: '(*&a)++;', pass: true },
  many_star_and_var: { wgsl: '*&*&*&a++;', pass: true },

  space: { wgsl: 'a ++;', pass: true },
  tab: { wgsl: 'a\t++;', pass: true },
  newline: { wgsl: 'a\n++;', pass: true },
  cr: { wgsl: 'a\r++;', pass: true },
  space_space: { wgsl: 'a ++ ;', pass: true },
  plus_space_plus: { wgsl: 'a+ +;', pass: false },
  minux_space_minus: { wgsl: 'a- -;', pass: false },

  no_var: { wgsl: '++;', pass: false },
  no_semi: { wgsl: 'a++', pass: false },
  prefix: { wgsl: '++a;', pass: false },

  postfix_x: { wgsl: 'v++.x;', pass: false },
  postfix_r: { wgsl: 'v++.r;', pass: false },
  postfix_index: { wgsl: 'v++[0];', pass: false },
  postfix_field: { wgsl: 'a++.foo;', pass: false },

  literal_i32: { wgsl: '12i++;', pass: false },
  literal_u32: { wgsl: '12u++;', pass: false },
  literal_abstract_int: { wgsl: '12++;', pass: false },
  literal_abstract_float: { wgsl: '12.0++;', pass: false },
  literal_f32: { wgsl: '12.0f++;', pass: false },

  assign_to: { wgsl: 'a++ = 1;', pass: false },

  at_global: { wgsl: '', pass: false, gdecl: 'var<private> g:i32; g++;' },
  private: { wgsl: 'g++;', pass: true, gdecl: 'var<private> g:i32;' },
  workgroup: { wgsl: 'g++;', pass: true, gdecl: 'var<workgroup> g:i32;' },
  storage_rw: {
    wgsl: 'g++;',
    pass: true,
    gdecl: '@group(0) @binding(0) var<storage,read_write> g: i32;'
  },
  storage_r: {
    wgsl: 'g++;',
    pass: false,
    gdecl: '@group(0) @binding(0) var<storage,read> g: i32;'
  },
  storage: { wgsl: 'g++;', pass: false, gdecl: '@group(0) @binding(0) var<storage,read> g: i32;' },
  uniform: { wgsl: 'g++;', pass: false, gdecl: '@group(0) @binding(0) var<uniform> g: i32;' },
  texture: { wgsl: 'g++;', pass: false, gdecl: '@group(0) @binding(0) var g: texture_2d<u32>;' },
  texture_x: {
    wgsl: 'g.x++;',
    pass: false,
    gdecl: '@group(0) @binding(0) var g: texture_2d<u32>;'
  },
  texture_storage: {
    wgsl: 'g++;',
    pass: false,
    gdecl: '@group(0) @binding(0) var g: texture_storage_2d<r32uint>;'
  },
  texture_storage_x: {
    wgsl: 'g.x++;',
    pass: false,
    gdecl: '@group(0) @binding(0) var g: texture_storage_2d<r32uint>;'
  },
  sampler: { wgsl: 'g++;', pass: false, gdecl: '@group(0) @binding(0) var g: sampler;' },
  sampler_comparison: {
    wgsl: 'g++;',
    pass: false,
    gdecl: '@group(0) @binding(0) var g: sampler_comparison;'
  },
  override: { wgsl: 'g++;', pass: false, gdecl: 'override g:i32;' },
  global_const: { wgsl: 'g++;', pass: false, gdecl: 'const g:i32 = 0;' },
  workgroup_atomic: { wgsl: 'g++;', pass: false, gdecl: 'var<workgroup> g:atomic<i32>;' },
  storage_atomic: {
    wgsl: 'g++;',
    pass: false,
    gdecl: '@group(0) @binding(0) var<storage,read_write> g:atomic<u32>;'
  },

  subexpr: { wgsl: 'a = b++;', pass: false },
  expr_paren: { wgsl: '(a++);', pass: false },
  expr_add: { wgsl: '0 + a++;', pass: false },
  expr_negate: { wgsl: '-a++;', pass: false },
  inc_inc: { wgsl: 'a++++;', pass: false },
  inc_space_inc: { wgsl: 'a++ ++;', pass: false },
  inc_dec: { wgsl: 'a++--;', pass: false },
  inc_space_dec: { wgsl: 'a++ --;', pass: false },
  paren_inc: { wgsl: '(a++)++;', pass: false },
  paren_dec: { wgsl: '(a++)--;', pass: false },

  in_block: { wgsl: '{ a++; }', pass: true },
  in_for_init: { wgsl: 'for (a++;false;) {}', pass: true },
  in_for_cond: { wgsl: 'for (;a++;) {}', pass: false },
  in_for_update: { wgsl: 'for (;false;a++) {}', pass: true },
  in_for_update_semi: { wgsl: 'for (;false;a++;) {}', pass: false },
  in_continuing: { wgsl: 'loop { continuing { a++; break if true;}}', pass: true },

  let: { wgsl: 'let c = a; c++;', pass: false },
  const: { wgsl: 'const c = 1; c++;', pass: false },
  builtin: { wgsl: 'max++', pass: false },
  enum: { wgsl: 'r32uint++', pass: false },
  param: { wgsl: '', pass: false, gdecl: 'fn bump(p: i32) { p++;}' }
};

g.test('parse').
desc(`Test that increment and decrement statements are parsed correctly.`).
params((u) => u.combine('test', keysOf(kTests)).combine('direction', ['up', 'down'])).
fn((t) => {
  const c = kTests[t.params.test];
  let { wgsl, gdecl } = c;
  gdecl = gdecl ?? '';
  if (t.params.direction === 'down') {
    wgsl = wgsl.replace('++', '--');
    gdecl = gdecl.replace('++', '--');
  }

  const code = `
${gdecl}
fn f() {
  var a: u32;
  var b: u32;
  var v: vec4u;
  ${wgsl}
}`;
  t.expectCompileResult(c.pass, code);
});
/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `Validation tests for tokenization`;
import { makeTestGroup } from '../../../common/framework/test_group.js';

import { ShaderValidationTest } from './shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

g.test('empty')
  .desc(`Test that an empty source file is consumed successfully.`)
  .fn(t => {
    t.expectCompileResult(true, '');
  });

g.test('null_characters')
  .desc(`Test that WGSL source containing a null character is rejected.`)
  .params(u =>
    u
      .combine('contains_null', [true, false])
      .combine('placement', ['comment', 'delimiter', 'eol'])
      .beginSubcases()
  )
  .fn(t => {
    let code = '';
    if (t.params.placement === 'comment') {
      code = `// Here is a ${t.params.contains_null ? '\0' : 'Z'} character`;
    } else if (t.params.placement === 'delimiter') {
      code = `let${t.params.contains_null ? '\0' : ' '}name : i32 = 0;`;
    } else if (t.params.placement === 'eol') {
      code = `let name : i32 = 0;${t.params.contains_null ? '\0' : ''}`;
    }
    t.expectCompileResult(!t.params.contains_null, code);
  });

g.test('whitespace')
  .desc(`Test that all whitespace characters act as delimiters.`)
  .fn(t => {
    const code = `
let space:i32=0;
let\thorizontal_tab:i32=0;
let\nlinefeed:i32=0;
let\vvertical_tab:i32=0;
let\fformfeed:i32=0;
let\rcarriage_return:i32=0;
`;
    t.expectCompileResult(true, code);
  });

g.test('comments')
  .desc(`Test that valid comments are handled correctly, including nesting.`)
  .fn(t => {
    const code = `
/**
 * Here is my shader.
 *
 * /* I can nest /**/ comments. */
 * // I can nest line comments too.
 **/
@stage(fragment) // This is the stage
fn main(/*
no
parameters
*/) -> @location(0) vec4<f32> {
  return/*block_comments_delimit_tokens*/vec4<f32>(.4, .2, .3, .1);
}/* terminated block comments are OK at EOF...*/`;
    t.expectCompileResult(true, code);
  });

g.test('line_comment_terminators')
  .desc(`Test that line comments are terminated by any blankspace other than space and \t`)
  .params(u => u.combine('blankspace', [' ', '\t', '\n', '\v', '\f', '\r']).beginSubcases())
  .fn(t => {
    const code = `// Line comment${t.params.blankspace}let invalid_outside_comment = should_fail`;

    t.expectCompileResult([' ', '\t'].includes(t.params.blankspace), code);
  });

g.test('unterminated_block_comment')
  .desc(`Test that unterminated block comments cause an error`)
  .params(u => u.combine('terminated', [true, false]).beginSubcases())
  .fn(t => {
    const code = `
/**
 * Unterminated block comment.
 *
 ${t.params.terminated ? '*/' : ''}`;

    t.expectCompileResult(t.params.terminated, code);
  });

const kValidIdentifiers = new Set(['foo', 'Foo', '_foo0', '_0foo', 'foo__0']);
const kInvalidIdentifiers = new Set([
  '_', // Single underscore is a syntactic token for phony assignment.
  '__foo', // Leading double underscore is reserved.
  '0foo', // Must start with single underscore or a letter.
  // No punctuation:
  'foo.bar',
  'foo-bar',
  'foo+bar',
  'foo#bar',
  'foo!bar',
  'foo\\bar',
  'foo/bar',
  'foo,bar',
  'foo@bar',
  'foo::bar',
  // Keywords:
  'array',
  'atomic',
  'bitcast',
  'bool',
  'break',
  'case',
  'continue',
  'continuing',
  'default',
  'discard',
  'enable',
  'else',
  'f32',
  'fallthrough',
  'false',
  'fn',
  'for',
  'function',
  'i32',
  'if',
  'let',
  'loop',
  'mat2x2',
  'mat2x3',
  'mat2x4',
  'mat3x2',
  'mat3x3',
  'mat3x4',
  'mat4x2',
  'mat4x3',
  'mat4x4',
  'override',
  'private',
  'ptr',
  'return',
  'sampler',
  'sampler_comparison',
  'storage',
  'struct',
  'switch',
  'texture_1d',
  'texture_2d',
  'texture_2d_array',
  'texture_3d',
  'texture_cube',
  'texture_cube_array',
  'texture_depth_2d',
  'texture_depth_2d_array',
  'texture_depth_cube',
  'texture_depth_cube_array',
  'texture_depth_multisampled_2d',
  'texture_multisampled_2d',
  'texture_storage_1d',
  'texture_storage_2d',
  'texture_storage_2d_array',
  'texture_storage_3d',
  'true',
  'type',
  'u32',
  'uniform',
  'var',
  'vec2',
  'vec3',
  'vec4',
  'while',
  'workgroup',
]);

g.test('identifiers')
  .desc(
    `Test that valid identifiers are accepted, and invalid identifiers are rejected.

TODO: Add reserved words, when they've been refined.`
  )
  .params(u =>
    u.combine('ident', new Set([...kValidIdentifiers, ...kInvalidIdentifiers])).beginSubcases()
  )
  .fn(t => {
    const code = `var<private> ${t.params.ident} : i32;`;
    t.expectCompileResult(kValidIdentifiers.has(t.params.ident), code);
  });

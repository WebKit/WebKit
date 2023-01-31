/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `Validation tests for @builtin`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kValidBuiltin = new Set([
  `@builtin(position)`,
  `@builtin(position,)`,
  `@ \n builtin(position)`,
  `@/^ comment ^/builtin/^ comment ^/\n\n(\t/^comment^/position/^comment^/)`,
]);

const kInvalidBuiltin = new Set([
  `@abuiltin(position)`,
  `@builtin`,
  `@builtin()`,
  `@builtin position`,
  `@builtin position)`,
  `@builtin(position`,
  `@builtin(position, frag_depth)`,
  `@builtin(identifier)`,
  `@builtin(2)`,
]);

g.test('parse')
  .desc(`Test that @builtin is parsed correctly.`)
  .params(u => u.combine('builtin', new Set([...kValidBuiltin, ...kInvalidBuiltin])))
  .fn(t => {
    const v = t.params.builtin.replace(/\^/g, '*');
    const code = `
@vertex
fn main() -> ${v} vec4<f32> {
  return vec4<f32>(.4, .2, .3, .1);
}`;
    t.expectCompileResult(kValidBuiltin.has(t.params.builtin), code);
  });

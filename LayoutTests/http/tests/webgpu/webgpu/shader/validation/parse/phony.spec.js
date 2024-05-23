/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `Validation parser tests for phony assignment statements`;import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { keysOf } from '../../../../common/util/data_tables.js';
import { ShaderValidationTest } from '../shader_validation_test.js';

export const g = makeTestGroup(ShaderValidationTest);

const kTests = {
  literal: { wgsl: `_ = 1;`, pass: true },
  expr: { wgsl: `_ = (1+v);`, pass: true },
  var: { wgsl: `_ = v;`, pass: true },

  in_for_init: { wgsl: `for (_ = v;false;) {}`, pass: true },
  in_for_init_semi: { wgsl: `for (_ = v;;false;) {}`, pass: false },
  in_for_update: { wgsl: `for (;false; _ = v) {}`, pass: true },
  in_for_update_semi: { wgsl: `for (;false; _ = v;) {}`, pass: false },

  in_block: { wgsl: `{_ = v;}`, pass: true },
  in_continuing: { wgsl: `loop { continuing { _ = v; break if true;}}`, pass: true },

  in_paren: { wgsl: `(_ = v;)`, pass: false },

  underscore: { wgsl: `_`, pass: false },
  underscore_semi: { wgsl: `_;`, pass: false },
  underscore_equal: { wgsl: `_=`, pass: false },
  underscore_equal_semi: { wgsl: `_=;`, pass: false },
  underscore_equal_underscore_semi: { wgsl: `_=_;`, pass: false },
  paren_underscore_paren: { wgsl: `(_) = 1;`, pass: false },
  // LHS is not a reference type
  star_ampersand_undsscore: { wgsl: `*&_ = 1;`, pass: false },
  compound: { wgsl: `_ += 1;`, pass: false },
  equality: { wgsl: `_ == 1;`, pass: false },
  block: { wgsl: `_ = {};`, pass: false },
  return: { wgsl: `_ = return;`, pass: false }
};

g.test('parse').
desc(`Test that 'phony assignment' statements are parsed correctly.`).
params((u) => u.combine('test', keysOf(kTests))).
fn((t) => {
  const code = `
fn f() {
  var v: u32;
  ${kTests[t.params.test].wgsl}
}`;
  t.expectCompileResult(kTests[t.params.test].pass, code);
});
/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/ /**
 * Use to test that certain WGSL builtins are only available in the fragment stage.
 * Create WGSL that defines a function "foo" and its required variables that uses
 * the builtin being tested. Append it to these code strings then compile. It should
 * succeed or fail based on the value `expectSuccess`.
 *
 * See ./textureSample.spec.ts was one example
 */export const kEntryPointsToValidateFragmentOnlyBuiltins = { none: {
    expectSuccess: true,
    code: ``
  },
  fragment: {
    expectSuccess: true,
    code: `
      @fragment
      fn main() {
        foo();
      }
    `
  },
  vertex: {
    expectSuccess: false,
    code: `
      @vertex
      fn main() -> @builtin(position) vec4f {
        foo();
        return vec4f();
      }
    `
  },
  compute: {
    expectSuccess: false,
    code: `
      @compute @workgroup_size(1)
      fn main() {
        foo();
      }
    `
  },
  fragment_and_compute: {
    expectSuccess: false,
    code: `
      @fragment
      fn main1() {
        foo();
      }

      @compute @workgroup_size(1)
      fn main2() {
        foo();
      }
    `
  },
  compute_without_call: {
    expectSuccess: true,
    code: `
      @compute @workgroup_size(1)
      fn main() {
      }
    `
  }
};
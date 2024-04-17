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

export const kTestTextureTypes = [
'texture_1d<f32>',
'texture_1d<u32>',
'texture_2d<f32>',
'texture_2d<u32>',
'texture_2d_array<f32>',
'texture_2d_array<u32>',
'texture_3d<f32>',
'texture_3d<u32>',
'texture_cube<f32>',
'texture_cube<u32>',
'texture_cube_array<f32>',
'texture_cube_array<u32>',
'texture_multisampled_2d<f32>',
'texture_multisampled_2d<u32>',
'texture_depth_multisampled_2d',
'texture_external',
'texture_storage_1d<rgba8unorm, read>',
'texture_storage_1d<r32uint, read>',
'texture_storage_2d<rgba8unorm, read>',
'texture_storage_2d<r32uint, read>',
'texture_storage_2d_array<rgba8unorm, read>',
'texture_storage_2d_a32uint8unorm, read>',
'texture_storage_3d<rgba8unorm, read>',
'texture_storage_3d<r32uint, read>',
'texture_depth_2d',
'texture_depth_2d_array',
'texture_depth_cube',
'texture_depth_cube_array'];
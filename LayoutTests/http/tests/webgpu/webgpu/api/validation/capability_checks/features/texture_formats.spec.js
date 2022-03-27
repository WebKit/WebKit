/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for capability checking for features enabling optional texture formats.

TODO(#902): test GPUTextureViewDescriptor.format
TODO(#902): test GPUCanvasConfiguration.format (it doesn't allow any optional formats today but the
  error might still be different - exception instead of validation.

TODO(#920): test GPUTextureDescriptor.viewFormats (if/when it takes formats)
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { kAllTextureFormats, kTextureFormatInfo } from '../../../../capability_info.js';
import { ValidationTest } from '../../validation_test.js';

export const g = makeTestGroup(ValidationTest);

const kOptionalTextureFormats = kAllTextureFormats.filter(
  t => kTextureFormatInfo[t].feature !== undefined
);

g.test('texture_descriptor')
  .desc(
    `
  Test creating a texture with an optional texture format will fail if the required optional feature
  is not enabled.

  TODO(#919): Actually it should throw an exception, not fail with a validation error.
  `
  )
  .params(u =>
    u
      .combine('format', kOptionalTextureFormats)
      .beginSubcases()
      .combine('enable_required_feature', [true, false])
  )
  .fn(async t => {
    const { format, enable_required_feature } = t.params;

    const formatInfo = kTextureFormatInfo[format];
    if (enable_required_feature) {
      await t.selectDeviceOrSkipTestCase(formatInfo.feature);
    }

    t.expectValidationError(() => {
      t.device.createTexture({
        format,
        size: [formatInfo.blockWidth, formatInfo.blockHeight, 1],
        usage: GPUTextureUsage.TEXTURE_BINDING,
      });
    }, !enable_required_feature);
  });

g.test('storage_texture_binding_layout')
  .desc(
    `
  Test creating a GPUStorageTextureBindingLayout with an optional texture format will fail if the
  required optional feature are not enabled.

  Note: This test has no cases if there are no optional texture formats supporting storage.
  `
  )
  .params(u =>
    u
      .combine('format', kOptionalTextureFormats)
      .filter(t => kTextureFormatInfo[t.format].storage)
      .beginSubcases()
      .combine('enable_required_feature', [true, false])
  )
  .fn(async t => {
    const { format, enable_required_feature } = t.params;

    const formatInfo = kTextureFormatInfo[format];
    if (enable_required_feature) {
      await t.selectDeviceOrSkipTestCase(formatInfo.feature);
    }

    t.expectValidationError(() => {
      t.device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.COMPUTE,
            storageTexture: {
              format,
            },
          },
        ],
      });
    }, !enable_required_feature);
  });

g.test('color_target_state')
  .desc(
    `
  Test creating a render pipeline with an optional texture format set in GPUColorTargetState will
  fail if the required optional feature is not enabled.

  Note: This test has no cases if there are no optional texture formats supporting color rendering.
  `
  )
  .params(u =>
    u
      .combine('format', kOptionalTextureFormats)
      .filter(t => kTextureFormatInfo[t.format].renderable && kTextureFormatInfo[t.format].color)
      .beginSubcases()
      .combine('enable_required_feature', [true, false])
  )
  .fn(async t => {
    const { format, enable_required_feature } = t.params;

    const formatInfo = kTextureFormatInfo[format];
    if (enable_required_feature) {
      await t.selectDeviceOrSkipTestCase(formatInfo.feature);
    }

    t.expectValidationError(() => {
      t.device.createRenderPipeline({
        vertex: {
          module: t.device.createShaderModule({
            code: `
              @stage(vertex)
              fn main()-> @builtin(position) vec4<f32> {
                return vec4<f32>(0.0, 0.0, 0.0, 1.0);
              }`,
          }),

          entryPoint: 'main',
        },

        fragment: {
          module: t.device.createShaderModule({
            code: `
              @stage(fragment)
              fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(0.0, 1.0, 0.0, 1.0);
              }`,
          }),

          entryPoint: 'main',
          targets: [{ format }],
        },
      });
    }, !enable_required_feature);
  });

g.test('depth_stencil_state')
  .desc(
    `
  Test creating a render pipeline with an optional texture format set in GPUColorTargetState will
  fail if the required optional feature is not enabled.
  `
  )
  .params(u =>
    u
      .combine('format', kOptionalTextureFormats)
      .filter(
        t =>
          kTextureFormatInfo[t.format].renderable &&
          (kTextureFormatInfo[t.format].depth || kTextureFormatInfo[t.format].stencil)
      )
      .beginSubcases()
      .combine('enable_required_feature', [true, false])
  )
  .fn(async t => {
    const { format, enable_required_feature } = t.params;

    const formatInfo = kTextureFormatInfo[format];
    if (enable_required_feature) {
      await t.selectDeviceOrSkipTestCase(formatInfo.feature);
    }

    t.expectValidationError(() => {
      t.device.createRenderPipeline({
        vertex: {
          module: t.device.createShaderModule({
            code: `
              @stage(vertex)
              fn main()-> @builtin(position) vec4<f32> {
                return vec4<f32>(0.0, 0.0, 0.0, 1.0);
              }`,
          }),

          entryPoint: 'main',
        },

        depthStencil: {
          format,
        },

        fragment: {
          module: t.device.createShaderModule({
            code: `
              @stage(fragment)
              fn main() -> @location(0) vec4<f32> {
                return vec4<f32>(0.0, 1.0, 0.0, 1.0);
              }`,
          }),

          entryPoint: 'main',
          targets: [{ format: 'rgba8unorm' }],
        },
      });
    }, !enable_required_feature);
  });

g.test('render_bundle_encoder_descriptor_color_format')
  .desc(
    `
  Test creating a render bundle encoder with an optional texture format set as one of the color
  format will fail if the required optional feature is not enabled.

  Note: This test has no cases if there are no optional texture formats supporting color rendering.
  `
  )
  .params(u =>
    u
      .combine('format', kOptionalTextureFormats)
      .filter(t => kTextureFormatInfo[t.format].renderable && kTextureFormatInfo[t.format].color)
      .beginSubcases()
      .combine('enable_required_feature', [true, false])
  )
  .fn(async t => {
    const { format, enable_required_feature } = t.params;

    const formatInfo = kTextureFormatInfo[format];
    if (enable_required_feature) {
      await t.selectDeviceOrSkipTestCase(formatInfo.feature);
    }

    t.expectValidationError(() => {
      t.device.createRenderBundleEncoder({
        colorFormats: [format],
      });
    }, !enable_required_feature);
  });

g.test('render_bundle_encoder_descriptor_depth_stencil_format')
  .desc(
    `
  Test creating a render bundle encoder with an optional texture format set as the depth stencil
  format will fail if the required optional feature is not enabled.
  `
  )
  .params(u =>
    u
      .combine('format', kOptionalTextureFormats)
      .filter(
        t =>
          kTextureFormatInfo[t.format].renderable &&
          (kTextureFormatInfo[t.format].depth || kTextureFormatInfo[t.format].stencil)
      )
      .beginSubcases()
      .combine('enable_required_feature', [true, false])
  )
  .fn(async t => {
    const { format, enable_required_feature } = t.params;

    const formatInfo = kTextureFormatInfo[format];
    if (enable_required_feature) {
      await t.selectDeviceOrSkipTestCase(formatInfo.feature);
    }

    t.expectValidationError(() => {
      t.device.createRenderBundleEncoder({
        colorFormats: ['rgba8unorm'],
        depthStencilFormat: format,
      });
    }, !enable_required_feature);
  });

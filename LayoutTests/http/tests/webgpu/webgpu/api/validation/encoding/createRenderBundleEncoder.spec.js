/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
createRenderBundleEncoder validation tests.
`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import {
  kAllTextureFormats,
  kDepthStencilFormats,
  kTextureFormatInfo,
  kMaxColorAttachments,
} from '../../../capability_info.js';
import { ValidationTest } from '../validation_test.js';

export const g = makeTestGroup(ValidationTest);

g.test('attachment_state')
  .desc(
    `
    Tests that createRenderBundleEncoder correctly validates the attachment state passed to it.
      - Must be <= kMaxColorAttachments (8) colorFormats
      - Must have a depthStencilFormat if no colorFormats are given
    `
  )
  .params(u =>
    u //
      .combine('colorFormatCount', [...Array(kMaxColorAttachments + 2).keys()]) // 0-9
      .beginSubcases()
      .combine('colorFormat', [undefined, 'bgra8unorm'])
      .combine('depthStencilFormat', [undefined, 'depth24plus-stencil8'])
      .filter(({ colorFormat, colorFormatCount }) => {
        // Only run the test with 0 colorFormats once.
        return (
          (colorFormat !== undefined && colorFormatCount > 0) ||
          (colorFormat === undefined && colorFormatCount === 0)
        );
      })
  )
  .fn(async t => {
    const { colorFormat, depthStencilFormat, colorFormatCount } = t.params;

    // Ensure up to kMaxColorAttachments (8) color formats are allowed.
    let shouldError = colorFormatCount > kMaxColorAttachments;

    // Zero color formats are only allowed if a depthStencilFormat is provided.
    if (depthStencilFormat === undefined && colorFormatCount === 0) {
      shouldError = true;
    }

    t.expectValidationError(() => {
      t.device.createRenderBundleEncoder({
        colorFormats: Array(colorFormatCount).fill(colorFormat),
        depthStencilFormat,
      });
    }, shouldError);
  });

g.test('valid_texture_formats')
  .desc(
    `
    Tests that createRenderBundleEncoder only accepts valid formats for its attachments.
      - colorFormats
      - depthStencilFormat
    `
  )
  .params(u =>
    u //
      .combine('format', kAllTextureFormats)
      .beginSubcases()
      .combine('attachment', ['color', 'depthStencil'])
  )
  .fn(async t => {
    const { format, attachment } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const colorRenderable =
      kTextureFormatInfo[format].renderable && kTextureFormatInfo[format].color;

    const depthStencil = kTextureFormatInfo[format].depth || kTextureFormatInfo[format].stencil;

    switch (attachment) {
      case 'color': {
        t.expectValidationError(() => {
          t.device.createRenderBundleEncoder({
            colorFormats: [format],
          });
        }, !colorRenderable);

        break;
      }
      case 'depthStencil': {
        t.expectValidationError(() => {
          t.device.createRenderBundleEncoder({
            colorFormats: [],
            depthStencilFormat: format,
          });
        }, !depthStencil);

        break;
      }
    }
  });

g.test('depth_stencil_readonly')
  .desc(
    `
    Tests that createRenderBundleEncoder validation of depthReadOnly and stencilReadOnly
      - With depth-only formats
      - With stencil-only formats
      - With depth-stencil-combined formats
    `
  )
  .params(u =>
    u //
      .combine('depthStencilFormat', kDepthStencilFormats)
      .beginSubcases()
      .combine('depthReadOnly', [false, true])
      .combine('stencilReadOnly', [false, true])
  )
  .fn(async t => {
    const { depthStencilFormat, depthReadOnly, stencilReadOnly } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(depthStencilFormat);

    let shouldError = false;
    if (
      kTextureFormatInfo[depthStencilFormat].depth &&
      kTextureFormatInfo[depthStencilFormat].stencil &&
      depthReadOnly !== stencilReadOnly
    ) {
      shouldError = true;
    }

    t.expectValidationError(() => {
      t.device.createRenderBundleEncoder({
        colorFormats: [],
        depthStencilFormat,
        depthReadOnly,
        stencilReadOnly,
      });
    }, shouldError);
  });

g.test('depth_stencil_readonly_with_undefined_depth')
  .desc(
    `
    Tests that createRenderBundleEncoder validation of depthReadOnly and stencilReadOnly is ignored
    if there is no depthStencilFormat set.
    `
  )
  .params(u =>
    u //
      .beginSubcases()
      .combine('depthReadOnly', [false, true])
      .combine('stencilReadOnly', [false, true])
  )
  .fn(async t => {
    const { depthReadOnly, stencilReadOnly } = t.params;

    t.device.createRenderBundleEncoder({
      colorFormats: ['bgra8unorm'],
      depthReadOnly,
      stencilReadOnly,
    });
  });

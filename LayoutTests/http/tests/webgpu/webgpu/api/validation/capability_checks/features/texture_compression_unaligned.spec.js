/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Tests for capability checking for the 'texture-compression-unaligned' feature.

When the feature is not enabled, the behavior is unchanged: the size of mip level 0 of a
block-compressed texture must be a multiple of the texel block size. When the feature is enabled,
mip level 0 is allowed to have a size that is not a multiple of the texel block size (i.e. partial
edge blocks).
`;import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import {
  kCompressedTextureFormats,
  getBlockInfoForTextureFormat,
  getRequiredFeatureForTextureFormat } from
'../../../../format_info.js';
import { UniqueFeaturesOrLimitsGPUTest } from '../../../../gpu_test.js';

export const g = makeTestGroup(UniqueFeaturesOrLimitsGPUTest);

g.test('createTexture,unaligned_size').
desc(
  `Test that creating a compressed texture whose mip level 0 size is not a multiple of the texel
    block size succeeds if and only if 'texture-compression-unaligned' is enabled.`
).
params((u) =>
u.
combine('format', kCompressedTextureFormats).
combine('enable_feature', [true, false])
// The unaligned dimension(s) of mip level 0 being tested.
.combine('sizeCase', ['width', 'height', 'both', 'single'])
).
beforeAllSubcases((t) => {
  const { format, enable_feature } = t.params;

  const requiredFeatures = [];
  const formatFeature = getRequiredFeatureForTextureFormat(format);
  if (formatFeature) {
    requiredFeatures.push(formatFeature);
  }
  if (enable_feature) {
    requiredFeatures.push('texture-compression-unaligned');
  }

  t.selectDeviceOrSkipTestCase({ requiredFeatures });
}).
fn((t) => {
  const { format, enable_feature, sizeCase } = t.params;
  t.skipIfTextureFormatNotSupported(format);

  const { blockWidth, blockHeight } = getBlockInfoForTextureFormat(format);

  // Construct a mip level 0 size that is not a multiple of the texel block size.
  const size = (() => {
    switch (sizeCase) {
      case 'width':
        return [blockWidth + 1, blockHeight, 1];
      case 'height':
        return [blockWidth, blockHeight + 1, 1];
      case 'both':
        return [blockWidth + 1, blockHeight + 1, 1];
      case 'single':
        // A single partial edge block in both dimensions.
        return [1, 1, 1];
    }
  })();

  const descriptor = {
    size,
    format,
    usage: GPUTextureUsage.TEXTURE_BINDING
  };

  // Without the feature, an unaligned mip level 0 size is a validation error.
  t.expectValidationError(() => {
    t.createTextureTracked(descriptor);
  }, !enable_feature);
});
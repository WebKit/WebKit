/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
TODO:
- 2 views:
    - x= {upon the same subresource, or different subresources {mip level, array layer, aspect} of
         the same texture}
    - x= possible resource usages on each view:
         - both in bind group {texture_binding, storage_binding}
    - x= different shader stages: {0, ..., 7}
        - maybe first view vis = {1, 2, 4}, second view vis = {0, ..., 7}
    - x= bindings are in {
        - same draw call
        - same pass, different draw call
        - different pass
        - }
(It's probably not necessary to test EVERY possible combination of options in this whole
block, so we could break it down into a few smaller ones (one for different types of
subresources, one for same draw/same pass/different pass, one for visibilities).)
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { unreachable } from '../../../../../common/util/util.js';
import { ValidationTest } from '../../validation_test.js';

class F extends ValidationTest {
  getColorAttachment(texture, textureViewDescriptor) {
    const view = texture.createView(textureViewDescriptor);

    return {
      view,
      clearValue: { r: 1.0, g: 0.0, b: 0.0, a: 1.0 },
      loadOp: 'clear',
      storeOp: 'store',
    };
  }
}

export const g = makeTestGroup(F);

const kTextureSize = 16;
const kTextureLevels = 3;
const kTextureLayers = 3;

g.test('subresources_from_same_texture_as_color_attachments')
  .desc(
    `
  Test that the different subresource of the same texture are allowed to be used as color
  attachments in same / different render pass encoder, while the same subresource is only allowed
  to be used as different color attachments in different render pass encoders.`
  )
  .params(u =>
    u
      .combine('baseLayer0', [0, 1])
      .combine('baseLevel0', [0, 1])
      .combine('baseLayer1', [0, 1])
      .combine('baseLevel1', [0, 1])
      .combine('inSamePass', [true, false])
      .unless(t => t.inSamePass && t.baseLevel0 !== t.baseLevel1)
  )
  .fn(async t => {
    const { baseLayer0, baseLevel0, baseLayer1, baseLevel1, inSamePass } = t.params;

    const texture = t.device.createTexture({
      format: 'rgba8unorm',
      usage: GPUTextureUsage.RENDER_ATTACHMENT,
      size: [kTextureSize, kTextureSize, kTextureLayers],
      mipLevelCount: kTextureLevels,
    });

    const colorAttachment1 = t.getColorAttachment(texture, {
      baseArrayLayer: baseLayer0,
      arrayLayerCount: 1,
      baseMipLevel: baseLevel0,
      mipLevelCount: 1,
    });

    const colorAttachment2 = t.getColorAttachment(texture, {
      baseArrayLayer: baseLayer1,
      baseMipLevel: baseLevel1,
      mipLevelCount: 1,
    });

    const encoder = t.device.createCommandEncoder();
    if (inSamePass) {
      const renderPass = encoder.beginRenderPass({
        colorAttachments: [colorAttachment1, colorAttachment2],
      });

      renderPass.end();
    } else {
      const renderPass1 = encoder.beginRenderPass({
        colorAttachments: [colorAttachment1],
      });

      renderPass1.end();
      const renderPass2 = encoder.beginRenderPass({
        colorAttachments: [colorAttachment2],
      });

      renderPass2.end();
    }

    const success = inSamePass ? baseLayer0 !== baseLayer1 : true;
    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

g.test('subresources_from_same_texture_as_color_attachment_and_in_bind_group')
  .desc(
    `
  Test that when one subresource of a texture is used as a color attachment, it cannot be used in a
  bind group simultaneously in the same render pass encoder. It is allowed when the bind group is
  used in another render pass encoder instead of the same one.`
  )
  .params(u =>
    u
      .combine('colorAttachmentLevel', [0, 1])
      .combine('colorAttachmentLayer', [0, 1])
      .combineWithParams([
        { bindGroupViewBaseLevel: 0, bindGroupViewLevelCount: 1 },
        { bindGroupViewBaseLevel: 1, bindGroupViewLevelCount: 1 },
        { bindGroupViewBaseLevel: 1, bindGroupViewLevelCount: 2 },
      ])
      .combineWithParams([
        { bindGroupViewBaseLayer: 0, bindGroupViewLayerCount: 1 },
        { bindGroupViewBaseLayer: 1, bindGroupViewLayerCount: 1 },
        { bindGroupViewBaseLayer: 1, bindGroupViewLayerCount: 2 },
      ])
      .combine('bindGroupUsage', ['texture', 'storage'])
      .unless(t => t.bindGroupUsage === 'storage' && t.bindGroupViewLevelCount > 0)
      .combine('inSamePass', [true, false])
  )
  .fn(async t => {
    const {
      colorAttachmentLevel,
      colorAttachmentLayer,
      bindGroupViewBaseLevel,
      bindGroupViewLevelCount,
      bindGroupViewBaseLayer,
      bindGroupViewLayerCount,
      bindGroupUsage,
      inSamePass,
    } = t.params;

    const bindGroupLayoutEntry = {
      binding: 0,
      visibility: GPUShaderStage.FRAGMENT,
    };

    let textureUsage = GPUTextureUsage.RENDER_ATTACHMENT;
    const viewDimension = bindGroupViewLayerCount > 1 ? '2d-array' : '2d';
    switch (bindGroupUsage) {
      case 'texture':
        bindGroupLayoutEntry.texture = { viewDimension };
        textureUsage |= GPUTextureUsage.TEXTURE_BINDING;
        break;
      case 'storage':
        bindGroupLayoutEntry.storageTexture = {
          access: 'write-only',
          format: 'rgba8unorm',
          viewDimension,
        };

        textureUsage |= GPUTextureUsage.STORAGE_BINDING;
        break;
      default:
        unreachable();
        break;
    }

    const texture = t.device.createTexture({
      format: 'rgba8unorm',
      usage: textureUsage,
      size: [kTextureSize, kTextureSize, kTextureLayers],
      mipLevelCount: kTextureLevels,
    });

    const bindGroupView = texture.createView({
      dimension: bindGroupViewLayerCount > 1 ? '2d-array' : '2d',
      baseArrayLayer: bindGroupViewBaseLayer,
      arrayLayerCount: bindGroupViewLayerCount,
      baseMipLevel: bindGroupViewBaseLevel,
      mipLevelCount: bindGroupViewLevelCount,
    });

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [bindGroupLayoutEntry],
    });

    const bindGroup = t.device.createBindGroup({
      layout: bindGroupLayout,
      entries: [{ binding: 0, resource: bindGroupView }],
    });

    const colorAttachment = t.getColorAttachment(texture, {
      baseArrayLayer: colorAttachmentLayer,
      arrayLayerCount: 1,
      baseMipLevel: colorAttachmentLevel,
      mipLevelCount: 1,
    });

    const encoder = t.device.createCommandEncoder();
    const renderPass = encoder.beginRenderPass({
      colorAttachments: [colorAttachment],
    });

    if (inSamePass) {
      renderPass.setBindGroup(0, bindGroup);
      renderPass.end();
    } else {
      renderPass.end();

      const texture2 = t.device.createTexture({
        format: 'rgba8unorm',
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
        size: [kTextureSize, kTextureSize, 1],
        mipLevelCount: 1,
      });

      const colorAttachment2 = t.getColorAttachment(texture2);
      const renderPass2 = encoder.beginRenderPass({
        colorAttachments: [colorAttachment2],
      });

      renderPass2.setBindGroup(0, bindGroup);
      renderPass2.end();
    }

    const isMipLevelOverlapped =
      colorAttachmentLevel >= bindGroupViewBaseLevel &&
      colorAttachmentLevel < bindGroupViewBaseLevel + bindGroupViewLevelCount;
    const isArrayLayerOverlapped =
      colorAttachmentLayer >= bindGroupViewBaseLayer &&
      colorAttachmentLayer < bindGroupViewBaseLayer + bindGroupViewLayerCount;
    const isOverlapped = isMipLevelOverlapped && isArrayLayerOverlapped;

    const success = inSamePass ? !isOverlapped : true;
    t.expectValidationError(() => {
      encoder.finish();
    }, !success);
  });

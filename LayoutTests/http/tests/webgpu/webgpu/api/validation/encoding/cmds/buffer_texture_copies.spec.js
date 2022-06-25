/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyTextureToBuffer and copyBufferToTexture validation tests not covered by
the general image_copy tests, or by destroyed,*.

TODO:
- Move all the tests here to image_copy/.
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { assert, unreachable } from '../../../../../common/util/util.js';
import {
  kDepthStencilFormats,
  depthStencilBufferTextureCopySupported,
  depthStencilFormatAspectSize,
} from '../../../../capability_info.js';
import { align } from '../../../../util/math.js';
import { kBufferCopyAlignment, kBytesPerRowAlignment } from '../../../../util/texture/layout.js';
import { ValidationTest } from '../../validation_test.js';

class ImageCopyTest extends ValidationTest {
  testCopyBufferToTexture(source, destination, copySize, isSuccess) {
    const { encoder, validateFinishAndSubmit } = this.createEncoder('non-pass');
    encoder.copyBufferToTexture(source, destination, copySize);
    validateFinishAndSubmit(isSuccess, true);
  }

  testCopyTextureToBuffer(source, destination, copySize, isSuccess) {
    const { encoder, validateFinishAndSubmit } = this.createEncoder('non-pass');
    encoder.copyTextureToBuffer(source, destination, copySize);
    validateFinishAndSubmit(isSuccess, true);
  }

  testWriteTexture(destination, uploadData, dataLayout, copySize, isSuccess) {
    this.expectGPUError(
      'validation',
      () => this.queue.writeTexture(destination, uploadData, dataLayout, copySize),
      !isSuccess
    );
  }
}

export const g = makeTestGroup(ImageCopyTest);

g.test('depth_stencil_format,copy_usage_and_aspect')
  .desc(
    `
  Validate the combination of usage and aspect of each depth stencil format in copyBufferToTexture,
  copyTextureToBuffer and writeTexture. See https://gpuweb.github.io/gpuweb/#depth-formats for more
  details.
`
  )
  .params(u =>
    u //
      .combine('format', kDepthStencilFormats)
      .beginSubcases()
      .combine('aspect', ['all', 'depth-only', 'stencil-only'])
  )
  .fn(async t => {
    const { format, aspect } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const textureSize = { width: 1, height: 1, depthOrArrayLayers: 1 };
    const texture = t.device.createTexture({
      size: textureSize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const uploadBufferSize = 32;
    const buffer = t.device.createBuffer({
      size: uploadBufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    {
      const success = depthStencilBufferTextureCopySupported('CopyB2T', format, aspect);
      t.testCopyBufferToTexture({ buffer }, { texture, aspect }, textureSize, success);
    }

    {
      const success = depthStencilBufferTextureCopySupported('CopyT2B', format, aspect);
      t.testCopyTextureToBuffer({ texture, aspect }, { buffer }, textureSize, success);
    }

    {
      const success = depthStencilBufferTextureCopySupported('WriteTexture', format, aspect);
      const uploadData = new Uint8Array(uploadBufferSize);
      t.testWriteTexture({ texture, aspect }, uploadData, {}, textureSize, success);
    }
  });

g.test('depth_stencil_format,copy_buffer_size')
  .desc(
    `
  Validate the minimum buffer size for each depth stencil format in copyBufferToTexture,
  copyTextureToBuffer and writeTexture.

  Given a depth stencil format, a copy aspect ('depth-only' or 'stencil-only'), the copy method
  (buffer-to-texture or texture-to-buffer) and the copy size, validate
  - if the copy can be successfully executed with the minimum required buffer size.
  - if the copy fails with a validation error when the buffer size is less than the minimum
  required buffer size.
`
  )
  .params(u =>
    u
      .combine('format', kDepthStencilFormats)
      .combine('aspect', ['depth-only', 'stencil-only'])
      .combine('copyType', ['CopyB2T', 'CopyT2B', 'WriteTexture'])
      .filter(param =>
        depthStencilBufferTextureCopySupported(param.copyType, param.format, param.aspect)
      )
      .beginSubcases()
      .combine('copySize', [
        { width: 8, height: 1, depthOrArrayLayers: 1 },
        { width: 4, height: 4, depthOrArrayLayers: 1 },
        { width: 4, height: 4, depthOrArrayLayers: 3 },
      ])
  )
  .fn(async t => {
    const { format, aspect, copyType, copySize } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const texture = t.device.createTexture({
      size: copySize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const texelAspectSize = depthStencilFormatAspectSize(format, aspect);
    assert(texelAspectSize > 0);

    const bytesPerRowAlignment = copyType === 'WriteTexture' ? 1 : kBytesPerRowAlignment;
    const bytesPerRow = align(texelAspectSize * copySize.width, bytesPerRowAlignment);
    const rowsPerImage = copySize.height;
    const minimumBufferSize =
      bytesPerRow * (rowsPerImage * copySize.depthOrArrayLayers - 1) +
      align(texelAspectSize * copySize.width, kBufferCopyAlignment);
    assert(minimumBufferSize > kBufferCopyAlignment);

    const bigEnoughBuffer = t.device.createBuffer({
      size: minimumBufferSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const smallerBuffer = t.device.createBuffer({
      size: minimumBufferSize - kBufferCopyAlignment,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    if (copyType === 'CopyB2T') {
      t.testCopyBufferToTexture(
        { buffer: bigEnoughBuffer, bytesPerRow, rowsPerImage },
        { texture, aspect },
        copySize,
        true
      );

      t.testCopyBufferToTexture(
        { buffer: smallerBuffer, bytesPerRow, rowsPerImage },
        { texture, aspect },
        copySize,
        false
      );
    } else if (copyType === 'CopyT2B') {
      t.testCopyTextureToBuffer(
        { texture, aspect },
        { buffer: bigEnoughBuffer, bytesPerRow, rowsPerImage },
        copySize,
        true
      );

      t.testCopyTextureToBuffer(
        { texture, aspect },
        { buffer: smallerBuffer, bytesPerRow, rowsPerImage },
        copySize,
        false
      );
    } else if (copyType === 'WriteTexture') {
      const enoughUploadData = new Uint8Array(minimumBufferSize);
      const smallerUploadData = new Uint8Array(minimumBufferSize - kBufferCopyAlignment);
      t.testWriteTexture(
        { texture, aspect },
        enoughUploadData,
        {
          bytesPerRow,
          rowsPerImage,
        },

        copySize,
        true
      );

      t.testWriteTexture(
        { texture, aspect },
        smallerUploadData,
        {
          bytesPerRow,
          rowsPerImage,
        },

        copySize,
        false
      );
    } else {
      unreachable();
    }
  });

g.test('depth_stencil_format,copy_buffer_offset')
  .desc(
    `
    Validate for every depth stencil formats the buffer offset must be a multiple of 4 in
    copyBufferToTexture() and copyTextureToBuffer(), but the offset in writeTexture() doesn't always
    need to be a multiple of 4.
    `
  )
  .params(u =>
    u
      .combine('format', kDepthStencilFormats)
      .combine('aspect', ['depth-only', 'stencil-only'])
      .combine('copyType', ['CopyB2T', 'CopyT2B', 'WriteTexture'])
      .filter(param =>
        depthStencilBufferTextureCopySupported(param.copyType, param.format, param.aspect)
      )
      .beginSubcases()
      .combine('offset', [1, 2, 4, 6, 8])
  )
  .fn(async t => {
    const { format, aspect, copyType, offset } = t.params;
    await t.selectDeviceForTextureFormatOrSkipTestCase(format);

    const textureSize = { width: 4, height: 4, depthOrArrayLayers: 1 };

    const texture = t.device.createTexture({
      size: textureSize,
      format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    const texelAspectSize = depthStencilFormatAspectSize(format, aspect);
    assert(texelAspectSize > 0);

    const bytesPerRowAlignment = copyType === 'WriteTexture' ? 1 : kBytesPerRowAlignment;
    const bytesPerRow = align(texelAspectSize * textureSize.width, bytesPerRowAlignment);
    const rowsPerImage = textureSize.height;
    const minimumBufferSize =
      bytesPerRow * (rowsPerImage * textureSize.depthOrArrayLayers - 1) +
      align(texelAspectSize * textureSize.width, kBufferCopyAlignment);
    assert(minimumBufferSize > kBufferCopyAlignment);

    const buffer = t.device.createBuffer({
      size: align(minimumBufferSize + offset, kBufferCopyAlignment),
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    const isSuccess = copyType === 'WriteTexture' ? true : offset % 4 === 0;

    if (copyType === 'CopyB2T') {
      t.testCopyBufferToTexture(
        { buffer, offset, bytesPerRow, rowsPerImage },
        { texture, aspect },
        textureSize,
        isSuccess
      );
    } else if (copyType === 'CopyT2B') {
      t.testCopyTextureToBuffer(
        { texture, aspect },
        { buffer, offset, bytesPerRow, rowsPerImage },
        textureSize,
        isSuccess
      );
    } else if (copyType === 'WriteTexture') {
      const uploadData = new Uint8Array(minimumBufferSize + offset);
      t.testWriteTexture(
        { texture, aspect },
        uploadData,
        {
          offset,
          bytesPerRow,
          rowsPerImage,
        },

        textureSize,
        isSuccess
      );
    } else {
      unreachable();
    }
  });

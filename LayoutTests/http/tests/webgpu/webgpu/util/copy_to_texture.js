/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert, memcpy, unreachable } from '../../common/util/util.js';
import { kTextureFormatInfo } from '../capability_info.js';
import { GPUTest } from '../gpu_test.js';

import { checkElementsEqual, checkElementsBetween } from './check_contents.js';
import { displayP3ToSrgb } from './color_space_conversion.js';
import { align } from './math.js';
import { kBytesPerRowAlignment } from './texture/layout.js';
import { kTexelRepresentationInfo } from './texture/texel_data.js';

function isFp16Format(format) {
  switch (format) {
    case 'r16float':
    case 'rg16float':
    case 'rgba16float':
      return true;
    default:
      return false;
  }
}

function isFp32Format(format) {
  switch (format) {
    case 'r32float':
    case 'rg32float':
    case 'rgba32float':
      return true;
    default:
      return false;
  }
}

function isUnormFormat(format) {
  switch (format) {
    case 'r8unorm':
    case 'rg8unorm':
    case 'rgba8unorm':
    case 'rgba8unorm-srgb':
    case 'bgra8unorm':
    case 'bgra8unorm-srgb':
    case 'rgb10a2unorm':
      return true;
    default:
      return false;
  }
}

export class CopyToTextureUtils extends GPUTest {
  doFlipY(sourcePixels, width, height, bytesPerPixel) {
    const dstPixels = new Uint8ClampedArray(width * height * bytesPerPixel);
    for (let i = 0; i < height; ++i) {
      for (let j = 0; j < width; ++j) {
        const srcPixelPos = i * width + j;
        // WebGL readPixel returns pixels from bottom-left origin. Using CopyExternalImageToTexture
        // to copy from WebGL Canvas keeps top-left origin. So the expectation from webgl.readPixel should
        // be flipped.
        const dstPixelPos = (height - i - 1) * width + j;

        memcpy(
          { src: sourcePixels, start: srcPixelPos * bytesPerPixel, length: bytesPerPixel },
          { dst: dstPixels, start: dstPixelPos * bytesPerPixel }
        );
      }
    }

    return dstPixels;
  }

  /**
   * If the destination format specifies a transfer function,
   * copyExternalImageToTexture (like B2T/T2T copies) should ignore it.
   */
  formatForExpectedPixels(format) {
    return format === 'rgba8unorm-srgb'
      ? 'rgba8unorm'
      : format === 'bgra8unorm-srgb'
      ? 'bgra8unorm'
      : format;
  }

  getSourceImageBitmapPixels(sourcePixels, width, height, isPremultiplied, isFlipY) {
    return this.getExpectedPixels(
      sourcePixels,
      width,
      height,
      'rgba8unorm',
      false,
      isPremultiplied,
      isFlipY
    );
  }

  getExpectedPixels(
    sourcePixels,
    width,
    height,
    format,
    srcPremultiplied,
    dstPremultiplied,
    isFlipY,
    srcColorSpace = 'srgb',
    dstColorSpace = 'srgb'
  ) {
    const bytesPerPixel = kTextureFormatInfo[format].bytesPerBlock;

    const orientedPixels = isFlipY ? this.doFlipY(sourcePixels, width, height, 4) : sourcePixels;
    const expectedPixels = new Uint8ClampedArray(bytesPerPixel * width * height);

    // Generate expectedPixels
    // Use getImageData and readPixels to get canvas contents.
    const rep = kTexelRepresentationInfo[format];
    const divide = 255.0;
    let rgba;
    const requireColorSpaceConversion = srcColorSpace !== dstColorSpace;
    const requireUnpremultiplyAlpha =
      requireColorSpaceConversion || (srcPremultiplied && !dstPremultiplied);
    const requirePremultiplyAlpha =
      requireColorSpaceConversion || (!srcPremultiplied && dstPremultiplied);
    for (let i = 0; i < height; ++i) {
      for (let j = 0; j < width; ++j) {
        const pixelPos = i * width + j;

        rgba = {
          R: orientedPixels[pixelPos * 4] / divide,
          G: orientedPixels[pixelPos * 4 + 1] / divide,
          B: orientedPixels[pixelPos * 4 + 2] / divide,
          A: orientedPixels[pixelPos * 4 + 3] / divide,
        };

        if (requireUnpremultiplyAlpha) {
          if (rgba.A !== 0.0) {
            rgba.R /= rgba.A;
            rgba.G /= rgba.A;
            rgba.B /= rgba.A;
          } else {
            assert(
              rgba.R === 0.0 && rgba.G === 0.0 && rgba.B === 0.0 && rgba.A === 0.0,
              'Unpremultiply ops with alpha value 0.0 requires all channels equals to 0.0'
            );
          }
        }

        if (requireColorSpaceConversion) {
          // WebGPU support 'srgb' as dstColorSpace only.
          if (srcColorSpace === 'display-p3' && dstColorSpace === 'srgb') {
            rgba = displayP3ToSrgb(rgba);
          } else {
            unreachable();
          }
        }

        if (requirePremultiplyAlpha) {
          rgba.R *= rgba.A;
          rgba.G *= rgba.A;
          rgba.B *= rgba.A;
        }

        // Clamp 0.0 around floats to 0.0
        if ((rgba.R > 0.0 && rgba.R < 1.0e-8) || (rgba.R < 0.0 && rgba.R > -1.0e-8)) {
          rgba.R = 0.0;
        }

        if ((rgba.G > 0.0 && rgba.G < 1.0e-8) || (rgba.G < 0.0 && rgba.G > -1.0e-8)) {
          rgba.G = 0.0;
        }

        if ((rgba.B > 0.0 && rgba.B < 1.0e-8) || (rgba.B < 0.0 && rgba.B > -1.0e-8)) {
          rgba.B = 0.0;
        }

        if (isUnormFormat(format)) {
          rgba.R = Math.max(0.0, Math.min(rgba.R, 1.0));
          rgba.G = Math.max(0.0, Math.min(rgba.G, 1.0));
          rgba.B = Math.max(0.0, Math.min(rgba.B, 1.0));
        }

        memcpy(
          { src: rep.pack(rep.encode(rgba)) },
          { dst: expectedPixels, start: pixelPos * bytesPerPixel }
        );
      }
    }

    return expectedPixels;
  }

  // MAINTENANCE_TODO(crbug.com/dawn/868): Should be possible to consolidate this along with texture checking
  checkCopyExternalImageResult(src, expected, width, height, bytesPerPixel, dstFormat) {
    const rowPitch = align(width * bytesPerPixel, kBytesPerRowAlignment);

    const readbackPromise = this.readGPUBufferRangeTyped(src, {
      type: Uint8Array,
      typedLength: rowPitch * height,
    });

    this.eventualAsyncExpectation(async niceStack => {
      const readback = await readbackPromise;
      const check = this.checkBufferWithRowPitch(
        readback.data,
        expected,
        width,
        height,
        rowPitch,
        bytesPerPixel,
        dstFormat
      );

      if (check !== undefined) {
        niceStack.message = check;
        this.rec.expectationFailed(niceStack);
      }
      readback.cleanup();
    });
  }

  // MAINTENANCE_TODO(crbug.com/dawn/868): Should be possible to consolidate this along with texture checking
  checkBufferWithRowPitch(actual, expected, width, height, rowPitch, bytesPerPixel, dstFormat) {
    const bytesPerRow = width * bytesPerPixel;

    if (isFp16Format(dstFormat)) {
      const expF16bits = new Uint16Array(
        expected.buffer,
        expected.byteOffset / Uint16Array.BYTES_PER_ELEMENT,
        expected.byteLength / Uint16Array.BYTES_PER_ELEMENT
      );

      const checkF16bits = new Uint16Array(
        actual.buffer,
        actual.byteOffset / Uint16Array.BYTES_PER_ELEMENT,
        actual.byteLength / Uint16Array.BYTES_PER_ELEMENT
      );

      for (let y = 0; y < height; ++y) {
        const expRowF16bits = expF16bits.subarray(
          (y * bytesPerRow) / expF16bits.BYTES_PER_ELEMENT,
          bytesPerRow / expF16bits.BYTES_PER_ELEMENT
        );

        const checkRowF16bits = checkF16bits.subarray(
          (y * bytesPerRow) / checkF16bits.BYTES_PER_ELEMENT,
          bytesPerRow / checkF16bits.BYTES_PER_ELEMENT
        );

        // 2701 is 0.0002 in float16. If the result is smaller than this, we
        // treat the value as 0;
        const checkResult = checkElementsBetween(checkRowF16bits, [
          i => (expRowF16bits[i] === 0 ? 0 : expRowF16bits[i] - 1),
          i => (expRowF16bits[i] === 0 ? 2701 : expRowF16bits[i] + 1),
        ]);

        if (checkResult !== undefined) return `on row ${y}: ${checkResult}`;
      }
    } else if (isFp32Format(dstFormat)) {
      const expF32 = new Float32Array(
        expected.buffer,
        expected.byteOffset / Float32Array.BYTES_PER_ELEMENT,
        expected.byteLength / Float32Array.BYTES_PER_ELEMENT
      );

      const checkF32 = new Float32Array(
        actual.buffer,
        actual.byteOffset / Float32Array.BYTES_PER_ELEMENT,
        actual.byteLength / Float32Array.BYTES_PER_ELEMENT
      );

      for (let y = 0; y < height; ++y) {
        const expRowF32 = expF32.subarray(
          (y * bytesPerRow) / expF32.BYTES_PER_ELEMENT,
          bytesPerRow / expF32.BYTES_PER_ELEMENT
        );

        const checkRowF32 = checkF32.subarray(
          (y * bytesPerRow) / checkF32.BYTES_PER_ELEMENT,
          bytesPerRow / checkF32.BYTES_PER_ELEMENT
        );

        const checkResult = checkElementsBetween(checkRowF32, [
          i => expRowF32[i] - 0.001,
          i => expRowF32[i] + 0.001,
        ]);

        if (checkResult !== undefined) return `on row ${y}: ${checkResult}`;
      }
    } else {
      for (let y = 0; y < height; ++y) {
        const exp = new Uint8Array(expected.buffer, expected.byteOffset, expected.byteLength);
        const check = new Uint8Array(actual.buffer, actual.byteOffset, actual.byteLength);

        const checkResult = checkElementsEqual(
          check.subarray(y * rowPitch, bytesPerRow),
          exp.subarray(y * bytesPerRow, bytesPerRow)
        );

        if (checkResult !== undefined) return `on row ${y}: ${checkResult}`;
      }
    }
    return undefined;
  }

  doTestAndCheckResult(
    imageCopyExternalImage,
    dstTextureCopyView,
    copySize,
    bytesPerPixel,
    expectedData,
    dstFormat
  ) {
    this.device.queue.copyExternalImageToTexture(
      imageCopyExternalImage,
      dstTextureCopyView,
      copySize
    );

    const externalImage = imageCopyExternalImage.source;
    const dstTexture = dstTextureCopyView.texture;

    const bytesPerRow = align(externalImage.width * bytesPerPixel, kBytesPerRowAlignment);
    const testBuffer = this.device.createBuffer({
      size: bytesPerRow * externalImage.height,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    this.trackForCleanup(testBuffer);

    const encoder = this.device.createCommandEncoder();

    encoder.copyTextureToBuffer(
      { texture: dstTexture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
      { buffer: testBuffer, bytesPerRow },
      { width: externalImage.width, height: externalImage.height, depthOrArrayLayers: 1 }
    );

    this.device.queue.submit([encoder.finish()]);

    this.checkCopyExternalImageResult(
      testBuffer,
      expectedData,
      externalImage.width,
      externalImage.height,
      bytesPerPixel,
      dstFormat
    );
  }
}

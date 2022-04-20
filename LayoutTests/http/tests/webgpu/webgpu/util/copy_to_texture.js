/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert, memcpy } from '../../common/util/util.js';
import { GPUTest } from '../gpu_test.js';

import { makeInPlaceColorConversion } from './color_space_conversion.js';
import { TexelView } from './texture/texel_view.js';
import { textureContentIsOKByT2B } from './texture/texture_ok.js';

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

  getExpectedPixels(sourcePixels, width, height, format, isFlipY, conversion) {
    const applyConversion = makeInPlaceColorConversion(conversion);

    const divide = 255.0;
    return TexelView.fromTexelsAsColors(
      format,
      coords => {
        assert(coords.x < width && coords.y < height && coords.z === 0, 'out of bounds');
        const y = isFlipY ? height - coords.y - 1 : coords.y;
        const pixelPos = y * width + coords.x;

        const rgba = {
          R: sourcePixels[pixelPos * 4] / divide,
          G: sourcePixels[pixelPos * 4 + 1] / divide,
          B: sourcePixels[pixelPos * 4 + 2] / divide,
          A: sourcePixels[pixelPos * 4 + 3] / divide,
        };

        applyConversion(rgba);
        return rgba;
      },
      { clampToFormatRange: true }
    );
  }

  doTestAndCheckResult(
    imageCopyExternalImage,
    dstTextureCopyView,
    expTexelView,
    copySize,
    texelCompareOptions
  ) {
    this.device.queue.copyExternalImageToTexture(
      imageCopyExternalImage,
      dstTextureCopyView,
      copySize
    );

    const resultPromise = textureContentIsOKByT2B(
      this,
      { texture: dstTextureCopyView.texture },
      copySize,
      { expTexelView },
      texelCompareOptions
    );

    this.eventualExpectOK(resultPromise);
  }
}

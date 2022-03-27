/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyExternalImageToTexture from ImageBitmaps created from various sources.

TODO: Test ImageBitmap generated from all possible ImageBitmapSource, relevant ImageBitmapOptions
    (https://html.spec.whatwg.org/multipage/imagebitmap-and-animations.html#images-2)
    and various source filetypes and metadata (weird dimensions, EXIF orientations, video rotations
    and visible/crop rectangles, etc. (In theory these things are handled inside createImageBitmap,
    but in theory could affect the internal representation of the ImageBitmap.)

TODO: Test zero-sized copies from all sources (just make sure params cover it) (e.g. 0x0, 0x4, 4x0).
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { unreachable } from '../../../common/util/util.js';
import { kTextureFormatInfo, kValidTextureFormatsForCopyE2T } from '../../capability_info.js';
import { CopyToTextureUtils } from '../../util/copy_to_texture.js';
import { kTexelRepresentationInfo } from '../../util/texture/texel_data.js';
var Color;

// Cache for generated pixels.
(function (Color) {
  Color[(Color['Red'] = 0)] = 'Red';
  Color[(Color['Green'] = 1)] = 'Green';
  Color[(Color['Blue'] = 2)] = 'Blue';
  Color[(Color['Black'] = 3)] = 'Black';
  Color[(Color['White'] = 4)] = 'White';
  Color[(Color['SemitransparentWhite'] = 5)] = 'SemitransparentWhite';
})(Color || (Color = {}));
const generatedPixelCache = new Map();

class F extends CopyToTextureUtils {
  generatePixel(color, format, hasTransparentPixels) {
    let formatEntry = generatedPixelCache.get(format);
    if (formatEntry === undefined) {
      formatEntry = new Map();
      generatedPixelCache.set(format, formatEntry);
    }

    const colorEntry = formatEntry.get(color);
    if (colorEntry === undefined) {
      // None of the dst texture format is 'uint' or 'sint', so we can always use float value.
      const rep = kTexelRepresentationInfo[format];
      let rgba;
      switch (color) {
        case Color.Red:
          rgba = { R: 1.0, G: 0.0, B: 0.0, A: 1.0 };
          break;
        case Color.Green:
          rgba = { R: 0.0, G: 1.0, B: 0.0, A: 1.0 };
          break;
        case Color.Blue:
          rgba = { R: 0.0, G: 0.0, B: 1.0, A: 1.0 };
          break;
        case Color.Black:
          rgba = { R: 0.0, G: 0.0, B: 0.0, A: 1.0 };
          break;
        case Color.White:
          rgba = { R: 1.0, G: 1.0, B: 1.0, A: 1.0 };
          break;
        case Color.SemitransparentWhite:
          rgba = { R: 1.0, G: 1.0, B: 1.0, A: 0.6 };
          break;
        default:
          unreachable();
      }

      const pixels = new Uint8Array(rep.pack(rep.encode(rgba)));
      formatEntry.set(color, pixels);
    }

    return formatEntry.get(color);
  }

  // Helper functions to generate imagePixels based input configs.
  getImagePixels({ format, width, height, hasTransparentPixels }) {
    const bytesPerPixel = kTextureFormatInfo[format].bytesPerBlock;

    // Generate input contents by iterating 'Color' enum
    const imagePixels = new Uint8ClampedArray(bytesPerPixel * width * height);
    const testColors = [Color.Red, Color.Green, Color.Blue, Color.Black, Color.White];
    if (hasTransparentPixels) testColors.push(Color.SemitransparentWhite);

    for (let i = 0; i < height; ++i) {
      for (let j = 0; j < width; ++j) {
        const pixelPos = i * width + j;
        const currentPixel = testColors[pixelPos % testColors.length];
        const pixelData = this.generatePixel(currentPixel, format, hasTransparentPixels);
        imagePixels.set(pixelData, pixelPos * bytesPerPixel);
      }
    }

    return imagePixels;
  }
}

export const g = makeTestGroup(F);

g.test('from_ImageData')
  .desc(
    `
  Test ImageBitmap generated from ImageData can be copied to WebGPU
  texture correctly. These imageBitmaps are highly possible living
  in CPU back resource.

  It generates pixels in ImageData one by one based on a color list:
  [Red, Green, Blue, Black, White, SemitransparentWhite].

  Then call copyExternalImageToTexture() to do a full copy to the 0 mipLevel
  of dst texture, and read the contents out to compare with the ImageBitmap contents.

  Do premultiply alpha during copy if 'premultipliedAlpha' in 'GPUImageCopyTextureTagged'
  is set to 'true' and do unpremultiply alpha if it is set to 'false'.

  If 'flipY' in 'GPUImageCopyExternalImage' is set to 'true', copy will ensure the result
  is flipped.

  The tests covers:
  - Valid canvas type
  - Source WebGPU Canvas lives in the same GPUDevice or different GPUDevice as test
  - Valid dstColorFormat of copyExternalImageToTexture()
  - Valid source image alphaMode
  - Valid dest alphaMode
  - Valid 'flipY' config in 'GPUImageCopyExternalImage' (named 'srcDoFlipYDuringCopy' in cases)
  - TODO(#913): color space tests need to be added
  - TODO: Add error tolerance for rgb10a2unorm dst texture format

  And the expected results are all passed.
  `
  )
  .params(u =>
    u
      .combine('alpha', ['none', 'premultiply'])
      .combine('orientation', ['none', 'flipY'])
      .combine('srcDoFlipYDuringCopy', [true, false])
      .combine('dstColorFormat', kValidTextureFormatsForCopyE2T)
      .combine('dstPremultiplied', [true, false])
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const {
      width,
      height,
      alpha,
      orientation,
      dstColorFormat,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
    } = t.params;

    // Generate input contents by iterating 'Color' enum
    const imagePixels = t.getImagePixels({
      format: 'rgba8unorm',
      width,
      height,
      hasTransparentPixels: true,
    });

    // Generate correct expected values
    const imageData = new ImageData(imagePixels, width, height);
    const imageBitmap = await createImageBitmap(imageData, {
      premultiplyAlpha: alpha,
      imageOrientation: orientation,
    });

    const dst = t.device.createTexture({
      size: {
        width: imageBitmap.width,
        height: imageBitmap.height,
        depthOrArrayLayers: 1,
      },

      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Construct expected value for different dst color format
    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;
    const srcPremultiplied = alpha === 'premultiply';
    const sourceImageBitmapPixels = t.getSourceImageBitmapPixels(
      imagePixels,
      width,
      height,
      srcPremultiplied,
      orientation === 'flipY'
    );

    const format = kTextureFormatInfo[dstColorFormat].baseFormat ?? dstColorFormat;

    const expectedPixels = t.getExpectedPixels(
      sourceImageBitmapPixels,
      width,
      height,
      format,
      srcPremultiplied,
      dstPremultiplied,
      srcDoFlipYDuringCopy
    );

    t.doTestAndCheckResult(
      { source: imageBitmap, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      { width: imageBitmap.width, height: imageBitmap.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels,
      dstColorFormat
    );
  });

g.test('from_canvas')
  .desc(
    `
  Test ImageBitmap generated from canvas/offscreenCanvas can be copied to WebGPU
  texture correctly. These imageBitmaps are highly possible living in GPU back resource.

  It generates pixels in ImageData one by one based on a color list:
  [Red, Green, Blue, Black, White].

  Then call copyExternalImageToTexture() to do a full copy to the 0 mipLevel
  of dst texture, and read the contents out to compare with the ImageBitmap contents.

  Do premultiply alpha during copy if 'premultipliedAlpha' in 'GPUImageCopyTextureTagged'
  is set to 'true' and do unpremultiply alpha if it is set to 'false'.

  If 'flipY' in 'GPUImageCopyExternalImage' is set to 'true', copy will ensure the result
  is flipped.

  The tests covers:
  - Valid canvas type
  - Source WebGPU Canvas lives in the same GPUDevice or different GPUDevice as test
  - Valid dstColorFormat of copyExternalImageToTexture()
  - Valid source image alphaMode
  - Valid dest alphaMode
  - Valid 'flipY' config in 'GPUImageCopyExternalImage' (named 'srcDoFlipYDuringCopy' in cases)
  - TODO(#913): color space tests need to be added
  - TODO: Add error tolerance for rgb10a2unorm dst texture format

  And the expected results are all passed.
  `
  )
  .params(u =>
    u
      .combine('orientation', ['none', 'flipY'])
      .combine('srcDoFlipYDuringCopy', [true, false])
      .combine('dstColorFormat', kValidTextureFormatsForCopyE2T)
      .combine('dstPremultiplied', [true, false])
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const {
      width,
      height,
      orientation,
      dstColorFormat,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
    } = t.params;

    // CTS sometimes runs on worker threads, where document is not available.
    // In this case, OffscreenCanvas can be used instead of <canvas>.
    // But some browsers don't support OffscreenCanvas, and some don't
    // support '2d' contexts on OffscreenCanvas.
    // In this situation, the case will be skipped.
    let imageCanvas;
    if (typeof document !== 'undefined') {
      imageCanvas = document.createElement('canvas');
      imageCanvas.width = width;
      imageCanvas.height = height;
    } else if (typeof OffscreenCanvas === 'undefined') {
      t.skip('OffscreenCanvas is not supported');
      return;
    } else {
      imageCanvas = new OffscreenCanvas(width, height);
    }
    const imageCanvasContext = imageCanvas.getContext('2d');
    if (imageCanvasContext === null) {
      t.skip('OffscreenCanvas "2d" context not available');
      return;
    }

    // Generate non-transparent pixel data to avoid canvas
    // different opt behaviour on putImageData()
    // from browsers.
    const imagePixels = t.getImagePixels({
      format: 'rgba8unorm',
      width,
      height,
      hasTransparentPixels: false,
    });

    const imageData = new ImageData(imagePixels, width, height);

    // Use putImageData to prevent color space conversion.
    imageCanvasContext.putImageData(imageData, 0, 0);

    // MAINTENANCE_TODO: Workaround for @types/offscreencanvas missing an overload of
    // `createImageBitmap` that takes `ImageBitmapOptions`.
    const imageBitmap = await createImageBitmap(imageCanvas, {
      premultiplyAlpha: 'premultiply',
      imageOrientation: orientation,
    });

    const dst = t.device.createTexture({
      size: {
        width: imageBitmap.width,
        height: imageBitmap.height,
        depthOrArrayLayers: 1,
      },

      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;

    const sourceImageBitmapPixels = t.getSourceImageBitmapPixels(
      imagePixels,
      width,
      height,
      true,
      orientation === 'flipY'
    );

    const format = kTextureFormatInfo[dstColorFormat].baseFormat ?? dstColorFormat;

    const expectedPixels = t.getExpectedPixels(
      sourceImageBitmapPixels,
      width,
      height,
      format,
      true,
      dstPremultiplied,
      srcDoFlipYDuringCopy
    );

    t.doTestAndCheckResult(
      { source: imageBitmap, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      { width: imageBitmap.width, height: imageBitmap.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels,
      dstColorFormat
    );
  });

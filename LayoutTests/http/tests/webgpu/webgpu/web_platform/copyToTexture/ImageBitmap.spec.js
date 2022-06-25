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
import { kTextureFormatInfo, kValidTextureFormatsForCopyE2T } from '../../capability_info.js';
import { CopyToTextureUtils } from '../../util/copy_to_texture.js';

import { TexelView } from '../../util/texture/texel_view.js';

// None of the dst texture format is 'uint' or 'sint', so we can always use float value.
const kColors = {
  Red: { R: 1.0, G: 0.0, B: 0.0, A: 1.0 },
  Green: { R: 0.0, G: 1.0, B: 0.0, A: 1.0 },
  Blue: { R: 0.0, G: 0.0, B: 1.0, A: 1.0 },
  Black: { R: 0.0, G: 0.0, B: 0.0, A: 1.0 },
  White: { R: 1.0, G: 1.0, B: 1.0, A: 1.0 },
  SemitransparentWhite: { R: 1.0, G: 1.0, B: 1.0, A: 0.6 },
};

const kTestColorsOpaque = [kColors.Red, kColors.Green, kColors.Blue, kColors.Black, kColors.White];

const kTestColorsAll = [...kTestColorsOpaque, kColors.SemitransparentWhite];

function makeTestColorsTexelView({ testColors, format, width, height, premultiplied, flipY }) {
  return TexelView.fromTexelsAsColors(format, coords => {
    const y = flipY ? height - coords.y - 1 : coords.y;
    const pixelPos = y * width + coords.x;
    const currentPixel = testColors[pixelPos % testColors.length];

    if (premultiplied && currentPixel.A !== 1.0) {
      return {
        R: currentPixel.R * currentPixel.A,
        G: currentPixel.G * currentPixel.A,
        B: currentPixel.B * currentPixel.A,
        A: currentPixel.A,
      };
    } else {
      return currentPixel;
    }
  });
}

export const g = makeTestGroup(CopyToTextureUtils);

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

    const testColors = kTestColorsAll;

    // Generate correct expected values
    const texelViewSource = makeTestColorsTexelView({
      testColors,
      format: 'rgba8unorm', // ImageData is always in rgba8unorm format.
      width,
      height,
      flipY: false,
      premultiplied: false,
    });

    const imageData = new ImageData(width, height);
    texelViewSource.writeTextureData(imageData.data, {
      bytesPerRow: width * 4,
      rowsPerImage: height,
      subrectOrigin: [0, 0],
      subrectSize: { width, height },
    });

    const imageBitmap = await createImageBitmap(imageData, {
      premultiplyAlpha: alpha,
      imageOrientation: orientation,
    });

    const dst = t.device.createTexture({
      size: { width, height },
      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const expFormat = kTextureFormatInfo[dstColorFormat].baseFormat ?? dstColorFormat;
    const expectFlipped = srcDoFlipYDuringCopy !== (orientation === 'flipY');
    const texelViewExpected = makeTestColorsTexelView({
      testColors,
      format: expFormat,
      width,
      height,
      flipY: expectFlipped,
      premultiplied: dstPremultiplied,
    });

    t.doTestAndCheckResult(
      { source: imageBitmap, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      texelViewExpected,
      { width, height, depthOrArrayLayers: 1 },
      { maxDiffULPsForFloatFormat: 0, maxDiffULPsForNormFormat: 0 }
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
    const texelViewSource = makeTestColorsTexelView({
      testColors: kTestColorsOpaque,
      format: 'rgba8unorm', // ImageData is always in rgba8unorm format.
      width,
      height,
      flipY: false,
      premultiplied: false,
    });

    // Generate correct expected values
    const imageData = new ImageData(width, height);
    texelViewSource.writeTextureData(imageData.data, {
      bytesPerRow: width * 4,
      rowsPerImage: height,
      subrectOrigin: [0, 0],
      subrectSize: { width, height },
    });

    // Use putImageData to prevent color space conversion.
    imageCanvasContext.putImageData(imageData, 0, 0);

    // MAINTENANCE_TODO: Workaround for @types/offscreencanvas missing an overload of
    // `createImageBitmap` that takes `ImageBitmapOptions`.
    const imageBitmap = await createImageBitmap(imageCanvas, {
      premultiplyAlpha: 'premultiply',
      imageOrientation: orientation,
    });

    const dst = t.device.createTexture({
      size: { width, height },
      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const expFormat = kTextureFormatInfo[dstColorFormat].baseFormat ?? dstColorFormat;
    const expectFlipped = srcDoFlipYDuringCopy !== (orientation === 'flipY');
    const texelViewExpected = makeTestColorsTexelView({
      testColors: kTestColorsOpaque,
      format: expFormat,
      width,
      height,
      flipY: expectFlipped,
      premultiplied: dstPremultiplied,
    });

    t.doTestAndCheckResult(
      { source: imageBitmap, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      texelViewExpected,
      { width, height, depthOrArrayLayers: 1 },
      { maxDiffULPsForFloatFormat: 0, maxDiffULPsForNormFormat: 0 }
    );
  });

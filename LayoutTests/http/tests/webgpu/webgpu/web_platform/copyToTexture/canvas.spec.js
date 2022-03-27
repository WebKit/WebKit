/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyToTexture with HTMLCanvasElement and OffscreenCanvas sources.

TODO: Add tests for flipY
`;
import { getResourcePath } from '../../../common/framework/resources.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { unreachable } from '../../../common/util/util.js';
import { kTextureFormatInfo, kValidTextureFormatsForCopyE2T } from '../../capability_info.js';
import { CopyToTextureUtils } from '../../util/copy_to_texture.js';
import { allCanvasTypes, createCanvas } from '../../util/create_elements.js';

class F extends CopyToTextureUtils {
  init2DCanvasContentWithColorSpace({ width, height, paintOpaqueRects, colorSpace }) {
    const canvas = createCanvas(this, 'onscreen', width, height);

    let canvasContext = null;
    canvasContext = canvas.getContext('2d', { colorSpace });

    if (canvasContext === null) {
      this.skip('onscreen canvas 2d context not available');
    }

    if (
      typeof canvasContext.getContextAttributes === 'undefined' ||
      typeof canvasContext.getContextAttributes().colorSpace === 'undefined'
    ) {
      this.skip('color space attr is not supported for canvas 2d context');
    }

    const SOURCE_PIXEL_BYTES = 4;
    const imagePixels = new Uint8ClampedArray(SOURCE_PIXEL_BYTES * width * height);

    const rectWidth = Math.floor(width / 2);
    const rectHeight = Math.floor(height / 2);

    const alphaValue = paintOpaqueRects ? 255 : 153;

    let pixelStartPos = 0;
    // Red;
    for (let i = 0; i < rectHeight; ++i) {
      for (let j = 0; j < rectWidth; ++j) {
        pixelStartPos = (i * width + j) * SOURCE_PIXEL_BYTES;
        imagePixels[pixelStartPos] = 255;
        imagePixels[pixelStartPos + 1] = 0;
        imagePixels[pixelStartPos + 2] = 0;
        imagePixels[pixelStartPos + 3] = alphaValue;
      }
    }

    // Lime;
    for (let i = 0; i < rectHeight; ++i) {
      for (let j = rectWidth; j < width; ++j) {
        pixelStartPos = (i * width + j) * SOURCE_PIXEL_BYTES;
        imagePixels[pixelStartPos] = 0;
        imagePixels[pixelStartPos + 1] = 255;
        imagePixels[pixelStartPos + 2] = 0;
        imagePixels[pixelStartPos + 3] = alphaValue;
      }
    }

    // Blue
    for (let i = rectHeight; i < height; ++i) {
      for (let j = 0; j < rectWidth; ++j) {
        pixelStartPos = (i * width + j) * SOURCE_PIXEL_BYTES;
        imagePixels[pixelStartPos] = 0;
        imagePixels[pixelStartPos + 1] = 0;
        imagePixels[pixelStartPos + 2] = 255;
        imagePixels[pixelStartPos + 3] = alphaValue;
      }
    }

    // Fuchsia
    for (let i = rectHeight; i < height; ++i) {
      for (let j = rectWidth; j < width; ++j) {
        pixelStartPos = (i * width + j) * SOURCE_PIXEL_BYTES;
        imagePixels[pixelStartPos] = 255;
        imagePixels[pixelStartPos + 1] = 0;
        imagePixels[pixelStartPos + 2] = 255;
        imagePixels[pixelStartPos + 3] = alphaValue;
      }
    }

    const imageData = new ImageData(imagePixels, width, height, { colorSpace });
    // MAINTENANCE_TODO: Remove as any when tsc support imageData.colorSpace

    if (typeof imageData.colorSpace === 'undefined') {
      this.skip('color space attr is not supported for ImageData');
    }

    const ctx = canvasContext;
    ctx.putImageData(imageData, 0, 0);

    return { canvas, canvasContext };
  }

  // MAINTENANCE_TODO: Cache the generated canvas to avoid duplicated initialization.
  init2DCanvasContent({ canvasType, width, height, paintOpaqueRects }) {
    const canvas = createCanvas(this, canvasType, width, height);

    let canvasContext = null;
    canvasContext = canvas.getContext('2d');

    if (canvasContext === null) {
      this.skip(canvasType + ' canvas 2d context not available');
    }

    // The rgb10a2unorm dst texture will have tiny errors when we compare actual and expectation.
    // This is due to the convert from 8-bit to 10-bit combined with alpha value ops. So for
    // rgb10a2unorm dst textures, we'll set alphaValue to 1.0 to test.
    const alphaValue = paintOpaqueRects ? 1.0 : 0.6;
    const ctx = canvasContext;
    this.paint2DCanvas(ctx, width, height, alphaValue);

    return { canvas, canvasContext };
  }

  paint2DCanvas(ctx, width, height, alphaValue) {
    const rectWidth = Math.floor(width / 2);
    const rectHeight = Math.floor(height / 2);

    // Red
    ctx.fillStyle = `rgba(255, 0, 0, ${alphaValue})`;
    ctx.fillRect(0, 0, rectWidth, rectHeight);
    // Lime
    ctx.fillStyle = `rgba(0, 255, 0, ${alphaValue})`;
    ctx.fillRect(rectWidth, 0, width - rectWidth, rectHeight);
    // Blue
    ctx.fillStyle = `rgba(0, 0, 255, ${alphaValue})`;
    ctx.fillRect(0, rectHeight, rectWidth, height - rectHeight);
    // Fuchsia
    ctx.fillStyle = `rgba(255, 0, 255, ${alphaValue})`;
    ctx.fillRect(rectWidth, rectHeight, width - rectWidth, height - rectHeight);
  }

  // MAINTENANCE_TODO: Cache the generated canvas to avoid duplicated initialization.
  initGLCanvasContent({ canvasType, contextName, width, height, premultiplied, paintOpaqueRects }) {
    const canvas = createCanvas(this, canvasType, width, height);

    // MAINTENANCE_TODO: Workaround for @types/offscreencanvas missing an overload of
    // `OffscreenCanvas.getContext` that takes `string` or a union of context types.
    const gl = canvas.getContext(contextName, {
      premultipliedAlpha: premultiplied,
    });

    if (gl === null) {
      this.skip(canvasType + ' canvas ' + contextName + ' context not available');
    }
    this.trackForCleanup(gl);

    const rectWidth = Math.floor(width / 2);
    const rectHeight = Math.floor(height / 2);

    const alphaValue = paintOpaqueRects ? 1.0 : 0.6;
    const colorValue = premultiplied ? alphaValue : 1.0;

    // For webgl/webgl2 context canvas, if the context created with premultipliedAlpha attributes,
    // it means that the value in drawing buffer is premultiplied or not. So we should set
    // premultipliedAlpha value for premultipliedAlpha true gl context and unpremultipliedAlpha value
    // for the premultipliedAlpha false gl context.
    gl.enable(gl.SCISSOR_TEST);
    gl.scissor(0, 0, rectWidth, rectHeight);
    gl.clearColor(colorValue, 0.0, 0.0, alphaValue);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.scissor(rectWidth, 0, width - rectWidth, rectHeight);
    gl.clearColor(0.0, colorValue, 0.0, alphaValue);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.scissor(0, rectHeight, rectWidth, height - rectHeight);
    gl.clearColor(0.0, 0.0, colorValue, alphaValue);
    gl.clear(gl.COLOR_BUFFER_BIT);

    gl.scissor(rectWidth, rectHeight, width - rectWidth, height - rectHeight);
    gl.clearColor(colorValue, colorValue, colorValue, alphaValue);
    gl.clear(gl.COLOR_BUFFER_BIT);

    return { canvas, canvasContext: gl };
  }

  getInitGPUCanvasData(width, height, premultiplied, paintOpaqueRects) {
    const rectWidth = Math.floor(width / 2);
    const rectHeight = Math.floor(height / 2);

    const alphaValue = paintOpaqueRects ? 255 : 153;
    const colorValue = premultiplied ? alphaValue : 255;

    // BGRA8Unorm texture
    const initialData = new Uint8ClampedArray(4 * width * height);
    const maxRectHeightIndex = width * rectHeight;
    for (let pixelIndex = 0; pixelIndex < initialData.length / 4; ++pixelIndex) {
      const index = pixelIndex * 4;

      // Top-half two rectangles
      if (pixelIndex < maxRectHeightIndex) {
        // top-left side rectangle
        if (pixelIndex % width < rectWidth) {
          // top-left side rectangle
          initialData[index] = colorValue;
          initialData[index + 1] = 0;
          initialData[index + 2] = 0;
          initialData[index + 3] = alphaValue;
        } else {
          // top-right side rectangle
          initialData[index] = 0;
          initialData[index + 1] = colorValue;
          initialData[index + 2] = 0;
          initialData[index + 3] = alphaValue;
        }
      } else {
        // Bottom-half two rectangles
        // bottom-left side rectangle
        if (pixelIndex % width < rectWidth) {
          initialData[index] = 0;
          initialData[index + 1] = 0;
          initialData[index + 2] = colorValue;
          initialData[index + 3] = alphaValue;
        } else {
          // bottom-right side rectangle
          initialData[index] = colorValue;
          initialData[index + 1] = colorValue;
          initialData[index + 2] = colorValue;
          initialData[index + 3] = alphaValue;
        }
      }
    }
    return initialData;
  }

  initGPUCanvasContent({ device, canvasType, width, height, premultiplied, paintOpaqueRects }) {
    const canvas = createCanvas(this, canvasType, width, height);

    const gpuContext = canvas.getContext('webgpu');

    if (gpuContext === null) {
      this.skip(canvasType + ' canvas webgpu context not available');
    }

    const alphaMode = premultiplied ? 'premultiplied' : 'opaque';

    gpuContext.configure({
      device,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC,
      compositingAlphaMode: alphaMode,
    });

    // BGRA8Unorm texture
    const initialData = this.getInitGPUCanvasData(width, height, premultiplied, paintOpaqueRects);
    const canvasTexture = gpuContext.getCurrentTexture();
    device.queue.writeTexture(
      { texture: canvasTexture },
      initialData,
      {
        bytesPerRow: width * 4,
        rowsPerImage: height,
      },

      {
        width,
        height,
        depthOrArrayLayers: 1,
      }
    );

    return { canvas };
  }

  getSourceCanvas2DContent(context, width, height) {
    // Always read back the raw data from canvas
    return context.getImageData(0, 0, width, height).data;
  }

  getSourceCanvasGLContent(gl, width, height) {
    const bytesPerPixel = 4;

    const sourcePixels = new Uint8ClampedArray(width * height * bytesPerPixel);
    gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, sourcePixels);

    return this.doFlipY(sourcePixels, width, height, bytesPerPixel);
  }

  calculateSourceContentOnCPU(width, height, premultipliedAlpha, paintOpaqueRects) {
    const bytesPerPixel = 4;

    const rgbaPixels = this.getInitGPUCanvasData(
      width,
      height,
      premultipliedAlpha,
      paintOpaqueRects
    );

    // The source canvas has bgra8unorm back resource. We
    // swizzle the channels to align with 2d/webgl canvas and
    // clear alpha to opaque when context compositingAlphaMode
    // is set to opaque (follow webgpu spec).
    for (let i = 0; i < height; ++i) {
      for (let j = 0; j < width; ++j) {
        const pixelPos = i * width + j;
        const r = rgbaPixels[pixelPos * bytesPerPixel + 2];
        if (!premultipliedAlpha) {
          rgbaPixels[pixelPos * bytesPerPixel + 3] = 255;
        }

        rgbaPixels[pixelPos * bytesPerPixel + 2] = rgbaPixels[pixelPos * bytesPerPixel];
        rgbaPixels[pixelPos * bytesPerPixel] = r;
      }
    }

    return rgbaPixels;
  }

  getTestImageURLByColorSpace(colorSpace) {
    switch (colorSpace) {
      case 'srgb':
        return getResourcePath('Webkit-logo-sRGB.png');
      case 'display-p3':
        return getResourcePath('Webkit-logo-P3.png');
      default:
        unreachable();
    }
  }
}

export const g = makeTestGroup(F);

g.test('copy_contents_from_2d_context_canvas')
  .desc(
    `
  Test HTMLCanvasElement and OffscreenCanvas with 2d context
  can be copied to WebGPU texture correctly.

  It creates HTMLCanvasElement/OffscreenCanvas with '2d'.
  Use fillRect(2d context) to render red rect for top-left,
  green rect for top-right, blue rect for bottom-left and white for bottom-right.

  Then call copyExternalImageToTexture() to do a full copy to the 0 mipLevel
  of dst texture, and read the contents out to compare with the canvas contents.

  Provide premultiplied input if 'premultipliedAlpha' in 'GPUImageCopyTextureTagged'
  is set to 'true' and unpremultiplied input if it is set to 'false'.

  If 'flipY' in 'GPUImageCopyExternalImage' is set to 'true', copy will ensure the result
  is flipped.

  The tests covers:
  - Valid canvas type
  - Valid 2d context type
  - Valid dstColorFormat of copyExternalImageToTexture()
  - Valid dest alphaMode
  - Valid 'flipY' config in 'GPUImageCopyExternalImage' (named 'srcDoFlipYDuringCopy' in cases)
  - TODO(#913): color space tests need to be added
  - TODO: Add error tolerance for rgb10a2unorm dst texture format

  And the expected results are all passed.
  `
  )
  .params(u =>
    u
      .combine('canvasType', allCanvasTypes)
      .combine('dstColorFormat', kValidTextureFormatsForCopyE2T)
      .combine('dstPremultiplied', [true, false])
      .combine('srcDoFlipYDuringCopy', [true, false])
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const {
      width,
      height,
      canvasType,
      dstColorFormat,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
    } = t.params;

    // When dst texture format is rgb10a2unorm, the generated expected value of the result
    // may have tiny errors compared to the actual result when the channel value is
    // not 1.0 or 0.0.
    // For example, we init the pixel with r channel to 0.6. And the denormalized value for
    // 10-bit channel is 613.8, which needs to call "round" or other function to get an integer.
    // It is possible that gpu adopt different "round" as our cpu implementation(we use Math.round())
    // and it will generate tiny errors.
    // So the cases with rgb10a2unorm dst texture format are handled specially by painting opaque rects
    // to ensure they will have stable result after alphaOps(should keep the same value).
    const { canvas, canvasContext } = t.init2DCanvasContent({
      canvasType,
      width,
      height,
      paintOpaqueRects: dstColorFormat === 'rgb10a2unorm',
    });

    const dst = t.device.createTexture({
      size: {
        width,
        height,
        depthOrArrayLayers: 1,
      },

      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Construct expected value for different dst color format
    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;
    const format =
      kTextureFormatInfo[dstColorFormat].baseFormat !== undefined
        ? kTextureFormatInfo[dstColorFormat].baseFormat
        : dstColorFormat;

    // For 2d canvas, get expected pixels with getImageData(), which returns unpremultiplied
    // values.
    const sourcePixels = t.getSourceCanvas2DContent(canvasContext, width, height);
    const expectedPixels = t.getExpectedPixels(
      sourcePixels,
      width,
      height,
      format,
      false,
      dstPremultiplied,
      srcDoFlipYDuringCopy
    );

    t.doTestAndCheckResult(
      { source: canvas, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      { width: canvas.width, height: canvas.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels,
      dstColorFormat
    );
  });

g.test('copy_contents_from_gl_context_canvas')
  .desc(
    `
  Test HTMLCanvasElement and OffscreenCanvas with webgl/webgl2 context
  can be copied to WebGPU texture correctly.

  It creates HTMLCanvasElement/OffscreenCanvas with webgl'/'webgl2'.
  Use scissor + clear to render red rect for top-left, green rect
  for top-right, blue rect for bottom-left and white for bottom-right.
  And do premultiply alpha in advance if the webgl/webgl2 context is created
  with premultipliedAlpha : true.

  Then call copyExternalImageToTexture() to do a full copy to the 0 mipLevel
  of dst texture, and read the contents out to compare with the canvas contents.

  Provide premultiplied input if 'premultipliedAlpha' in 'GPUImageCopyTextureTagged'
  is set to 'true' and unpremultiplied input if it is set to 'false'.

  If 'flipY' in 'GPUImageCopyExternalImage' is set to 'true', copy will ensure the result
  is flipped.

  The tests covers:
  - Valid canvas type
  - Valid webgl/webgl2 context type
  - Valid dstColorFormat of copyExternalImageToTexture()
  - Valid source image alphaMode
  - Valid dest alphaMode
  - Valid 'flipY' config in 'GPUImageCopyExternalImage'(named 'srcDoFlipYDuringCopy' in cases)
  - TODO: color space tests need to be added
  - TODO: Add error tolerance for rgb10a2unorm dst texture format

  And the expected results are all passed.
  `
  )
  .params(u =>
    u
      .combine('canvasType', allCanvasTypes)
      .combine('contextName', ['webgl', 'webgl2'])
      .combine('dstColorFormat', kValidTextureFormatsForCopyE2T)
      .combine('srcPremultiplied', [true, false])
      .combine('dstPremultiplied', [true, false])
      .combine('srcDoFlipYDuringCopy', [true, false])
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const {
      width,
      height,
      canvasType,
      contextName,
      dstColorFormat,
      srcPremultiplied,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
    } = t.params;

    // When dst texture format is rgb10a2unorm, the generated expected value of the result
    // may have tiny errors compared to the actual result when the channel value is
    // not 1.0 or 0.0.
    // For example, we init the pixel with r channel to 0.6. And the denormalized value for
    // 10-bit channel is 613.8, which needs to call "round" or other function to get an integer.
    // It is possible that gpu adopt different "round" as our cpu implementation(we use Math.round())
    // and it will generate tiny errors.
    // So the cases with rgb10a2unorm dst texture format are handled specially by by painting opaque rects
    // to ensure they will have stable result after alphaOps(should keep the same value).
    const { canvas, canvasContext } = t.initGLCanvasContent({
      canvasType,
      contextName,
      width,
      height,
      premultiplied: srcPremultiplied,
      paintOpaqueRects: dstColorFormat === 'rgb10a2unorm',
    });

    const dst = t.device.createTexture({
      size: {
        width,
        height,
        depthOrArrayLayers: 1,
      },

      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Construct expected value for different dst color format
    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;
    const format =
      kTextureFormatInfo[dstColorFormat].baseFormat !== undefined
        ? kTextureFormatInfo[dstColorFormat].baseFormat
        : dstColorFormat;
    const sourcePixels = t.getSourceCanvasGLContent(canvasContext, width, height);
    const expectedPixels = t.getExpectedPixels(
      sourcePixels,
      width,
      height,
      format,
      srcPremultiplied,
      dstPremultiplied,
      srcDoFlipYDuringCopy
    );

    t.doTestAndCheckResult(
      { source: canvas, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      { width: canvas.width, height: canvas.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels,
      dstColorFormat
    );
  });

g.test('copy_contents_from_gpu_context_canvas')
  .desc(
    `
  Test HTMLCanvasElement and OffscreenCanvas with webgpu context
  can be copied to WebGPU texture correctly.

  It creates HTMLCanvasElement/OffscreenCanvas with 'webgpu'.
  Use writeTexture to copy pixels to back buffer. The results are:
  red rect for top-left, green rect for top-right, blue rect for bottom-left
  and white for bottom-right.

  And do premultiply alpha in advance if the webgpu context is created
  with compositingAlphaMode="premultiplied".

  Then call copyExternalImageToTexture() to do a full copy to the 0 mipLevel
  of dst texture, and read the contents out to compare with the canvas contents.

  Provide premultiplied input if 'premultipliedAlpha' in 'GPUImageCopyTextureTagged'
  is set to 'true' and unpremultiplied input if it is set to 'false'.

  If 'flipY' in 'GPUImageCopyExternalImage' is set to 'true', copy will ensure the result
  is flipped.

  The tests covers:
  - Valid canvas type
  - Source WebGPU Canvas lives in the same GPUDevice or different GPUDevice as test
  - Valid dstColorFormat of copyExternalImageToTexture()
  - Valid source image alphaMode
  - Valid dest alphaMode
  - Valid 'flipY' config in 'GPUImageCopyExternalImage'(named 'srcDoFlipYDuringCopy' in cases)
  - TODO: color space tests need to be added
  - TODO: Add error tolerance for rgb10a2unorm dst texture format

  And the expected results are all passed.
  `
  )
  .params(u =>
    u
      .combine('canvasType', allCanvasTypes)
      .combine('srcAndDstInSameGPUDevice', [true, false])
      .combine('dstColorFormat', kValidTextureFormatsForCopyE2T)
      .combine('srcPremultiplied', [true])
      .combine('dstPremultiplied', [true, false])
      .combine('srcDoFlipYDuringCopy', [true, false])
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const {
      width,
      height,
      canvasType,
      srcAndDstInSameGPUDevice,
      dstColorFormat,
      srcPremultiplied,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
    } = t.params;

    let device;

    if (!srcAndDstInSameGPUDevice) {
      await t.selectMismatchedDeviceOrSkipTestCase(undefined);
      device = t.mismatchedDevice;
    } else {
      device = t.device;
    }

    // When dst texture format is rgb10a2unorm, the generated expected value of the result
    // may have tiny errors compared to the actual result when the channel value is
    // not 1.0 or 0.0.
    // For example, we init the pixel with r channel to 0.6. And the denormalized value for
    // 10-bit channel is 613.8, which needs to call "round" or other function to get an integer.
    // It is possible that gpu adopt different "round" as our cpu implementation(we use Math.round())
    // and it will generate tiny errors.
    // So the cases with rgb10a2unorm dst texture format are handled specially by by painting opaque rects
    // to ensure they will have stable result after alphaOps(should keep the same value).
    const { canvas } = t.initGPUCanvasContent({
      device,
      canvasType,
      width,
      height,
      premultiplied: srcPremultiplied,
      paintOpaqueRects: dstColorFormat === 'rgb10a2unorm',
    });

    const dst = t.device.createTexture({
      size: {
        width,
        height,
        depthOrArrayLayers: 1,
      },

      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // Construct expected value for different dst color format
    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;
    const format = kTextureFormatInfo[dstColorFormat].baseFormat ?? dstColorFormat;
    const sourcePixels = t.calculateSourceContentOnCPU(
      width,
      height,
      srcPremultiplied,
      dstColorFormat === 'rgb10a2unorm'
    );

    const expectedPixels = t.getExpectedPixels(
      sourcePixels,
      width,
      height,
      format,
      srcPremultiplied,
      dstPremultiplied,
      srcDoFlipYDuringCopy
    );

    t.doTestAndCheckResult(
      { source: canvas, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: 'srgb',
        premultipliedAlpha: dstPremultiplied,
      },

      { width: canvas.width, height: canvas.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels,
      dstColorFormat
    );
  });

g.test('color_space_conversion')
  .desc(
    `
    Test HTMLCanvasElement with 2d context can created with 'colorSpace' attribute.
    Using CopyExternalImageToTexture to copy from such type of canvas needs
    to do color space converting correctly.
  
    It creates HTMLCanvasElement/OffscreenCanvas with '2d' and 'colorSpace' attributes.
    Use fillRect(2d context) to render red rect for top-left,
    green rect for top-right, blue rect for bottom-left and white for bottom-right.
  
    Then call copyExternalImageToTexture() to do a full copy to the 0 mipLevel
    of dst texture, and read the contents out to compare with the canvas contents.
  
    Provide premultiplied input if 'premultipliedAlpha' in 'GPUImageCopyTextureTagged'
    is set to 'true' and unpremultiplied input if it is set to 'false'.
  
    If 'flipY' in 'GPUImageCopyExternalImage' is set to 'true', copy will ensure the result
    is flipped.

    If color space from source input and user defined dstTexture color space are different, the
    result must convert the content to user defined color space
  
    The tests covers:
    - Valid dstColorFormat of copyExternalImageToTexture()
    - Valid dest alphaMode
    - Valid 'flipY' config in 'GPUImageCopyExternalImage' (named 'srcDoFlipYDuringCopy' in cases)
    - Valid 'colorSpace' config in 'dstColorSpace'
    - TODO: Add error tolerance for rgb10a2unorm dst texture format
  
    And the expected results are all passed.
  `
  )
  .params(u =>
    u
      .combine('srcColorSpace', ['srgb', 'display-p3'])
      .combine('dstColorSpace', ['srgb'])
      .combine('dstColorFormat', kValidTextureFormatsForCopyE2T)
      .combine('dstPremultiplied', [true, false])
      .combine('srcDoFlipYDuringCopy', [true, false])
      .beginSubcases()
      .combine('width', [1, 2, 4, 15, 255, 256])
      .combine('height', [1, 2, 4, 15, 255, 256])
  )
  .fn(async t => {
    const {
      width,
      height,
      srcColorSpace,
      dstColorSpace,
      dstColorFormat,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
    } = t.params;
    const { canvas, canvasContext } = t.init2DCanvasContentWithColorSpace({
      width,
      height,
      paintOpaqueRects: true,
      colorSpace: srcColorSpace,
    });

    const dst = t.device.createTexture({
      size: {
        width,
        height,
        depthOrArrayLayers: 1,
      },

      format: dstColorFormat,
      usage:
        GPUTextureUsage.COPY_DST | GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const sourcePixels = t.getSourceCanvas2DContent(canvasContext, width, height);

    const dstBytesPerPixel = kTextureFormatInfo[dstColorFormat].bytesPerBlock;

    const expectedPixels = t.getExpectedPixels(
      sourcePixels,
      width,
      height,
      dstColorFormat,
      false,
      dstPremultiplied,
      srcDoFlipYDuringCopy,
      srcColorSpace,
      dstColorSpace
    );

    t.doTestAndCheckResult(
      { source: canvas, origin: { x: 0, y: 0 }, flipY: srcDoFlipYDuringCopy },
      {
        texture: dst,
        origin: { x: 0, y: 0 },
        colorSpace: dstColorSpace,
        premultipliedAlpha: dstPremultiplied,
      },

      { width: canvas.width, height: canvas.height, depthOrArrayLayers: 1 },
      dstBytesPerPixel,
      expectedPixels,
      dstColorFormat
    );
  });

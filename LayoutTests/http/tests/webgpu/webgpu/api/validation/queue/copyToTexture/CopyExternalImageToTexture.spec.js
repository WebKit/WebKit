/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
copyExternalImageToTexture Validation Tests in Queue.
`;
import { getResourcePath } from '../../../../../common/framework/resources.js';
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { raceWithRejectOnTimeout, unreachable, assert } from '../../../../../common/util/util.js';
import {
  kTextureFormatInfo,
  kTextureFormats,
  kTextureUsages,
  kValidTextureFormatsForCopyE2T,
} from '../../../../capability_info.js';
import { kResourceStates } from '../../../../gpu_test.js';
import {
  createCanvas,
  createOnscreenCanvas,
  createOffscreenCanvas,
} from '../../../../util/create_elements.js';
import { ValidationTest } from '../../validation_test.js';

const kDefaultBytesPerPixel = 4; // using 'bgra8unorm' or 'rgba8unorm'
const kDefaultWidth = 32;
const kDefaultHeight = 32;
const kDefaultDepth = 1;
const kDefaultMipLevelCount = 6;

/** Valid contextId for HTMLCanvasElement/OffscreenCanvas,
 *  spec: https://html.spec.whatwg.org/multipage/canvas.html#dom-canvas-getcontext
 */
export const kValidContextId = ['2d', 'bitmaprenderer', 'webgl', 'webgl2', 'webgpu'];

function computeMipMapSize(width, height, mipLevel) {
  return {
    mipWidth: Math.max(width >> mipLevel, 1),
    mipHeight: Math.max(height >> mipLevel, 1),
  };
}

// Helper function to generate copySize for src OOB test
function generateCopySizeForSrcOOB({ srcOrigin }) {
  // OOB origin fails even with no-op copy.
  if (srcOrigin.x > kDefaultWidth || srcOrigin.y > kDefaultHeight) {
    return [{ width: 0, height: 0, depthOrArrayLayers: 0 }];
  }

  const justFitCopySize = {
    width: kDefaultWidth - srcOrigin.x,
    height: kDefaultHeight - srcOrigin.y,
    depthOrArrayLayers: 1,
  };

  return [
    justFitCopySize, // correct size, maybe no-op copy.
    { width: justFitCopySize.width + 1, height: justFitCopySize.height, depthOrArrayLayers: 1 }, // OOB in width
    { width: justFitCopySize.width, height: justFitCopySize.height + 1, depthOrArrayLayers: 1 }, // OOB in height
    { width: justFitCopySize.width, height: justFitCopySize.height, depthOrArrayLayers: 2 }, // OOB in depthOrArrayLayers
  ];
}

// Helper function to generate dst origin value based on mipLevel.
function generateDstOriginValue({ mipLevel }) {
  const origin = computeMipMapSize(kDefaultWidth, kDefaultHeight, mipLevel);

  return [
    { x: 0, y: 0, z: 0 },
    { x: origin.mipWidth - 1, y: 0, z: 0 },
    { x: 0, y: origin.mipHeight - 1, z: 0 },
    { x: origin.mipWidth, y: 0, z: 0 },
    { x: 0, y: origin.mipHeight, z: 0 },
    { x: 0, y: 0, z: kDefaultDepth },
    { x: origin.mipWidth + 1, y: 0, z: 0 },
    { x: 0, y: origin.mipHeight + 1, z: 0 },
    { x: 0, y: 0, z: kDefaultDepth + 1 },
  ];
}

// Helper function to generate copySize for dst OOB test
function generateCopySizeForDstOOB({ mipLevel, dstOrigin }) {
  const dstMipMapSize = computeMipMapSize(kDefaultWidth, kDefaultHeight, mipLevel);

  // OOB origin fails even with no-op copy.
  if (
    dstOrigin.x > dstMipMapSize.mipWidth ||
    dstOrigin.y > dstMipMapSize.mipHeight ||
    dstOrigin.z > kDefaultDepth
  ) {
    return [{ width: 0, height: 0, depthOrArrayLayers: 0 }];
  }

  const justFitCopySize = {
    width: dstMipMapSize.mipWidth - dstOrigin.x,
    height: dstMipMapSize.mipHeight - dstOrigin.y,
    depthOrArrayLayers: kDefaultDepth - dstOrigin.z,
  };

  return [
    justFitCopySize,
    {
      width: justFitCopySize.width + 1,
      height: justFitCopySize.height,
      depthOrArrayLayers: justFitCopySize.depthOrArrayLayers,
    },
    // OOB in width
    {
      width: justFitCopySize.width,
      height: justFitCopySize.height + 1,
      depthOrArrayLayers: justFitCopySize.depthOrArrayLayers,
    },
    // OOB in height
    {
      width: justFitCopySize.width,
      height: justFitCopySize.height,
      depthOrArrayLayers: justFitCopySize.depthOrArrayLayers + 1,
    },
    // OOB in depthOrArrayLayers
  ];
}

function canCopyFromContextType(contextName) {
  switch (contextName) {
    case '2d':
    case 'webgl':
    case 'webgl2':
    case 'webgpu':
      return true;
    default:
      return false;
  }
}

class CopyExternalImageToTextureTest extends ValidationTest {
  getImageData(width, height) {
    const pixelSize = kDefaultBytesPerPixel * width * height;
    const imagePixels = new Uint8ClampedArray(pixelSize);
    return new ImageData(imagePixels, width, height);
  }

  getCanvasWithContent(canvasType, width, height, content) {
    const canvas = createCanvas(this, canvasType, 1, 1);
    const ctx = canvas.getContext('2d');
    assert(ctx !== null);
    ctx.drawImage(content, 0, 0);

    return canvas;
  }

  runTest(imageBitmapCopyView, textureCopyView, copySize, validationScopeSuccess, exceptionName) {
    // copyExternalImageToTexture will generate two types of errors. One is synchronous exceptions;
    // the other is asynchronous validation error scope errors.
    if (exceptionName) {
      this.shouldThrow(exceptionName, () => {
        this.device.queue.copyExternalImageToTexture(
          imageBitmapCopyView,
          textureCopyView,
          copySize
        );
      });
    } else {
      this.expectValidationError(() => {
        this.device.queue.copyExternalImageToTexture(
          imageBitmapCopyView,
          textureCopyView,
          copySize
        );
      }, !validationScopeSuccess);
    }
  }
}

export const g = makeTestGroup(CopyExternalImageToTextureTest);

g.test('source_canvas,contexts')
  .desc(
    `
  Test HTMLCanvasElement as source image with different contexts.

  Call HTMLCanvasElement.getContext() with different context type.
  Only '2d', 'experimental-webgl', 'webgl', 'webgl2' is valid context
  type.

  Check whether 'OperationError' is generated when context type is invalid.
  `
  )
  .params(u =>
    u //
      .combine('contextType', kValidContextId)
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { contextType, copySize } = t.params;
    const canvas = createOnscreenCanvas(t, 1, 1);
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    const ctx = canvas.getContext(contextType);
    if (ctx === null) {
      t.skip('Failed to get context for canvas element');
      return;
    }
    t.tryTrackForCleanup(ctx);

    t.runTest(
      { source: canvas },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      canCopyFromContextType(contextType) ? '' : 'OperationError'
    );
  });

g.test('source_offscreenCanvas,contexts')
  .desc(
    `
  Test OffscreenCanvas as source image with different contexts.

  Call OffscreenCanvas.getContext() with different context type.
  Only '2d', 'webgl', 'webgl2', 'webgpu' is valid context type.

  Check whether 'OperationError' is generated when context type is invalid.
  `
  )
  .params(u =>
    u //
      .combine('contextType', kValidContextId)
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { contextType, copySize } = t.params;
    const canvas = createOffscreenCanvas(t, 1, 1);
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    // MAINTENANCE_TODO: Workaround for @types/offscreencanvas missing an overload of
    // `OffscreenCanvas.getContext` that takes `string` or a union of context types.
    const ctx = canvas.getContext(contextType);

    if (ctx === null) {
      t.skip('Failed to get context for canvas element');
      return;
    }
    t.tryTrackForCleanup(ctx);

    t.runTest(
      { source: canvas },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      canCopyFromContextType(contextType) ? '' : 'OperationError'
    );
  });

g.test('source_image,crossOrigin')
  .desc(
    `
  Test contents of source image is [clean, cross-origin].

  Load crossOrigin image or same origin image and init the source
  images.

  Check whether 'SecurityError' is generated when source image is not origin clean.

  TODO: make this test case work offline, ref link to achieve this :
  https://web-platform-tests.org/writing-tests/server-features.html#tests-involving-multiple-origins
  `
  )
  .params(u =>
    u //
      .combine('sourceImage', ['canvas', 'offscreenCanvas', 'imageBitmap'])
      .combine('isOriginClean', [true, false])
      .beginSubcases()
      .combine('contentFrom', ['image', 'imageBitmap', 'canvas', 'offscreenCanvas'])
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { sourceImage, isOriginClean, contentFrom, copySize } = t.params;

    const crossOriginUrl = 'https://get.webgl.org/conformance-resources/opengl_logo.jpg';
    const originCleanUrl = getResourcePath('Di-3d.png');

    const img = document.createElement('img');
    img.src = isOriginClean ? originCleanUrl : crossOriginUrl;

    // Load image
    const timeout_ms = 5000;
    try {
      await raceWithRejectOnTimeout(img.decode(), timeout_ms, 'load image timeout');
    } catch (e) {
      if (isOriginClean) {
        throw e;
      } else {
        t.warn('Something wrong happens in get.webgl.org');
        t.skip('Cannot load image in time');
        return;
      }
    }

    // The externalImage contents can be updated by:
    // - decoded image element
    // - canvas/offscreenCanvas with image draw on it.
    // - imageBitmap created with the image.
    // Test covers all of these cases to ensure origin clean checks works.
    let source;
    switch (contentFrom) {
      case 'image': {
        source = img;
        break;
      }
      case 'imageBitmap': {
        source = await createImageBitmap(img);
        break;
      }
      case 'canvas':
      case 'offscreenCanvas': {
        const canvasType = contentFrom === 'offscreenCanvas' ? 'offscreen' : 'onscreen';
        source = t.getCanvasWithContent(canvasType, 1, 1, img);
        break;
      }
      default:
        unreachable();
    }

    // Update the externalImage content with source.
    let externalImage;
    switch (sourceImage) {
      case 'imageBitmap': {
        externalImage = await createImageBitmap(source);
        break;
      }
      case 'canvas':
      case 'offscreenCanvas': {
        const canvasType = contentFrom === 'offscreenCanvas' ? 'offscreen' : 'onscreen';
        externalImage = t.getCanvasWithContent(canvasType, 1, 1, source);
        break;
      }
      default:
        unreachable();
    }

    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    t.runTest(
      { source: externalImage },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      isOriginClean ? '' : 'SecurityError'
    );
  });

g.test('source_imageBitmap,state')
  .desc(
    `
  Test ImageBitmap as source image in state [valid, closed].

  Call imageBitmap.close() to transfer the imageBitmap into
  'closed' state.

  Check whether 'InvalidStateError' is generated when ImageBitmap is
  closed.
  `
  )
  .params(u =>
    u //
      .combine('closed', [false, true])
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { closed, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    if (closed) imageBitmap.close();

    t.runTest(
      { source: imageBitmap },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      closed ? 'InvalidStateError' : ''
    );
  });

g.test('source_canvas,state')
  .desc(
    `
  Test HTMLCanvasElement as source image in state
  [nocontext, 'placeholder-nocontext', 'placeholder-hascontext', valid].

  Nocontext means using a canvas without any context as copy param.

  Call 'transferControlToOffscreen' on HTMLCanvasElement will cause the
  canvas control right transfer. And this canvas is in state 'placeholder'
  Whether getContext in new generated offscreenCanvas won't affect the origin
  canvas state.


  Check whether 'OperationError' is generated when HTMLCanvasElement has no
  context.

  Check whether 'InvalidStateError' is generated when HTMLCanvasElement is
  in 'placeholder' state.
    `
  )
  .params(u =>
    u //
      .combine('state', ['nocontext', 'placeholder-nocontext', 'placeholder-hascontext', 'valid'])
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { state, copySize } = t.params;
    const canvas = createOnscreenCanvas(t, 1, 1);
    if (typeof canvas.transferControlToOffscreen === 'undefined') {
      t.skip("Browser doesn't support HTMLCanvasElement transfer control right");
      return;
    }

    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    let exceptionName = '';

    switch (state) {
      case 'nocontext': {
        exceptionName = 'OperationError';
        break;
      }
      case 'placeholder-nocontext': {
        canvas.transferControlToOffscreen();
        exceptionName = 'InvalidStateError';
        break;
      }
      case 'placeholder-hascontext': {
        const offscreenCanvas = canvas.transferControlToOffscreen();
        t.tryTrackForCleanup(offscreenCanvas.getContext('webgl'));
        exceptionName = 'InvalidStateError';
        break;
      }
      case 'valid': {
        assert(canvas.getContext('2d') !== null);
        break;
      }
      default:
        unreachable();
    }

    t.runTest(
      { source: canvas },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      exceptionName
    );
  });

g.test('source_offscreenCanvas,state')
  .desc(
    `
  Test OffscreenCanvas as source image in state [valid, detached].

  Nocontext means using a canvas without any context as copy param.

  Transfer OffscreenCanvas with MessageChannel will detach the OffscreenCanvas.

  Check whether 'OperationError' is generated when HTMLCanvasElement has no
  context.

  Check whether 'InvalidStateError' is generated when OffscreenCanvas is
  detached.
  `
  )
  .params(u =>
    u //
      .combine('state', ['nocontext', 'detached-nocontext', 'detached-hascontext', 'valid'])
      .beginSubcases()
      .combine('getContextInOffscreenCanvas', [false, true])
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { state, copySize } = t.params;
    const offscreenCanvas = createOffscreenCanvas(t, 1, 1);
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    let exceptionName = '';
    switch (state) {
      case 'nocontext': {
        exceptionName = 'OperationError';
        break;
      }
      case 'detached-nocontext': {
        const messageChannel = new MessageChannel();
        messageChannel.port1.postMessage(offscreenCanvas, [offscreenCanvas]);

        exceptionName = 'InvalidStateError';
        break;
      }
      case 'detached-hascontext': {
        const messageChannel = new MessageChannel();
        const port2FirstMessage = new Promise(resolve => {
          messageChannel.port2.onmessage = m => resolve(m);
        });

        messageChannel.port1.postMessage(offscreenCanvas, [offscreenCanvas]);

        const receivedOffscreenCanvas = await port2FirstMessage;
        t.tryTrackForCleanup(receivedOffscreenCanvas.data.getContext('webgl'));

        exceptionName = 'InvalidStateError';
        break;
      }
      case 'valid': {
        offscreenCanvas.getContext('webgl');
        break;
      }
      default:
        unreachable();
    }

    t.runTest(
      { source: offscreenCanvas },
      { texture: dstTexture },
      copySize,
      true, // No validation errors.
      exceptionName
    );
  });

g.test('destination_texture,state')
  .desc(
    `
  Test dst texture is [valid, invalid, destroyed].

  Check that an error is generated when texture is an error texture.
  Check that an error is generated when texture is in destroyed state.
  `
  )
  .params(u =>
    u //
      .combine('state', kResourceStates)
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { state, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.createTextureWithState(state);

    t.runTest({ source: imageBitmap }, { texture: dstTexture }, copySize, state === 'valid');
  });

g.test('destination_texture,device_mismatch')
  .desc(
    'Tests copyExternalImageToTexture cannot be called with a destination texture created from another device'
  )
  .paramsSubcasesOnly(u => u.combine('mismatched', [true, false]))
  .unimplemented();

g.test('destination_texture,dimension')
  .desc(
    `
  Test dst texture dimension is [1d, 2d, 3d].

  Check that an error is generated when texture is not '2d' dimension.
  `
  )
  .params(u =>
    u //
      .combine('dimension', ['1d', '2d', '3d'])
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { dimension, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
      dimension,
    });

    t.runTest({ source: imageBitmap }, { texture: dstTexture }, copySize, dimension === '2d');
  });

g.test('destination_texture,usage')
  .desc(
    `
  Test dst texture usages

  Check that an error is generated when texture is created without usage COPY_DST | RENDER_ATTACHMENT.
  `
  )
  .params(u =>
    u //
      .combine('usage', kTextureUsages)
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { usage, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage,
    });

    t.runTest(
      { source: imageBitmap },
      { texture: dstTexture },
      copySize,
      !!(usage & GPUTextureUsage.COPY_DST && usage & GPUTextureUsage.RENDER_ATTACHMENT)
    );
  });

g.test('destination_texture,sample_count')
  .desc(
    `
  Test dst texture sample count.

  Check that an error is generated when sample count it not 1.
  `
  )
  .params(u =>
    u //
      .combine('sampleCount', [1, 4])
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { sampleCount, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      sampleCount,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    t.runTest({ source: imageBitmap }, { texture: dstTexture }, copySize, sampleCount === 1);
  });

g.test('destination_texture,mipLevel')
  .desc(
    `
  Test dst mipLevel.

  Check that an error is generated when mipLevel is too large.
  `
  )
  .params(u =>
    u //
      .combine('mipLevel', [0, kDefaultMipLevelCount - 1, kDefaultMipLevelCount])
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { mipLevel, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));
    const dstTexture = t.device.createTexture({
      size: { width: kDefaultWidth, height: kDefaultHeight, depthOrArrayLayers: kDefaultDepth },
      mipLevelCount: kDefaultMipLevelCount,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    t.runTest(
      { source: imageBitmap },
      { texture: dstTexture, mipLevel },
      copySize,
      mipLevel < kDefaultMipLevelCount
    );
  });

g.test('destination_texture,format')
  .desc(
    `
  Test dst texture format.

  Check that an error is generated when texture format is not valid.
  `
  )
  .params(u =>
    u
      .combine('format', kTextureFormats)
      .beginSubcases()
      .combine('copySize', [
        { width: 0, height: 0, depthOrArrayLayers: 0 },
        { width: 1, height: 1, depthOrArrayLayers: 1 },
      ])
  )
  .fn(async t => {
    const { format, copySize } = t.params;
    await t.selectDeviceOrSkipTestCase(kTextureFormatInfo[format].feature);

    const imageBitmap = await createImageBitmap(t.getImageData(1, 1));

    // createTexture with all possible texture format may have validation error when using
    // compressed texture format.
    t.device.pushErrorScope('validation');
    const dstTexture = t.device.createTexture({
      size: { width: 1, height: 1, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    t.device.popErrorScope();

    const success = kValidTextureFormatsForCopyE2T.includes(format);

    t.runTest({ source: imageBitmap }, { texture: dstTexture }, copySize, success);
  });

g.test('OOB,source')
  .desc(
    `
  Test source image origin and copy size

  Check that an error is generated when source.externalImage.origin + copySize is too large.
  `
  )
  .paramsSubcasesOnly(u =>
    u
      .combine('srcOrigin', [
        { x: 0, y: 0 }, // origin is on top-left
        { x: kDefaultWidth - 1, y: 0 }, // x near the border
        { x: 0, y: kDefaultHeight - 1 }, // y is near the border
        { x: kDefaultWidth, y: kDefaultHeight }, // origin is on bottom-right
        { x: kDefaultWidth + 1, y: 0 }, // x is too large
        { x: 0, y: kDefaultHeight + 1 }, // y is too large
      ])
      .expand('copySize', generateCopySizeForSrcOOB)
  )
  .fn(async t => {
    const { srcOrigin, copySize } = t.params;
    const imageBitmap = await createImageBitmap(t.getImageData(kDefaultWidth, kDefaultHeight));
    const dstTexture = t.device.createTexture({
      size: {
        width: kDefaultWidth + 1,
        height: kDefaultHeight + 1,
        depthOrArrayLayers: kDefaultDepth,
      },

      mipLevelCount: kDefaultMipLevelCount,
      format: 'bgra8unorm',
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    let success = true;

    if (
      srcOrigin.x + copySize.width > kDefaultWidth ||
      srcOrigin.y + copySize.height > kDefaultHeight ||
      copySize.depthOrArrayLayers > 1
    ) {
      success = false;
    }

    t.runTest(
      { source: imageBitmap, origin: srcOrigin },
      { texture: dstTexture },
      copySize,
      true,
      success ? '' : 'OperationError'
    );
  });

g.test('OOB,destination')
  .desc(
    `
  Test dst texture copy origin and copy size

  Check that an error is generated when destination.texture.origin + copySize is too large.
  Check that 'OperationError' is generated when copySize.depth is larger than 1.
  `
  )
  .paramsSubcasesOnly(u =>
    u
      .combine('mipLevel', [0, 1, kDefaultMipLevelCount - 2])
      .expand('dstOrigin', generateDstOriginValue)
      .expand('copySize', generateCopySizeForDstOOB)
  )
  .fn(async t => {
    const { mipLevel, dstOrigin, copySize } = t.params;

    const imageBitmap = await createImageBitmap(
      t.getImageData(kDefaultWidth + 1, kDefaultHeight + 1)
    );

    const dstTexture = t.device.createTexture({
      size: {
        width: kDefaultWidth,
        height: kDefaultHeight,
        depthOrArrayLayers: kDefaultDepth,
      },

      format: 'bgra8unorm',
      mipLevelCount: kDefaultMipLevelCount,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT,
    });

    let success = true;
    let hasOperationError = false;
    const dstMipMapSize = computeMipMapSize(kDefaultWidth, kDefaultHeight, mipLevel);

    if (
      copySize.depthOrArrayLayers > 1 ||
      dstOrigin.x + copySize.width > dstMipMapSize.mipWidth ||
      dstOrigin.y + copySize.height > dstMipMapSize.mipHeight ||
      dstOrigin.z + copySize.depthOrArrayLayers > kDefaultDepth
    ) {
      success = false;
    }
    if (copySize.depthOrArrayLayers > 1) {
      hasOperationError = true;
    }

    t.runTest(
      { source: imageBitmap },
      {
        texture: dstTexture,
        mipLevel,
        origin: dstOrigin,
      },

      copySize,
      success,
      hasOperationError ? 'OperationError' : ''
    );
  });

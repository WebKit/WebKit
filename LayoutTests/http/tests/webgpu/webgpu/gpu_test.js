/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { Fixture } from '../common/framework/fixture.js';
import { attemptGarbageCollection } from '../common/util/collect_garbage.js';
import { assert, range, unreachable } from '../common/util/util.js';

import { kTextureFormatInfo, kQueryTypeInfo, resolvePerAspectFormat } from './capability_info.js';
import { makeBufferWithContents } from './util/buffer.js';
import {
  checkElementsEqual,
  checkElementsBetween,
  checkElementsFloat16Between,
} from './util/check_contents.js';
import { CommandBufferMaker } from './util/command_buffer_maker.js';
import { DevicePool, TestOOMedShouldAttemptGC } from './util/device_pool.js';
import { align, roundDown } from './util/math.js';
import { makeTextureWithContents } from './util/texture.js';
import { getTextureCopyLayout, getTextureSubCopyLayout } from './util/texture/layout.js';
import { kTexelRepresentationInfo } from './util/texture/texel_data.js';

const devicePool = new DevicePool();

// MAINTENANCE_TODO: When DevicePool becomes able to provide multiple devices at once, use the
// usual one instead of a new one.
const mismatchedDevicePool = new DevicePool();

const kResourceStateValues = ['valid', 'invalid', 'destroyed'];

export const kResourceStates = kResourceStateValues;

export function initUncanonicalizedDeviceDescriptor(descriptor) {
  if (typeof descriptor === 'string') {
    return { requiredFeatures: [descriptor] };
  } else if (descriptor instanceof Array) {
    return {
      requiredFeatures: descriptor.filter(f => f !== undefined),
    };
  } else {
    return descriptor;
  }
}

/**
 * Base fixture for WebGPU tests.
 */
export class GPUTest extends Fixture {
  /** Must not be replaced once acquired. */

  // Some tests(e.g. Device mismatched validation) require another GPUDevice
  // different from the default GPUDevice of GPUTest. It is only used to
  //create device mismatched objects.

  /** GPUDevice for the test to use. */
  get device() {
    assert(
      this.provider !== undefined,
      'No provider available right now; did you "await" selectDeviceOrSkipTestCase?'
    );

    if (!this.acquiredDevice) {
      this.acquiredDevice = this.provider.acquire();
    }
    return this.acquiredDevice;
  }

  /** GPUDevice for tests requires another device from default one.
   *  e.g. creating objects required creating mismatched objects required
   * by device mismatched validation tests.
   */
  get mismatchedDevice() {
    assert(
      this.mismatchedProvider !== undefined,
      'No provider available right now; did you "await" selectMismatchedDeviceOrSkipTestCase?'
    );

    if (!this.mismatchedAcquiredDevice) {
      this.mismatchedAcquiredDevice = this.mismatchedProvider.acquire();
    }
    return this.mismatchedAcquiredDevice;
  }

  /**
   * Create other device different with current test device, which could be got by `.mismatchedDevice`.
   * A `descriptor` may be undefined, which returns a `default` mismatched device.
   * If the request descriptor or feature name can't be supported, throws an exception to skip the entire test case.
   */
  async selectMismatchedDeviceOrSkipTestCase(descriptor) {
    assert(
      this.mismatchedProvider === undefined,
      "Can't selectMismatchedDeviceOrSkipTestCase() multiple times"
    );

    this.mismatchedProvider =
      descriptor === undefined
        ? await mismatchedDevicePool.reserve()
        : await mismatchedDevicePool.reserve(initUncanonicalizedDeviceDescriptor(descriptor));

    this.mismatchedAcquiredDevice = this.mismatchedProvider.acquire();
  }

  /** GPUQueue for the test to use. (Same as `t.device.queue`.) */
  get queue() {
    return this.device.queue;
  }

  async init() {
    await super.init();

    this.provider = await devicePool.reserve();
  }

  async finalize() {
    await super.finalize();

    if (this.provider) {
      let threw = false;
      let thrownValue;
      {
        const provider = this.provider;
        this.provider = undefined;
        try {
          await devicePool.release(provider);
        } catch (ex) {
          threw = true;
          thrownValue = ex;
        }
      }
      // The GPUDevice and GPUQueue should now have no outstanding references.

      if (threw) {
        if (thrownValue instanceof TestOOMedShouldAttemptGC) {
          // Try to clean up, in case there are stray GPU resources in need of collection.
          await attemptGarbageCollection();
        }
        throw thrownValue;
      }
    }

    if (this.mismatchedProvider) {
      // MAINTENANCE_TODO(kainino0x): Deduplicate this with code in GPUTest.finalize
      let threw = false;
      let thrownValue;
      {
        const provider = this.mismatchedProvider;
        this.mismatchedProvider = undefined;
        try {
          await mismatchedDevicePool.release(provider);
        } catch (ex) {
          threw = true;
          thrownValue = ex;
        }
      }

      if (threw) {
        if (thrownValue instanceof TestOOMedShouldAttemptGC) {
          // Try to clean up, in case there are stray GPU resources in need of collection.
          await attemptGarbageCollection();
        }
        throw thrownValue;
      }
    }
  }

  /**
   * When a GPUTest test accesses `.device` for the first time, a "default" GPUDevice
   * (descriptor = `undefined`) is provided by default.
   * However, some tests or cases need particular nonGuaranteedFeatures to be enabled.
   * Call this function with a descriptor or feature name (or `undefined`) to select a
   * GPUDevice with matching capabilities.
   *
   * If the request descriptor can't be supported, throws an exception to skip the entire test case.
   */
  async selectDeviceOrSkipTestCase(descriptor) {
    if (descriptor === undefined) return;

    assert(this.provider !== undefined);
    // Make sure the device isn't replaced after it's been retrieved once.
    assert(
      !this.acquiredDevice,
      "Can't selectDeviceOrSkipTestCase() after the device has been used"
    );

    const oldProvider = this.provider;
    this.provider = undefined;
    await devicePool.release(oldProvider);

    this.provider = await devicePool.reserve(initUncanonicalizedDeviceDescriptor(descriptor));
    this.acquiredDevice = this.provider.acquire();
  }

  /**
   * Create device with texture format(s) required feature(s).
   * If the device creation fails, then skip the test for that format(s).
   */
  async selectDeviceForTextureFormatOrSkipTestCase(formats) {
    if (!Array.isArray(formats)) {
      formats = [formats];
    }
    const features = new Set();
    for (const format of formats) {
      if (format !== undefined) {
        features.add(kTextureFormatInfo[format].feature);
      }
    }

    await this.selectDeviceOrSkipTestCase(Array.from(features));
  }

  /**
   * Create device with query type(s) required feature(s).
   * If the device creation fails, then skip the test for that type(s).
   */
  async selectDeviceForQueryTypeOrSkipTestCase(types) {
    if (!Array.isArray(types)) {
      types = [types];
    }
    const features = types.map(t => kQueryTypeInfo[t].feature);
    await this.selectDeviceOrSkipTestCase(features);
  }

  /** Snapshot a GPUBuffer's contents, returning a new GPUBuffer with the `MAP_READ` usage. */
  createCopyForMapRead(src, srcOffset, size) {
    assert(srcOffset % 4 === 0);
    assert(size % 4 === 0);

    const dst = this.device.createBuffer({
      size,
      usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
    });

    this.trackForCleanup(dst);

    const c = this.device.createCommandEncoder();
    c.copyBufferToBuffer(src, srcOffset, dst, 0, size);
    this.queue.submit([c.finish()]);

    return dst;
  }

  /**
   * Offset and size passed to createCopyForMapRead must be divisible by 4. For that
   * we might need to copy more bytes from the buffer than we want to map.
   * begin and end values represent the part of the copied buffer that stores the contents
   * we initially wanted to map.
   * The copy will not cause an OOB error because the buffer size must be 4-aligned.
   */
  createAlignedCopyForMapRead(src, size, offset) {
    const alignedOffset = roundDown(offset, 4);
    const subarrayByteStart = offset - alignedOffset;
    const alignedSize = align(size + subarrayByteStart, 4);
    const mappable = this.createCopyForMapRead(src, alignedOffset, alignedSize);
    return { mappable, subarrayByteStart };
  }

  /**
   * Snapshot the current contents of a range of a GPUBuffer, and return them as a TypedArray.
   * Also provides a cleanup() function to unmap and destroy the staging buffer.
   */
  async readGPUBufferRangeTyped(src, { srcByteOffset = 0, method = 'copy', type, typedLength }) {
    assert(
      srcByteOffset % type.BYTES_PER_ELEMENT === 0,
      'srcByteOffset must be a multiple of BYTES_PER_ELEMENT'
    );

    const byteLength = typedLength * type.BYTES_PER_ELEMENT;
    let mappable;
    let mapOffset, mapSize, subarrayByteStart;
    if (method === 'copy') {
      ({ mappable, subarrayByteStart } = this.createAlignedCopyForMapRead(
        src,
        byteLength,
        srcByteOffset
      ));
    } else if (method === 'map') {
      mappable = src;
      mapOffset = roundDown(srcByteOffset, 8);
      mapSize = align(byteLength, 4);
      subarrayByteStart = srcByteOffset - mapOffset;
    } else {
      unreachable();
    }

    assert(subarrayByteStart % type.BYTES_PER_ELEMENT === 0);
    const subarrayStart = subarrayByteStart / type.BYTES_PER_ELEMENT;

    // 2. Map the staging buffer, and create the TypedArray from it.
    await mappable.mapAsync(GPUMapMode.READ, mapOffset, mapSize);
    const mapped = new type(mappable.getMappedRange(mapOffset, mapSize));
    const data = mapped.subarray(subarrayStart, typedLength);

    return {
      data,
      cleanup() {
        mappable.unmap();
        mappable.destroy();
      },
    };
  }

  /**
   * Expect a GPUBuffer's contents to pass the provided check.
   *
   * A library of checks can be found in {@link webgpu/util/check_contents}.
   */
  expectGPUBufferValuesPassCheck(
    src,
    check,
    { srcByteOffset = 0, type, typedLength, method = 'copy', mode = 'fail' }
  ) {
    const readbackPromise = this.readGPUBufferRangeTyped(src, {
      srcByteOffset,
      type,
      typedLength,
      method,
    });

    this.eventualAsyncExpectation(async niceStack => {
      const readback = await readbackPromise;
      this.expectOK(check(readback.data), { mode, niceStack });
      readback.cleanup();
    });
  }

  /**
   * Expect a GPUBuffer's contents to equal the values in the provided TypedArray.
   */
  expectGPUBufferValuesEqual(
    src,
    expected,
    srcByteOffset = 0,
    { method = 'copy', mode = 'fail' } = {}
  ) {
    this.expectGPUBufferValuesPassCheck(src, a => checkElementsEqual(a, expected), {
      srcByteOffset,
      type: expected.constructor,
      typedLength: expected.length,
      method,
      mode,
    });
  }

  /**
   * Expect a buffer to consist exclusively of rows of some repeated expected value. The size of
   * `expectedValue` must be 1, 2, or any multiple of 4 bytes. Rows in the buffer are expected to be
   * zero-padded out to `bytesPerRow`. `minBytesPerRow` is the number of bytes per row that contain
   * actual (non-padding) data and must be an exact multiple of the byte-length of `expectedValue`.
   */
  expectGPUBufferRepeatsSingleValue(
    buffer,
    { expectedValue, numRows, minBytesPerRow, bytesPerRow }
  ) {
    const valueSize = expectedValue.byteLength;
    assert(valueSize === 1 || valueSize === 2 || valueSize % 4 === 0);
    assert(minBytesPerRow % valueSize === 0);
    assert(bytesPerRow % 4 === 0);

    // If the buffer is small enough, just generate the full expected buffer contents and check
    // against them on the CPU.
    const kMaxBufferSizeToCheckOnCpu = 256 * 1024;
    const bufferSize = bytesPerRow * (numRows - 1) + minBytesPerRow;
    if (bufferSize <= kMaxBufferSizeToCheckOnCpu) {
      const valueBytes = Array.from(new Uint8Array(expectedValue));
      const rowValues = new Array(minBytesPerRow / valueSize).fill(valueBytes);
      const rowBytes = new Uint8Array([].concat(...rowValues));
      const expectedContents = new Uint8Array(bufferSize);
      range(numRows, row => expectedContents.set(rowBytes, row * bytesPerRow));
      this.expectGPUBufferValuesEqual(buffer, expectedContents);
      return;
    }

    // Copy into a buffer suitable for STORAGE usage.
    const storageBuffer = this.device.createBuffer({
      size: bufferSize,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
    });

    this.trackForCleanup(storageBuffer);

    // This buffer conveys the data we expect to see for a single value read. Since we read 32 bits at
    // a time, for values smaller than 32 bits we pad this expectation with repeated value data, or
    // with zeroes if the width of a row in the buffer is less than 4 bytes. For value sizes larger
    // than 32 bits, we assume they're a multiple of 32 bits and expect to read exact matches of
    // `expectedValue` as-is.
    const expectedDataSize = Math.max(4, valueSize);
    const expectedDataBuffer = this.device.createBuffer({
      size: expectedDataSize,
      usage: GPUBufferUsage.STORAGE,
      mappedAtCreation: true,
    });

    this.trackForCleanup(expectedDataBuffer);
    const expectedData = new Uint32Array(expectedDataBuffer.getMappedRange());
    if (valueSize === 1) {
      const value = new Uint8Array(expectedValue)[0];
      const values = new Array(Math.min(4, minBytesPerRow)).fill(value);
      const padding = new Array(Math.max(0, 4 - values.length)).fill(0);
      const expectedBytes = new Uint8Array(expectedData.buffer);
      expectedBytes.set([...values, ...padding]);
    } else if (valueSize === 2) {
      const value = new Uint16Array(expectedValue)[0];
      const expectedWords = new Uint16Array(expectedData.buffer);
      expectedWords.set([value, minBytesPerRow > 2 ? value : 0]);
    } else {
      expectedData.set(new Uint32Array(expectedValue));
    }
    expectedDataBuffer.unmap();

    // The output buffer has one 32-bit entry per buffer row. An entry's value will be 1 if every
    // read from the corresponding row matches the expected data derived above, or 0 otherwise.
    const resultBuffer = this.device.createBuffer({
      size: numRows * 4,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
    });

    this.trackForCleanup(resultBuffer);

    const readsPerRow = Math.ceil(minBytesPerRow / expectedDataSize);
    const reducer = `
    struct Buffer { data: array<u32>; };
    @group(0) @binding(0) var<storage, read> expected: Buffer;
    @group(0) @binding(1) var<storage, read> in: Buffer;
    @group(0) @binding(2) var<storage, read_write> out: Buffer;
    @stage(compute) @workgroup_size(1) fn reduce(
        @builtin(global_invocation_id) id: vec3<u32>) {
      let rowBaseIndex = id.x * ${bytesPerRow / 4}u;
      let readSize = ${expectedDataSize / 4}u;
      out.data[id.x] = 1u;
      for (var i: u32 = 0u; i < ${readsPerRow}u; i = i + 1u) {
        let elementBaseIndex = rowBaseIndex + i * readSize;
        for (var j: u32 = 0u; j < readSize; j = j + 1u) {
          if (in.data[elementBaseIndex + j] != expected.data[j]) {
            out.data[id.x] = 0u;
            return;
          }
        }
      }
    }
    `;

    const pipeline = this.device.createComputePipeline({
      compute: {
        module: this.device.createShaderModule({ code: reducer }),
        entryPoint: 'reduce',
      },
    });

    const bindGroup = this.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: { buffer: expectedDataBuffer } },
        { binding: 1, resource: { buffer: storageBuffer } },
        { binding: 2, resource: { buffer: resultBuffer } },
      ],
    });

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyBufferToBuffer(buffer, 0, storageBuffer, 0, bufferSize);
    const pass = commandEncoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatch(numRows);
    pass.end();
    this.device.queue.submit([commandEncoder.finish()]);

    const expectedResults = new Array(numRows).fill(1);
    this.expectGPUBufferValuesEqual(resultBuffer, new Uint32Array(expectedResults));
  }

  // MAINTENANCE_TODO: add an expectContents for textures, which logs data: uris on failure

  /**
   * Expect a whole GPUTexture to have the single provided color.
   */
  expectSingleColor(src, format, { size, exp, dimension = '2d', slice = 0, layout }) {
    format = resolvePerAspectFormat(format, layout?.aspect);
    const { byteLength, minBytesPerRow, bytesPerRow, rowsPerImage, mipSize } = getTextureCopyLayout(
      format,
      dimension,
      size,
      layout
    );

    const rep = kTexelRepresentationInfo[format];
    const expectedTexelData = rep.pack(rep.encode(exp));

    const buffer = this.device.createBuffer({
      size: byteLength,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    this.trackForCleanup(buffer);

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyTextureToBuffer(
      {
        texture: src,
        mipLevel: layout?.mipLevel,
        origin: { x: 0, y: 0, z: slice },
        aspect: layout?.aspect,
      },

      { buffer, bytesPerRow, rowsPerImage },
      mipSize
    );

    this.queue.submit([commandEncoder.finish()]);

    this.expectGPUBufferRepeatsSingleValue(buffer, {
      expectedValue: expectedTexelData,
      numRows: rowsPerImage,
      minBytesPerRow,
      bytesPerRow,
    });
  }

  /** Return a GPUBuffer that data are going to be written into. */
  readSinglePixelFrom2DTexture(src, format, { x, y }, { slice = 0, layout }) {
    const { byteLength, bytesPerRow, rowsPerImage } = getTextureSubCopyLayout(
      format,
      [1, 1],
      layout
    );

    const buffer = this.device.createBuffer({
      size: byteLength,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
    });

    this.trackForCleanup(buffer);

    const commandEncoder = this.device.createCommandEncoder();
    commandEncoder.copyTextureToBuffer(
      { texture: src, mipLevel: layout?.mipLevel, origin: { x, y, z: slice } },
      { buffer, bytesPerRow, rowsPerImage },
      [1, 1]
    );

    this.queue.submit([commandEncoder.finish()]);

    return buffer;
  }

  /**
   * Expect a single pixel of a 2D texture to have a particular byte representation.
   *
   * MAINTENANCE_TODO: Add check for values of depth/stencil, probably through sampling of shader
   * MAINTENANCE_TODO: Can refactor this and expectSingleColor to use a similar base expect
   */
  expectSinglePixelIn2DTexture(
    src,
    format,
    { x, y },
    { exp, slice = 0, layout, generateWarningOnly = false }
  ) {
    const buffer = this.readSinglePixelFrom2DTexture(src, format, { x, y }, { slice, layout });
    this.expectGPUBufferValuesEqual(buffer, exp, 0, {
      mode: generateWarningOnly ? 'warn' : 'fail',
    });
  }

  /**
   * Take a single pixel of a 2D texture, interpret it using a TypedArray of the `expected` type,
   * and expect each value in that array to be between the corresponding "expected" values
   * (either `a[i] <= actual[i] <= b[i]` or `a[i] >= actual[i] => b[i]`).
   */
  expectSinglePixelBetweenTwoValuesIn2DTexture(
    src,
    format,
    { x, y },
    {
      exp,
      slice = 0,
      layout,
      generateWarningOnly = false,
      checkElementsBetweenFn = (act, [a, b]) => checkElementsBetween(act, [i => a[i], i => b[i]]),
    }
  ) {
    assert(exp[0].constructor === exp[1].constructor);
    const constructor = exp[0].constructor;
    assert(exp[0].length === exp[1].length);
    const typedLength = exp[0].length;

    const buffer = this.readSinglePixelFrom2DTexture(src, format, { x, y }, { slice, layout });
    this.expectGPUBufferValuesPassCheck(buffer, a => checkElementsBetweenFn(a, exp), {
      type: constructor,
      typedLength,
      mode: generateWarningOnly ? 'warn' : 'fail',
    });
  }

  /**
   * Equivalent to {@link expectSinglePixelBetweenTwoValuesIn2DTexture} but uses a special check func
   * to interpret incoming values as float16
   */
  expectSinglePixelBetweenTwoValuesFloat16In2DTexture(
    src,
    format,
    { x, y },
    { exp, slice = 0, layout, generateWarningOnly = false }
  ) {
    this.expectSinglePixelBetweenTwoValuesIn2DTexture(
      src,
      format,
      { x, y },
      {
        exp,
        slice,
        layout,
        generateWarningOnly,
        checkElementsBetweenFn: checkElementsFloat16Between,
      }
    );
  }

  /**
   * Expect the specified WebGPU error to be generated when running the provided function.
   */
  expectGPUError(filter, fn, shouldError = true) {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (!shouldError) {
      return fn();
    }

    this.device.pushErrorScope(filter);
    const returnValue = fn();
    const promise = this.device.popErrorScope();

    this.eventualAsyncExpectation(async niceStack => {
      const error = await promise;

      let failed = false;
      switch (filter) {
        case 'out-of-memory':
          failed = !(error instanceof GPUOutOfMemoryError);
          break;
        case 'validation':
          failed = !(error instanceof GPUValidationError);
          break;
      }

      if (failed) {
        niceStack.message = `Expected ${filter} error`;
        this.rec.expectationFailed(niceStack);
      } else {
        niceStack.message = `Captured ${filter} error`;
        if (error instanceof GPUValidationError) {
          niceStack.message += ` - ${error.message}`;
        }
        this.rec.debug(niceStack);
      }
    });

    return returnValue;
  }

  /**
   * Expect a validation error inside the callback.
   *
   * Tests should always do just one WebGPU call in the callback, to make sure that's what's tested.
   */
  expectValidationError(fn, shouldError = true) {
    // If no error is expected, we let the scope surrounding the test catch it.
    if (shouldError) {
      this.device.pushErrorScope('validation');
    }

    // Note: A return value is not allowed for the callback function. This is to avoid confusion
    // about what the actual behavior would be; either of the following could be reasonable:
    //   - Make expectValidationError async, and have it await on fn(). This causes an async split
    //     between pushErrorScope and popErrorScope, so if the caller doesn't `await` on
    //     expectValidationError (either accidentally or because it doesn't care to do so), then
    //     other test code will be (nondeterministically) caught by the error scope.
    //   - Make expectValidationError NOT await fn(), but just execute its first block (until the
    //     first await) and return the return value (a Promise). This would be confusing because it
    //     would look like the error scope includes the whole async function, but doesn't.
    // If we do decide we need to return a value, we should use the latter semantic.
    const returnValue = fn();
    assert(
      returnValue === undefined,
      'expectValidationError callback should not return a value (or be async)'
    );

    if (shouldError) {
      const promise = this.device.popErrorScope();

      this.eventualAsyncExpectation(async niceStack => {
        const gpuValidationError = await promise;
        if (!gpuValidationError) {
          niceStack.message = 'Validation succeeded unexpectedly.';
          this.rec.validationFailed(niceStack);
        } else if (gpuValidationError instanceof GPUValidationError) {
          niceStack.message = `Validation failed, as expected - ${gpuValidationError.message}`;
          this.rec.debug(niceStack);
        }
      });
    }
  }

  /**
   * Expects that the device should be lost for a particular reason at the teardown of the test.
   */
  expectDeviceLost(reason) {
    assert(
      this.provider !== undefined,
      'No provider available right now; did you "await" selectDeviceOrSkipTestCase?'
    );

    this.provider.expectDeviceLost(reason);
  }

  /**
   * Create a GPUBuffer with the specified contents and usage.
   *
   * MAINTENANCE_TODO: Several call sites would be simplified if this took ArrayBuffer as well.
   */
  makeBufferWithContents(dataArray, usage) {
    return this.trackForCleanup(makeBufferWithContents(this.device, dataArray, usage));
  }

  /**
   * Creates a texture with the contents of a TexelView.
   */
  makeTextureWithContents(texelView, desc) {
    return this.trackForCleanup(makeTextureWithContents(this.device, texelView, desc));
  }

  /**
   * Create a GPUTexture with multiple mip levels, each having the specified contents.
   */
  createTexture2DWithMipmaps(mipmapDataArray) {
    const format = 'rgba8unorm';
    const mipLevelCount = mipmapDataArray.length;
    const textureSizeMipmap0 = 1 << (mipLevelCount - 1);
    const texture = this.device.createTexture({
      mipLevelCount,
      size: { width: textureSizeMipmap0, height: textureSizeMipmap0, depthOrArrayLayers: 1 },
      format,
      usage: GPUTextureUsage.COPY_DST | GPUTextureUsage.TEXTURE_BINDING,
    });

    this.trackForCleanup(texture);

    const textureEncoder = this.device.createCommandEncoder();
    for (let i = 0; i < mipLevelCount; i++) {
      const { byteLength, bytesPerRow, rowsPerImage, mipSize } = getTextureCopyLayout(
        format,
        '2d',
        [textureSizeMipmap0, textureSizeMipmap0, 1],
        { mipLevel: i }
      );

      const data = new Uint8Array(byteLength);
      const mipLevelData = mipmapDataArray[i];
      assert(rowsPerImage === mipSize[0]); // format is rgba8unorm and block size should be 1
      for (let r = 0; r < rowsPerImage; r++) {
        const o = r * bytesPerRow;
        for (let c = o, end = o + mipSize[1] * 4; c < end; c += 4) {
          data[c] = mipLevelData[0];
          data[c + 1] = mipLevelData[1];
          data[c + 2] = mipLevelData[2];
          data[c + 3] = mipLevelData[3];
        }
      }
      const buffer = this.makeBufferWithContents(
        data,
        GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST
      );

      textureEncoder.copyBufferToTexture(
        { buffer, bytesPerRow, rowsPerImage },
        { texture, mipLevel: i, origin: [0, 0, 0] },
        mipSize
      );
    }
    this.device.queue.submit([textureEncoder.finish()]);

    return texture;
  }

  /**
   * Returns a GPUCommandEncoder, GPUComputePassEncoder, GPURenderPassEncoder, or
   * GPURenderBundleEncoder, and a `finish` method returning a GPUCommandBuffer.
   * Allows testing methods which have the same signature across multiple encoder interfaces.
   *
   * @example
   * ```
   * g.test('popDebugGroup')
   *   .params(u => u.combine('encoderType', kEncoderTypes))
   *   .fn(t => {
   *     const { encoder, finish } = t.createEncoder(t.params.encoderType);
   *     encoder.popDebugGroup();
   *   });
   *
   * g.test('writeTimestamp')
   *   .params(u => u.combine('encoderType', ['non-pass', 'compute pass', 'render pass'] as const)
   *   .fn(t => {
   *     const { encoder, finish } = t.createEncoder(t.params.encoderType);
   *     // Encoder type is inferred, so `writeTimestamp` can be used even though it doesn't exist
   *     // on GPURenderBundleEncoder.
   *     encoder.writeTimestamp(args);
   *   });
   * ```
   */
  createEncoder(encoderType, { attachmentInfo, occlusionQuerySet } = {}) {
    const fullAttachmentInfo = {
      // Defaults if not overridden:
      colorFormats: ['rgba8unorm'],
      sampleCount: 1,
      // Passed values take precedent.
      ...attachmentInfo,
    };

    switch (encoderType) {
      case 'non-pass': {
        const encoder = this.device.createCommandEncoder();

        return new CommandBufferMaker(this, encoder, () => {
          return encoder.finish();
        });
      }
      case 'render bundle': {
        const device = this.device;
        const rbEncoder = device.createRenderBundleEncoder(fullAttachmentInfo);
        const pass = this.createEncoder('render pass', { attachmentInfo });

        return new CommandBufferMaker(this, rbEncoder, () => {
          pass.encoder.executeBundles([rbEncoder.finish()]);
          return pass.finish();
        });
      }
      case 'compute pass': {
        const commandEncoder = this.device.createCommandEncoder();
        const encoder = commandEncoder.beginComputePass();

        return new CommandBufferMaker(this, encoder, () => {
          encoder.end();
          return commandEncoder.finish();
        });
      }
      case 'render pass': {
        const makeAttachmentView = format =>
          this.trackForCleanup(
            this.device.createTexture({
              size: [16, 16, 1],
              format,
              usage: GPUTextureUsage.RENDER_ATTACHMENT,
              sampleCount: fullAttachmentInfo.sampleCount,
            })
          ).createView();

        let depthStencilAttachment = undefined;
        if (fullAttachmentInfo.depthStencilFormat !== undefined) {
          depthStencilAttachment = {
            view: makeAttachmentView(fullAttachmentInfo.depthStencilFormat),
            depthReadOnly: fullAttachmentInfo.depthReadOnly,
            stencilReadOnly: fullAttachmentInfo.stencilReadOnly,
          };

          if (
            kTextureFormatInfo[fullAttachmentInfo.depthStencilFormat].depth &&
            !fullAttachmentInfo.depthReadOnly
          ) {
            depthStencilAttachment.depthClearValue = 0;
            depthStencilAttachment.depthLoadOp = 'clear';
            depthStencilAttachment.depthStoreOp = 'discard';
          }
          if (
            kTextureFormatInfo[fullAttachmentInfo.depthStencilFormat].stencil &&
            !fullAttachmentInfo.stencilReadOnly
          ) {
            depthStencilAttachment.stencilClearValue = 1;
            depthStencilAttachment.stencilLoadOp = 'clear';
            depthStencilAttachment.stencilStoreOp = 'discard';
          }
        }
        const passDesc = {
          colorAttachments: Array.from(fullAttachmentInfo.colorFormats, format =>
            format
              ? {
                  view: makeAttachmentView(format),
                  clearValue: [0, 0, 0, 0],
                  loadOp: 'clear',
                  storeOp: 'store',
                }
              : null
          ),

          depthStencilAttachment,
          occlusionQuerySet,
        };

        const commandEncoder = this.device.createCommandEncoder();
        const encoder = commandEncoder.beginRenderPass(passDesc);
        return new CommandBufferMaker(this, encoder, () => {
          encoder.end();
          return commandEncoder.finish();
        });
      }
    }

    unreachable();
  }
}

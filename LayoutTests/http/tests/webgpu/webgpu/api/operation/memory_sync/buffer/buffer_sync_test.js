/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../../../../common/util/util.js';
import { GPUTest } from '../../../../gpu_test.js';
import { checkElementsEqualEither } from '../../../../util/check_contents.js';

const kSize = 4;

export const kAllWriteOps = ['render', 'render-via-bundle', 'compute', 'b2b-copy', 't2b-copy'];

// Note: If it would be useful to have any of these helpers be separate from the fixture,
// they can be refactored into standalone functions.
export class BufferSyncTest extends GPUTest {
  // Create a buffer, and initialize it to a specified value for all elements.
  async createBufferWithValue(initValue) {
    const buffer = this.device.createBuffer({
      mappedAtCreation: true,
      size: kSize,
      usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
    });

    new Uint32Array(buffer.getMappedRange()).fill(initValue);
    buffer.unmap();
    await this.queue.onSubmittedWorkDone();
    return buffer;
  }

  // Create a texture, and initialize it to a specified value for all elements.
  async createTextureWithValue(initValue) {
    const data = new Uint32Array(kSize / 4).fill(initValue);
    const texture = this.device.createTexture({
      size: { width: kSize / 4, height: 1, depthOrArrayLayers: 1 },
      format: 'r32uint',
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.COPY_DST,
    });

    this.device.queue.writeTexture(
      { texture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
      data,
      { offset: 0, bytesPerRow: kSize, rowsPerImage: 1 },
      { width: kSize / 4, height: 1, depthOrArrayLayers: 1 }
    );

    await this.queue.onSubmittedWorkDone();
    return texture;
  }

  createBindGroup(pipeline, buffer) {
    return this.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: [{ binding: 0, resource: { buffer } }],
    });
  }

  // Create a compute pipeline and write given data into storage buffer.
  createStorageWriteComputePipeline(value) {
    const wgslCompute = `
      struct Data {
        a : i32;
      };

      @group(0) @binding(0) var<storage, read_write> data : Data;
      @stage(compute) @workgroup_size(1) fn main() {
        data.a = ${value};
        return;
      }
    `;

    return this.device.createComputePipeline({
      compute: {
        module: this.device.createShaderModule({
          code: wgslCompute,
        }),

        entryPoint: 'main',
      },
    });
  }

  // Create a render pipeline and write given data into storage buffer at fragment stage.
  createStorageWriteRenderPipeline(value) {
    const wgslShaders = {
      vertex: `
      @stage(vertex) fn vert_main() -> @builtin(position) vec4<f32> {
        return vec4<f32>(0.5, 0.5, 0.0, 1.0);
      }
    `,

      fragment: `
      struct Data {
        a : i32;
      };

      @group(0) @binding(0) var<storage, read_write> data : Data;
      @stage(fragment) fn frag_main() -> @location(0) vec4<f32> {
        data.a = ${value};
        return vec4<f32>(1.0, 0.0, 0.0, 1.0);
      }
    `,
    };

    return this.device.createRenderPipeline({
      vertex: {
        module: this.device.createShaderModule({
          code: wgslShaders.vertex,
        }),

        entryPoint: 'vert_main',
      },

      fragment: {
        module: this.device.createShaderModule({
          code: wgslShaders.fragment,
        }),

        entryPoint: 'frag_main',
        targets: [{ format: 'rgba8unorm' }],
      },

      primitive: { topology: 'point-list' },
    });
  }

  beginSimpleRenderPass(encoder) {
    const view = this.device
      .createTexture({
        size: { width: 1, height: 1, depthOrArrayLayers: 1 },
        format: 'rgba8unorm',
        usage: GPUTextureUsage.RENDER_ATTACHMENT,
      })
      .createView();
    return encoder.beginRenderPass({
      colorAttachments: [
        {
          view,
          clearValue: { r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    });
  }

  // Write buffer via draw call in render pass. Use bundle if needed.
  encodeWriteAsStorageBufferInRenderPass(encoder, buffer, value, inBundle) {
    const pipeline = this.createStorageWriteRenderPipeline(value);
    const bindGroup = this.createBindGroup(pipeline, buffer);

    const pass = this.beginSimpleRenderPass(encoder);
    const renderer = inBundle
      ? this.device.createRenderBundleEncoder({ colorFormats: ['rgba8unorm'] })
      : pass;
    renderer.setBindGroup(0, bindGroup);
    renderer.setPipeline(pipeline);
    renderer.draw(1, 1, 0, 0);

    if (inBundle) pass.executeBundles([renderer.finish()]);
    pass.end();
  }

  // Write buffer via dispatch call in compute pass.
  encodeWriteAsStorageBufferInComputePass(encoder, buffer, value) {
    const pipeline = this.createStorageWriteComputePipeline(value);
    const bindGroup = this.createBindGroup(pipeline, buffer);
    const pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatch(1);
    pass.end();
  }

  /** Write buffer via BufferToBuffer copy. */
  async encodeWriteByB2BCopy(encoder, buffer, value) {
    const tmpBuffer = await this.createBufferWithValue(value);

    // The write operation via b2b copy is just encoded into command encoder, it doesn't write immediately.
    encoder.copyBufferToBuffer(tmpBuffer, 0, buffer, 0, kSize);
  }

  // Write buffer via TextureToBuffer copy.
  async encodeWriteByT2BCopy(encoder, buffer, value) {
    const tmpTexture = await this.createTextureWithValue(value);

    // The write operation via t2b copy is just encoded into command encoder, it doesn't write immediately.
    encoder.copyTextureToBuffer(
      { texture: tmpTexture, mipLevel: 0, origin: { x: 0, y: 0, z: 0 } },
      { buffer, bytesPerRow: 256 },
      { width: 1, height: 1, depthOrArrayLayers: 1 }
    );
  }

  // Write buffer via writeBuffer API on queue
  writeByWriteBuffer(buffer, value) {
    const data = new Uint32Array(kSize / 4).fill(value);
    this.device.queue.writeBuffer(buffer, 0, data);
  }

  // Issue write operation via render pass, compute pass, copy, etc.
  async encodeWriteOp(encoder, writeOp, buffer, value) {
    switch (writeOp) {
      case 'render':
        this.encodeWriteAsStorageBufferInRenderPass(encoder, buffer, value, false);
        break;
      case 'render-via-bundle':
        this.encodeWriteAsStorageBufferInRenderPass(encoder, buffer, value, true);
        break;
      case 'compute':
        this.encodeWriteAsStorageBufferInComputePass(encoder, buffer, value);
        break;
      case 'b2b-copy':
        await this.encodeWriteByB2BCopy(encoder, buffer, value);
        break;
      case 't2b-copy':
        await this.encodeWriteByT2BCopy(encoder, buffer, value);
        break;
      default:
        assert(false);
    }
  }

  async createCommandBufferWithWriteOp(writeOp, buffer, value) {
    const encoder = this.device.createCommandEncoder();
    await this.encodeWriteOp(encoder, writeOp, buffer, value);
    return encoder.finish();
  }

  async submitWriteOp(writeOp, buffer, value) {
    if (writeOp === 'write-buffer') {
      this.writeByWriteBuffer(buffer, value);
    } else {
      const encoder = this.device.createCommandEncoder();
      await this.encodeWriteOp(encoder, writeOp, buffer, value);
      this.device.queue.submit([encoder.finish()]);
    }
  }

  verifyData(buffer, expectedValue) {
    const bufferData = new Uint32Array(1);
    bufferData[0] = expectedValue;
    this.expectGPUBufferValuesEqual(buffer, bufferData);
  }

  verifyDataTwoValidValues(buffer, expectedValue1, expectedValue2) {
    const bufferData1 = new Uint32Array(1);
    bufferData1[0] = expectedValue1;
    const bufferData2 = new Uint32Array(1);
    bufferData2[0] = expectedValue2;
    this.expectGPUBufferValuesPassCheck(
      buffer,
      a => checkElementsEqualEither(a, [bufferData1, bufferData2]),
      { type: Uint32Array, typedLength: 1 }
    );
  }
}

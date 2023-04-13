/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for external textures from HTMLVideoElement (and other video-type sources?).

- videos with various encodings/formats (webm vp8, webm vp9, ogg theora, mp4), color spaces
  (bt.601, bt.709, bt.2020)
- TODO: enhance with more cases with crop, rotation, etc.

TODO: consider whether external_texture and copyToTexture video tests should be in the same file
`;
import { getResourcePath } from '../../../common/framework/resources.js';
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { makeTable } from '../../../common/util/data_tables.js';
import { GPUTest } from '../../gpu_test.js';
import {
  startPlayingAndWaitForVideo,
  getVideoColorSpaceInit,
  getVideoFrameFromVideoElement,
  waitForNextFrame,
} from '../../web_platform/util.js';

const kHeight = 16;
const kWidth = 16;
const kFormat = 'rgba8unorm';

const kVideoInfo = makeTable(['colorSpace', 'mimeType'], [undefined, undefined], {
  // All video names
  'red-green.webmvp8.webm': ['REC601', 'video/webm; codecs=vp8'],
  'red-green.theora.ogv': ['REC601', 'video/ogg; codecs=theora'],
  'red-green.mp4': ['REC601', 'video/mp4; codecs=avc1.4d400c'],
  'red-green.bt601.vp9.webm': ['REC601', 'video/webm; codecs=vp9'],
  'red-green.bt709.vp9.webm': ['REC709', 'video/webm; codecs=vp9'],
  'red-green.bt2020.vp9.webm': ['REC2020', 'video/webm; codecs=vp9'],
});

const kVideoExpectations = [
  {
    videoName: 'red-green.webmvp8.webm',
    _redExpectation: new Uint8Array([0xf8, 0x24, 0x00, 0xff]),
    _greenExpectation: new Uint8Array([0x3f, 0xfb, 0x00, 0xff]),
  },
  {
    videoName: 'red-green.theora.ogv',
    _redExpectation: new Uint8Array([0xf8, 0x24, 0x00, 0xff]),
    _greenExpectation: new Uint8Array([0x3f, 0xfb, 0x00, 0xff]),
  },
  {
    videoName: 'red-green.mp4',
    _redExpectation: new Uint8Array([0xf8, 0x24, 0x00, 0xff]),
    _greenExpectation: new Uint8Array([0x3f, 0xfb, 0x00, 0xff]),
  },
  {
    videoName: 'red-green.bt601.vp9.webm',
    _redExpectation: new Uint8Array([0xf8, 0x24, 0x00, 0xff]),
    _greenExpectation: new Uint8Array([0x3f, 0xfb, 0x00, 0xff]),
  },
  {
    videoName: 'red-green.bt709.vp9.webm',
    _redExpectation: new Uint8Array([0xff, 0x00, 0x00, 0xff]),
    _greenExpectation: new Uint8Array([0x00, 0xff, 0x00, 0xff]),
  },
  {
    videoName: 'red-green.bt2020.vp9.webm',
    _redExpectation: new Uint8Array([0xff, 0x00, 0x00, 0xff]),
    _greenExpectation: new Uint8Array([0x00, 0xff, 0x00, 0xff]),
  },
];

export const g = makeTestGroup(GPUTest);

function createExternalTextureSamplingTestPipeline(t) {
  const pipeline = t.device.createRenderPipeline({
    layout: 'auto',
    vertex: {
      module: t.device.createShaderModule({
        code: `
        @vertex fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
            var pos = array<vec4<f32>, 6>(
              vec4<f32>( 1.0,  1.0, 0.0, 1.0),
              vec4<f32>( 1.0, -1.0, 0.0, 1.0),
              vec4<f32>(-1.0, -1.0, 0.0, 1.0),
              vec4<f32>( 1.0,  1.0, 0.0, 1.0),
              vec4<f32>(-1.0, -1.0, 0.0, 1.0),
              vec4<f32>(-1.0,  1.0, 0.0, 1.0)
            );
            return pos[VertexIndex];
        }
        `,
      }),
      entryPoint: 'main',
    },
    fragment: {
      module: t.device.createShaderModule({
        code: `
        @group(0) @binding(0) var s : sampler;
        @group(0) @binding(1) var t : texture_external;

        @fragment fn main(@builtin(position) FragCoord : vec4<f32>)
                                 -> @location(0) vec4<f32> {
            return textureSampleBaseClampToEdge(t, s, FragCoord.xy / vec2<f32>(16.0, 16.0));
        }
        `,
      }),
      entryPoint: 'main',
      targets: [
        {
          format: kFormat,
        },
      ],
    },
    primitive: { topology: 'triangle-list' },
  });

  return pipeline;
}

function createExternalTextureSamplingTestBindGroup(t, source, pipeline) {
  const linearSampler = t.device.createSampler();

  const externalTexture = t.device.importExternalTexture({
    source: source,
  });

  const bindGroup = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      {
        binding: 0,
        resource: linearSampler,
      },
      {
        binding: 1,
        resource: externalTexture,
      },
    ],
  });

  return bindGroup;
}

function getVideoElementAndInfo(t, sourceType, videoName) {
  if (sourceType === 'VideoFrame' && typeof VideoFrame === 'undefined') {
    t.skip('WebCodec is not supported');
  }

  const videoElement = document.createElement('video');
  const videoInfo = kVideoInfo[videoName];

  if (videoElement.canPlayType(videoInfo.mimeType) === '') {
    t.skip('Video codec is not supported');
  }

  const videoUrl = getResourcePath(videoName);
  videoElement.src = videoUrl;

  return { videoElement, videoInfo };
}

g.test('importExternalTexture,sample')
  .desc(
    `
Tests that we can import an HTMLVideoElement/VideoFrame into a GPUExternalTexture, sample from it
for several combinations of video format and color space.
`
  )
  .params(u =>
    u //
      .combine('sourceType', ['VideoElement', 'VideoFrame'])
      .combineWithParams(kVideoExpectations)
  )
  .fn(async t => {
    const sourceType = t.params.sourceType;
    const { videoElement, videoInfo } = getVideoElementAndInfo(t, sourceType, t.params.videoName);

    await startPlayingAndWaitForVideo(videoElement, async () => {
      const source =
        sourceType === 'VideoFrame'
          ? await getVideoFrameFromVideoElement(
              t,
              videoElement,
              getVideoColorSpaceInit(videoInfo.colorSpace)
            )
          : videoElement;

      const colorAttachment = t.device.createTexture({
        format: kFormat,
        size: { width: kWidth, height: kHeight, depthOrArrayLayers: 1 },
        usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
      });

      const pipeline = createExternalTextureSamplingTestPipeline(t);
      const bindGroup = createExternalTextureSamplingTestBindGroup(t, source, pipeline);

      const commandEncoder = t.device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass({
        colorAttachments: [
          {
            view: colorAttachment.createView(),
            clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
            loadOp: 'clear',
            storeOp: 'store',
          },
        ],
      });
      passEncoder.setPipeline(pipeline);
      passEncoder.setBindGroup(0, bindGroup);
      passEncoder.draw(6);
      passEncoder.end();
      t.device.queue.submit([commandEncoder.finish()]);

      // Top left corner should be red. Sample a few pixels away from the edges to avoid compression
      // artifacts.
      t.expectSinglePixelIn2DTexture(
        colorAttachment,
        kFormat,
        { x: 5, y: 5 },
        {
          exp: t.params._redExpectation,
        }
      );

      // Bottom right corner should be green. Sample a few pixels away from the edges to avoid
      // compression artifacts.
      t.expectSinglePixelIn2DTexture(
        colorAttachment,
        kFormat,
        { x: kWidth - 5, y: kHeight - 5 },
        {
          exp: t.params._greenExpectation,
        }
      );

      if (sourceType === 'VideoFrame') source.close();
    });
  });

g.test('importExternalTexture,expired')
  .desc(
    `
Tests that GPUExternalTexture.expired is false when HTMLVideoElement is not updated
or VideoFrame(webcodec) is alive. And it will be changed to true when imported
HTMLVideoElement is updated or imported VideoFrame is closed. Using expired
GPUExternalTexture results in an error.

TODO: Make this test work without requestVideoFrameCallback support (in waitForNextFrame).
`
  )
  .params(u =>
    u //
      .combine('sourceType', ['VideoElement', 'VideoFrame'])
  )
  .fn(async t => {
    const sourceType = t.params.sourceType;
    const { videoElement } = getVideoElementAndInfo(t, sourceType, 'red-green.webmvp8.webm');

    if (!('requestVideoFrameCallback' in videoElement)) {
      t.skip('HTMLVideoElement.requestVideoFrameCallback is not supported');
    }

    const colorAttachment = t.device.createTexture({
      format: kFormat,
      size: { width: kWidth, height: kHeight, depthOrArrayLayers: 1 },
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });
    const passDescriptor = {
      colorAttachments: [
        {
          view: colorAttachment.createView(),
          clearValue: [0, 0, 0, 1],
          loadOp: 'clear',
          storeOp: 'store',
        },
      ],
    };

    const bindGroupLayout = t.device.createBindGroupLayout({
      entries: [{ binding: 0, visibility: GPUShaderStage.FRAGMENT, externalTexture: {} }],
    });

    let bindGroup;
    const useExternalTexture = () => {
      const commandEncoder = t.device.createCommandEncoder();
      const passEncoder = commandEncoder.beginRenderPass(passDescriptor);
      passEncoder.setBindGroup(0, bindGroup);
      passEncoder.end();
      return commandEncoder.finish();
    };

    let externalTexture;
    await startPlayingAndWaitForVideo(videoElement, async () => {
      const source =
        sourceType === 'VideoFrame'
          ? await getVideoFrameFromVideoElement(t, videoElement)
          : videoElement;
      externalTexture = t.device.importExternalTexture({
        source: source,
      });
      // Set `bindGroup` here, which will then be used in microtask1 and microtask3.
      bindGroup = t.device.createBindGroup({
        layout: bindGroupLayout,
        entries: [{ binding: 0, resource: externalTexture }],
      });

      const commandBuffer = useExternalTexture();
      t.expectGPUError('validation', () => t.device.queue.submit([commandBuffer]), false);
      t.expect(!externalTexture.expired);

      if (sourceType === 'VideoFrame') {
        source.close();
        const commandBuffer = useExternalTexture();
        t.expectGPUError('validation', () => t.device.queue.submit([commandBuffer]), true);
        t.expect(externalTexture.expired);
      }
    });

    if (sourceType === 'VideoElement') {
      // Update new video frame.
      await waitForNextFrame(videoElement, () => {
        // VideoFrame is updated. GPUExternalTexture imported from HTMLVideoElement should be expired.
        // Using the GPUExternalTexture should result in an error.
        const commandBuffer = useExternalTexture();
        t.expectGPUError('validation', () => t.device.queue.submit([commandBuffer]), true);
        t.expect(externalTexture.expired);
      });
    }
  });

g.test('importExternalTexture,compute')
  .desc(
    `
Tests that we can import an HTMLVideoElement/VideoFrame into a GPUExternalTexture and use it in a
compute shader, for several combinations of video format and color space.
`
  )
  .params(u =>
    u //
      .combine('sourceType', ['VideoElement', 'VideoFrame'])
      .combineWithParams(kVideoExpectations)
  )
  .fn(async t => {
    const sourceType = t.params.sourceType;
    const { videoElement, videoInfo } = getVideoElementAndInfo(t, sourceType, t.params.videoName);

    await startPlayingAndWaitForVideo(videoElement, async () => {
      const source =
        sourceType === 'VideoFrame'
          ? await getVideoFrameFromVideoElement(
              t,
              videoElement,
              getVideoColorSpaceInit(videoInfo.colorSpace)
            )
          : videoElement;
      const externalTexture = t.device.importExternalTexture({
        source: source,
      });

      const outputTexture = t.device.createTexture({
        format: 'rgba8unorm',
        size: [2, 1, 1],
        usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.STORAGE_BINDING,
      });

      const pipeline = t.device.createComputePipeline({
        layout: 'auto',
        compute: {
          // Shader will load a pixel near the upper left and lower right corners, which are then
          // stored in storage texture.
          module: t.device.createShaderModule({
            code: `
              @group(0) @binding(0) var t : texture_external;
              @group(0) @binding(1) var outImage : texture_storage_2d<rgba8unorm, write>;

              @compute @workgroup_size(1) fn main() {
                var red : vec4<f32> = textureLoad(t, vec2<i32>(10,10));
                textureStore(outImage, vec2<i32>(0, 0), red);
                var green : vec4<f32> = textureLoad(t, vec2<i32>(70,118));
                textureStore(outImage, vec2<i32>(1, 0), green);
                return;
              }
            `,
          }),
          entryPoint: 'main',
        },
      });

      const bg = t.device.createBindGroup({
        entries: [
          { binding: 0, resource: externalTexture },
          { binding: 1, resource: outputTexture.createView() },
        ],

        layout: pipeline.getBindGroupLayout(0),
      });

      const encoder = t.device.createCommandEncoder();
      const pass = encoder.beginComputePass();
      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bg);
      pass.dispatchWorkgroups(1);
      pass.end();
      t.device.queue.submit([encoder.finish()]);

      // Pixel loaded from top left corner should be red.
      t.expectSinglePixelIn2DTexture(
        outputTexture,
        kFormat,
        { x: 0, y: 0 },
        {
          exp: t.params._redExpectation,
        }
      );

      // Pixel loaded from Bottom right corner should be green.
      t.expectSinglePixelIn2DTexture(
        outputTexture,
        kFormat,
        { x: 1, y: 0 },
        {
          exp: t.params._greenExpectation,
        }
      );

      if (sourceType === 'VideoFrame') source.close();
    });
  });

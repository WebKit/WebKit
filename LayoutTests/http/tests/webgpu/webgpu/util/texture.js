/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from '../../common/util/util.js';
import { kTextureFormatInfo } from '../capability_info.js';
import { align } from './math.js';

import { reifyExtent3D } from './unions.js';

/**
 * Creates a texture with the contents of a TexelView.
 */
export function makeTextureWithContents(device, texelView, desc) {
  const { width, height, depthOrArrayLayers } = reifyExtent3D(desc.size);

  const { bytesPerBlock, blockWidth } = kTextureFormatInfo[texelView.format];
  // Currently unimplemented for compressed textures.
  assert(blockWidth === 1);

  // Compute bytes per row.
  const bytesPerRow = align(bytesPerBlock * width, 256);

  // Create a staging buffer to upload the texture contents.
  const stagingBuffer = device.createBuffer({
    mappedAtCreation: true,
    size: bytesPerRow * height * depthOrArrayLayers,
    usage: GPUBufferUsage.COPY_SRC,
  });

  // Write the texels into the staging buffer.
  texelView.writeTextureData(new Uint8Array(stagingBuffer.getMappedRange()), {
    bytesPerRow,
    rowsPerImage: height,
    subrectOrigin: [0, 0, 0],
    subrectSize: [width, height, depthOrArrayLayers],
  });
  stagingBuffer.unmap();

  // Create the texture.
  const texture = device.createTexture({
    ...desc,
    format: texelView.format,
    usage: desc.usage | GPUTextureUsage.COPY_DST,
  });

  // Copy from the staging buffer into the texture.
  const commandEncoder = device.createCommandEncoder();
  commandEncoder.copyBufferToTexture(
    { buffer: stagingBuffer, bytesPerRow },
    { texture },
    desc.size
  );

  device.queue.submit([commandEncoder.finish()]);

  // Clean up the staging buffer.
  stagingBuffer.destroy();

  return texture;
}

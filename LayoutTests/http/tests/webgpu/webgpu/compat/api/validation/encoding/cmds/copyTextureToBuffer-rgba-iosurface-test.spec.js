/**
* Test for BGRA→RGBA swizzling in copyTextureToBuffer with IOSurface-backed textures
* This test verifies the fix for WebKit Bug #296435
**/
export const description = `
Tests BGRA→RGBA swizzling in copyTextureToBuffer when RGBA WebGPU textures are backed by BGRA IOSurfaces.
This addresses the issue where copyTextureToBuffer needed to handle format mismatches between WebGPU RGBA
formats and the underlying BGRA IOSurface representation.
`;

import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { CompatibilityTest } from '../../../../compatibility_test.js';

export const g = makeTestGroup(CompatibilityTest);

g.test('rgba_iosurface_swizzling')
  .desc(`Tests that copyTextureToBuffer correctly swizzles BGRA→RGBA when an RGBA WebGPU texture is backed by a BGRA IOSurface`)
  .params((u) => u.combine('format', ['rgba8unorm', 'rgba8unorm-srgb'] as const))
  .fn(async (t) => {
    const { format } = t.params;
    
    // Create a canvas element to trigger IOSurface backing (on platforms that support it)
    const canvas = document.createElement('canvas');
    canvas.width = 4;
    canvas.height = 4;
    
    // Get WebGPU context from canvas - this may create IOSurface-backed textures
    const context = canvas.getContext('webgpu');
    if (!context) {
      t.skip('WebGPU canvas context not available');
      return;
    }
    
    // Configure canvas with RGBA format
    context.configure({
      device: t.device,
      format: format,
      usage: GPUTextureUsage.COPY_SRC | GPUTextureUsage.RENDER_ATTACHMENT,
    });
    
    // Get the canvas texture (potentially IOSurface-backed)
    const canvasTexture = context.getCurrentTexture();
    
    // Create a test pattern with known BGRA values
    // We'll render blue=255, green=128, red=64, alpha=255 (BGRA order)
    // After swizzling, we expect red=64, green=128, blue=255, alpha=255 (RGBA order)
    const renderPassDescriptor = {
      colorAttachments: [{
        view: canvasTexture.createView(),
        clearValue: { r: 64/255, g: 128/255, b: 255/255, a: 1.0 }, // RGBA values
        loadOp: 'clear' as const,
        storeOp: 'store' as const,
      }],
    };
    
    const commandEncoder = t.device.createCommandEncoder();
    const renderPass = commandEncoder.beginRenderPass(renderPassDescriptor);
    renderPass.end();
    
    // Create buffer to copy texture data to
    const bytesPerPixel = 4; // RGBA8
    const bytesPerRow = canvas.width * bytesPerPixel;
    const bufferSize = bytesPerRow * canvas.height;
    
    const outputBuffer = t.createBufferTracked({
      size: bufferSize,
      usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
    });
    
    // Copy texture to buffer - this should trigger swizzling if needed
    commandEncoder.copyTextureToBuffer(
      { texture: canvasTexture },
      { buffer: outputBuffer, bytesPerRow },
      { width: canvas.width, height: canvas.height }
    );
    
    t.device.queue.submit([commandEncoder.finish()]);
    
    // Read back the data and verify swizzling occurred correctly
    await outputBuffer.mapAsync(GPUMapMode.READ);
    const outputData = new Uint8Array(outputBuffer.getMappedRange());
    
    // Check first pixel - should be RGBA order: [64, 128, 255, 255]
    const expectedR = 64;
    const expectedG = 128; 
    const expectedB = 255;
    const expectedA = 255;
    
    const actualR = outputData[0];
    const actualG = outputData[1];
    const actualB = outputData[2];
    const actualA = outputData[3];
    
    // Allow small tolerance for format conversion
    const tolerance = format.includes('srgb') ? 2 : 1;
    
    t.expect(
      Math.abs(actualR - expectedR) <= tolerance,
      `Red channel mismatch: expected ${expectedR}, got ${actualR}`
    );
    t.expect(
      Math.abs(actualG - expectedG) <= tolerance,
      `Green channel mismatch: expected ${expectedG}, got ${actualG}`
    );
    t.expect(
      Math.abs(actualB - expectedB) <= tolerance,
      `Blue channel mismatch: expected ${expectedB}, got ${actualB}`
    );
    t.expect(
      Math.abs(actualA - expectedA) <= tolerance,
      `Alpha channel mismatch: expected ${expectedA}, got ${actualA}`
    );
    
    outputBuffer.unmap();
  });
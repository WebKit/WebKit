/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

const SpatialRenderTest = {};

SpatialRenderTest.encodeDirection = function(longitude, latitude)
{
    return [
        Math.round((longitude + Math.PI) / (2 * Math.PI) * 255),
        Math.round((latitude + Math.PI / 2) / Math.PI * 255),
        128,
    ];
};

SpatialRenderTest.decodeDirection = function(red, green)
{
    return {
        longitude: red / 255 * 2 * Math.PI - Math.PI,
        latitude: green / 255 * Math.PI - Math.PI / 2,
    };
};

SpatialRenderTest.directionToLongitudeLatitude = function([x, y, z])
{
    const length = Math.hypot(x, y, z);
    return {
        longitude: Math.atan2(x / length, -z / length),
        latitude: Math.asin(Math.min(1, Math.max(-1, y / length))),
    };
};

SpatialRenderTest.paintGrid = function(width, height, texelToDirection)
{
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext("2d");
    const image = context.createImageData(width, height);
    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const direction = texelToDirection((x + 0.5) / width, (y + 0.5) / height);
            const offset = (y * width + x) * 4;
            if (!direction) {
                image.data[offset + 3] = 255;
                continue;
            }
            const { longitude, latitude } = SpatialRenderTest.directionToLongitudeLatitude(direction);
            const [red, green, blue] = SpatialRenderTest.encodeDirection(longitude, latitude);
            image.data[offset] = red;
            image.data[offset + 1] = green;
            image.data[offset + 2] = blue;
            image.data[offset + 3] = 255;
        }
    }
    context.putImageData(image, 0, 0);
    return canvas;
};

// Brings up a renderer for one projection without going through _maybeEnable, which would need a
// video with real dimensions and metadata.
SpatialRenderTest.createRenderer = function(projection, { width = 256, height = 256, fovDegrees = null } = {})
{
    const container = document.createElement("div");
    container.style.position = "relative";
    container.style.width = width + "px";
    container.style.height = height + "px";
    document.body.appendChild(container);

    const media = document.createElement("video");
    container.appendChild(media);

    const support = new SpatialVideoSupport({
        media,
        host: { spatialVideoRenderingEnabled: false },
        shadowRoot: container,
        container,
    });

    support._projection = projection;
    support._fovDegrees = fovDegrees;
    support._enable();
    if (!support._active)
        return null;

    // The texture is supplied by the test rather than by video frames, so keep _drawLoop from
    // replacing it, and sample it directly instead of through mipmaps the test never builds.
    support._useVideoFrameCallback = true;
    const gl = support._gl;
    gl.bindTexture(gl.TEXTURE_2D, support._texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    return support;
};

SpatialRenderTest.uploadGrid = function(support, grid)
{
    const gl = support._gl;
    gl.bindTexture(gl.TEXTURE_2D, support._texture);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, grid);
};

SpatialRenderTest.directionAtCentre = function(support, yawDegrees, pitchDegrees)
{
    support._yaw = yawDegrees * Math.PI / 180;
    support._pitch = pitchDegrees * Math.PI / 180;
    support._drawLoop();
    if (support._animationFrame)
        cancelAnimationFrame(support._animationFrame);

    const gl = support._gl;
    const pixel = new Uint8Array(4);
    gl.readPixels(Math.floor(gl.drawingBufferWidth / 2), Math.floor(gl.drawingBufferHeight / 2),
        1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);
    return SpatialRenderTest.decodeDirection(pixel[0], pixel[1]);
};

// Measured as the angle between the two directions, since a difference of longitude and latitude
// would overstate it near the poles where lines of longitude crowd together.
SpatialRenderTest.aimErrorDegrees = function(support, yawDegrees, pitchDegrees)
{
    const { longitude, latitude } = SpatialRenderTest.directionAtCentre(support, yawDegrees, pitchDegrees);
    const rendered = [
        Math.cos(latitude) * Math.sin(longitude),
        Math.sin(latitude),
        -Math.cos(latitude) * Math.cos(longitude),
    ];

    const yaw = yawDegrees * Math.PI / 180;
    const pitch = pitchDegrees * Math.PI / 180;
    const aimed = [
        Math.sin(yaw) * Math.cos(pitch),
        -Math.sin(pitch),
        -Math.cos(yaw) * Math.cos(pitch),
    ];

    const dot = rendered[0] * aimed[0] + rendered[1] * aimed[1] + rendered[2] * aimed[2];
    return Math.acos(Math.min(1, Math.max(-1, dot))) * 180 / Math.PI;
};

SpatialRenderTest.destroy = function(support)
{
    const container = support.mediaController.container;
    support._teardown();
    container.remove();
};

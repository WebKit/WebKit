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

class SpatialVideoSupport extends MediaControllerSupport
{
    constructor(mediaController)
    {
        super(mediaController);
        this._active = false;
        this._yaw = 0;
        this._pitch = 0;
        this._dragging = false;
        this._moved = false;
        this._lastX = 0;
        this._lastY = 0;
        this._projection = "equirect360";
        this._maybeEnable();
    }

    get mediaEvents()
    {
        return ["loadedmetadata", "resize"];
    }

    get tracksToMonitor()
    {
        const videoTracks = this.mediaController.media.videoTracks;
        return videoTracks ? [videoTracks] : [];
    }

    enable()
    {
        const host = this.mediaController.host;
        if (!host || !host.spatialVideoRenderingEnabled)
            return;
        super.enable();
    }

    handleEvent()
    {
        this._maybeEnable();
    }

    disable()
    {
        super.disable();
        this._teardown();
    }

    _maybeEnable()
    {
        const media = this.mediaController.media;
        if (this._active || !(media instanceof HTMLVideoElement))
            return;

        const host = this.mediaController.host;
        if (!host || !host.spatialVideoRenderingEnabled)
            return;

        const resolved = this._resolveProjection(media, host);
        if (!resolved)
            return;
        this._projection = resolved.projection;
        this._fovDegrees = resolved.fovDegrees;

        if (!media.videoWidth || !media.videoHeight)
            return;

        this._enable();
    }

    _resolveProjection(media, host)
    {
        const declared = ProjectionAttributeValues[media.getAttribute("x-webkit-projection")?.trim().toLowerCase()];
        if (declared)
            return { projection: declared, fovDegrees: null };

        const fov = typeof host.spatialVideoHorizontalFieldOfView === "number" ? host.spatialVideoHorizontalFieldOfView : null;
        switch (host.spatialVideoProjectionKind) {
        case "Equirectangular":
            return { projection: "equirect360", fovDegrees: null };
        case "HalfEquirectangular":
            return { projection: "equirect180", fovDegrees: null };
        case "Parametric":
        case "AppleImmersiveVideo":
            return { projection: "wideFOV", fovDegrees: fov };
        }
        return null;
    }

    _enable()
    {
        const shadowRoot = this.mediaController.shadowRoot;
        if (!shadowRoot)
            return;

        const canvas = document.createElement("canvas");
        canvas.style.position = "absolute";
        canvas.style.top = "0";
        canvas.style.left = "0";
        canvas.style.width = "100%";
        canvas.style.height = "100%";
        canvas.style.zIndex = "0";
        canvas.style.visibility = "visible";
        canvas.style.touchAction = "none";
        this.mediaController.container.insertBefore(canvas, this.mediaController.container.firstChild);
        this._canvas = canvas;

        const gl = canvas.getContext("webgl2", { antialias: true, alpha: false });
        if (!gl) {
            canvas.remove();
            this._canvas = null;
            return;
        }
        this._gl = gl;

        if (!this._initGL()) {
            canvas.remove();
            this._canvas = null;
            return;
        }

        this._installInteraction(canvas);
        this._active = true;
        this._resize();
        this._startRenderLoops();
    }

    _initGL()
    {
        const gl = this._gl;
        const vertexShader = `
            attribute vec3 aPosition;
            attribute vec2 aTexCoord;
            attribute float aEdgeDistance;
            uniform mat4 uModelViewProjection;
            varying vec2 vTexCoord;
            varying float vEdgeDistance;
            void main() {
                vTexCoord = aTexCoord;
                vEdgeDistance = aEdgeDistance;
                gl_Position = uModelViewProjection * vec4(aPosition, 1.0);
            }`;
        const fragmentShader = `
            precision mediump float;
            varying vec2 vTexCoord;
            varying float vEdgeDistance;
            uniform sampler2D uTexture;
            uniform float uFeather;
            void main() {
                vec4 color = texture2D(uTexture, vTexCoord);
                if (uFeather > 0.0)
                    color.rgb *= 1.0 - smoothstep(1.0 - uFeather, 1.0, vEdgeDistance);
                gl_FragColor = color;
            }`;

        const program = this._link(vertexShader, fragmentShader);
        if (!program)
            return false;
        this._program = program;
        gl.useProgram(program);
        this._positionLocation = gl.getAttribLocation(program, "aPosition");
        this._texCoordLocation = gl.getAttribLocation(program, "aTexCoord");
        this._edgeDistanceLocation = gl.getAttribLocation(program, "aEdgeDistance");
        this._mvpLocation = gl.getUniformLocation(program, "uModelViewProjection");
        this._featherLocation = gl.getUniformLocation(program, "uFeather");

        this._buildMesh(this._projection);

        this._texture = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, this._texture);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        const anisoExtension = gl.getExtension("EXT_texture_filter_anisotropic");
        if (anisoExtension) {
            const maxAniso = gl.getParameter(anisoExtension.MAX_TEXTURE_MAX_ANISOTROPY_EXT);
            gl.texParameterf(gl.TEXTURE_2D, anisoExtension.TEXTURE_MAX_ANISOTROPY_EXT, Math.min(8, maxAniso));
        }
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array([0, 0, 0, 255]));
        return true;
    }

    _link(vertexSource, fragmentSource)
    {
        const gl = this._gl;
        const compile = (type, source) => {
            const shader = gl.createShader(type);
            gl.shaderSource(shader, source);
            gl.compileShader(shader);
            if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
                gl.deleteShader(shader);
                return null;
            }
            return shader;
        };
        const vertexShader = compile(gl.VERTEX_SHADER, vertexSource);
        const fragmentShader = compile(gl.FRAGMENT_SHADER, fragmentSource);
        if (!vertexShader || !fragmentShader)
            return null;
        const program = gl.createProgram();
        gl.attachShader(program, vertexShader);
        gl.attachShader(program, fragmentShader);
        gl.linkProgram(program);
        if (!gl.getProgramParameter(program, gl.LINK_STATUS))
            return null;
        return program;
    }

    _buildMesh(projection)
    {
        const fov = this._fovDegrees;
        if (projection === "fisheye") {
            this._mesh = this._makeFisheye(fov || 180);
            this._feather = SpatialVideoSupport.FeatherFraction;
        } else if (projection === "wideFOV") {
            this._mesh = this._makeFisheye(fov || 150);
            this._feather = SpatialVideoSupport.FeatherFraction;
        } else if (projection === "equirect180") {
            this._mesh = this._makeSphere(Math.PI);
            this._feather = SpatialVideoSupport.FeatherFraction;
        } else {
            this._mesh = this._makeSphere(2 * Math.PI);
            this._feather = 0;
        }
    }

    _makeSphere(span, radius = 10)
    {
        const stacks = 48, slices = 96, positions = [], texCoords = [], edges = [], indices = [];
        const lonStart = -span / 2;
        const hasBoundary = span < 2 * Math.PI - 0.001;
        for (let i = 0; i <= stacks; i++) {
            const v = i / stacks, lat = Math.PI / 2 - v * Math.PI;
            for (let j = 0; j <= slices; j++) {
                const u = j / slices, lon = lonStart + u * span;
                positions.push(radius * Math.cos(lat) * Math.sin(lon), radius * Math.sin(lat), -radius * Math.cos(lat) * Math.cos(lon));
                texCoords.push(u, v);
                edges.push(hasBoundary ? 1.0 - 2.0 * Math.min(u, 1.0 - u) : 0.0);
            }
        }
        const cols = slices + 1;
        for (let i = 0; i < stacks; i++) {
            for (let j = 0; j < slices; j++) {
                const topLeft = i * cols + j, bottomLeft = topLeft + cols;
                indices.push(topLeft, topLeft + 1, bottomLeft, topLeft + 1, bottomLeft + 1, bottomLeft);
            }
        }
        return this._uploadMesh(positions, texCoords, edges, indices);
    }

    _makeFisheye(fovDegrees, radius = 10)
    {
        const rings = 48, segments = 96, positions = [], texCoords = [], edges = [], indices = [];
        const fovHalf = (fovDegrees / 2) * Math.PI / 180;
        for (let i = 0; i <= rings; i++) {
            const t = i / rings, theta = t * fovHalf;
            for (let j = 0; j <= segments; j++) {
                const phi = j / segments * 2 * Math.PI;
                positions.push(radius * Math.sin(theta) * Math.cos(phi), radius * Math.sin(theta) * Math.sin(phi), -radius * Math.cos(theta));
                const uvRadius = 0.5 * t;
                texCoords.push(0.5 + uvRadius * Math.cos(phi), 0.5 - uvRadius * Math.sin(phi));
                edges.push(t);
            }
        }
        const cols = segments + 1;
        for (let i = 0; i < rings; i++) {
            for (let j = 0; j < segments; j++) {
                const topLeft = i * cols + j, bottomLeft = topLeft + cols;
                indices.push(topLeft, topLeft + 1, bottomLeft, topLeft + 1, bottomLeft + 1, bottomLeft);
            }
        }
        return this._uploadMesh(positions, texCoords, edges, indices);
    }

    _uploadMesh(positions, texCoords, edges, indices)
    {
        const gl = this._gl;
        const position = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, position);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(positions), gl.STATIC_DRAW);
        const texCoord = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, texCoord);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(texCoords), gl.STATIC_DRAW);
        const edge = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, edge);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(edges), gl.STATIC_DRAW);
        const index = gl.createBuffer();
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, index);
        gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.STATIC_DRAW);
        return { position, texCoord, edge, index, count: indices.length };
    }

    _installInteraction(canvas)
    {
        const container = this.mediaController.container;
        this._savedContainerTouchAction = container.style.touchAction;
        container.style.touchAction = "none";

        this._onPointerDown = (event) => {
            this._dragging = true;
            this._moved = false;
            this._lastX = event.clientX;
            this._lastY = event.clientY;
        };
        this._onPointerMove = (event) => {
            if (!this._dragging)
                return;
            const dx = event.clientX - this._lastX, dy = event.clientY - this._lastY;
            if (!this._moved && Math.abs(dx) + Math.abs(dy) < SpatialVideoSupport.DragTolerance)
                return;
            this._moved = true;
            event.preventDefault();
            event.stopPropagation();
            this._yaw -= dx * SpatialVideoSupport.DragSpeed;
            this._pitch -= dy * SpatialVideoSupport.DragSpeed;
            const pitchLimit = Math.PI / 2 - 0.01;
            this._pitch = Math.max(-pitchLimit, Math.min(pitchLimit, this._pitch));
            this._lastX = event.clientX;
            this._lastY = event.clientY;
        };
        this._onPointerUp = (event) => {
            if (this._moved) {
                event.preventDefault();
                event.stopPropagation();
            }
            this._dragging = false;
        };
        this._onClick = (event) => {
            if (this._moved) {
                event.preventDefault();
                event.stopPropagation();
                this._moved = false;
            }
        };
        container.addEventListener("pointerdown", this._onPointerDown, true);
        container.addEventListener("pointermove", this._onPointerMove, true);
        container.addEventListener("pointerup", this._onPointerUp, true);
        container.addEventListener("pointercancel", this._onPointerUp, true);
        container.addEventListener("click", this._onClick, true);
    }

    _removeInteraction()
    {
        const container = this.mediaController.container;
        if (!container || !this._onPointerDown)
            return;
        container.removeEventListener("pointerdown", this._onPointerDown, true);
        container.removeEventListener("pointermove", this._onPointerMove, true);
        container.removeEventListener("pointerup", this._onPointerUp, true);
        container.removeEventListener("pointercancel", this._onPointerUp, true);
        container.removeEventListener("click", this._onClick, true);
        if (this._savedContainerTouchAction !== undefined)
            container.style.touchAction = this._savedContainerTouchAction;
    }

    _resize()
    {
        const canvas = this._canvas;
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        canvas.width = Math.max(1, Math.floor(canvas.clientWidth * dpr));
        canvas.height = Math.max(1, Math.floor(canvas.clientHeight * dpr));
    }

    _startRenderLoops()
    {
        const media = this.mediaController.media;
        this._useVideoFrameCallback = typeof media.requestVideoFrameCallback === "function";
        if (this._useVideoFrameCallback)
            this._videoFrameCallback = media.requestVideoFrameCallback(() => this._onVideoFrame());
        this._animationFrame = requestAnimationFrame(() => this._drawLoop());
    }

    _onVideoFrame()
    {
        if (!this._active)
            return;
        this._uploadFrame();
        this._videoFrameCallback = this.mediaController.media.requestVideoFrameCallback(() => this._onVideoFrame());
    }

    _uploadFrame()
    {
        const gl = this._gl, media = this.mediaController.media;
        if (media.readyState < media.HAVE_CURRENT_DATA)
            return true;
        gl.bindTexture(gl.TEXTURE_2D, this._texture);
        try {
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, media);
        } catch {
            this._teardown();
            return false;
        }
        gl.generateMipmap(gl.TEXTURE_2D);
        return true;
    }

    _drawLoop()
    {
        if (!this._active)
            return;
        const gl = this._gl;

        if (!this._useVideoFrameCallback && !this._uploadFrame())
            return;

        if (canvasSizeChanged(this._canvas))
            this._resize();

        gl.viewport(0, 0, this._canvas.width, this._canvas.height);
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.enable(gl.DEPTH_TEST);

        const projection = perspective(SpatialVideoSupport.CameraFOV, this._canvas.width / this._canvas.height, 0.05, 100);
        const view = mul4(rotX(this._pitch), rotY(this._yaw));
        gl.uniformMatrix4fv(this._mvpLocation, false, new Float32Array(mul4(projection, view)));
        gl.uniform1f(this._featherLocation, this._feather);

        const mesh = this._mesh;
        gl.bindBuffer(gl.ARRAY_BUFFER, mesh.position);
        gl.enableVertexAttribArray(this._positionLocation);
        gl.vertexAttribPointer(this._positionLocation, 3, gl.FLOAT, false, 0, 0);
        gl.bindBuffer(gl.ARRAY_BUFFER, mesh.texCoord);
        gl.enableVertexAttribArray(this._texCoordLocation);
        gl.vertexAttribPointer(this._texCoordLocation, 2, gl.FLOAT, false, 0, 0);
        if (this._edgeDistanceLocation >= 0) {
            gl.bindBuffer(gl.ARRAY_BUFFER, mesh.edge);
            gl.enableVertexAttribArray(this._edgeDistanceLocation);
            gl.vertexAttribPointer(this._edgeDistanceLocation, 1, gl.FLOAT, false, 0, 0);
        }
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, mesh.index);
        gl.drawElements(gl.TRIANGLES, mesh.count, gl.UNSIGNED_SHORT, 0);

        this._animationFrame = requestAnimationFrame(() => this._drawLoop());
    }

    _teardown()
    {
        this._active = false;
        if (this._animationFrame)
            cancelAnimationFrame(this._animationFrame);
        const media = this.mediaController.media;
        if (this._videoFrameCallback && typeof media.cancelVideoFrameCallback === "function")
            media.cancelVideoFrameCallback(this._videoFrameCallback);
        this._removeInteraction();
        if (this._canvas) {
            this._canvas.remove();
            this._canvas = null;
        }
        if (media)
            media.style.visibility = "";
    }
}

SpatialVideoSupport.FeatherFraction = 0.12;
SpatialVideoSupport.CameraFOV = 80;
SpatialVideoSupport.DragTolerance = 3;
SpatialVideoSupport.DragSpeed = 0.005;
const ProjectionAttributeValues = {
    "equirectangular": "equirect360",
    "360": "equirect360",
    "halfequirectangular": "equirect180",
    "180": "equirect180",
    "parametric": "wideFOV",
    "wfov": "wideFOV",
    "fisheye": "fisheye",
};

function canvasSizeChanged(canvas)
{
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    return canvas.width !== Math.floor(canvas.clientWidth * dpr) || canvas.height !== Math.floor(canvas.clientHeight * dpr);
}

function perspective(fovDegrees, aspect, near, far)
{
    const f = 1 / Math.tan(fovDegrees * Math.PI / 360), nf = 1 / (near - far);
    return [f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, (far + near) * nf, -1, 0, 0, 2 * far * near * nf, 0];
}

function mul4(a, b)
{
    const o = new Array(16);
    for (let r = 0; r < 4; r++) {
        for (let c = 0; c < 4; c++)
            o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] + a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
    }
    return o;
}

function rotY(a) { const c = Math.cos(a), s = Math.sin(a); return [c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1]; }
function rotX(a) { const c = Math.cos(a), s = Math.sin(a); return [1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1]; }

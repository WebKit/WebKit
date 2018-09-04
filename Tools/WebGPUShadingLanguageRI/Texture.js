/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"use strict";

class Texture {
    constructor(dimension, width, height, depth, levelCount, layerCount, innerType, data)
    {
        this._dimension = dimension;
        this._width = width;
        this._height = height;
        this._depth = depth;
        this._levelCount = levelCount;
        this._layerCount = layerCount;
        this._innerType = innerType;
        this._data = data;
    }

    get dimension() { return this._dimension; }
    get width() { return this._width; }
    get height() { return this._height; }
    get depth() { return this._depth; }
    get levelCount() { return this._levelCount; }
    get layerCount() { return this._layerCount; }
    get innerType() { return this._innerType; }
    get data() { return this._data; }

    elementChecked(layer, level, k, j, i)
    {
        if (layer < 0 || layer >= this.layerCount
            || level < 0 || level >= this.levelCount
            || k < 0 || k >= this.depth
            || j < 0 || j >= this.height
            || i < 0 || i >= this.width)
            return undefined;
        return this.element(layer, level, k, j, i);
    }

    setElementChecked(layer, level, k, j, i, value)
    {
        if (layer < 0 || layer >= this.layerCount
            || level < 0 || level >= this.levelCount
            || k < 0 || k >= this.depth
            || j < 0 || j >= this.height
            || i < 0 || i >= this.width)
            return false;
        this.setElement(layer, level, k, j, i, value);
        return true;
    }
}

class Texture1D extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of mipmaps.
        // The first mipmap is an array of elements.
        // Each element may be a scalar or an array of 2-4 scalars.
        // The width must be a power-of-two.
        // Each mipmap is half-size of the previous size.

        let dimension = 1;
        let width = data[0].length;
        let height = 1;
        let depth = 1;
        let levelCount = data.length;
        let layerCount = 1;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[level].length;
    }

    heightAtLevel(level)
    {
        return 1;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[level][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[level][i] = value;
    }
}

class Texture1DArray extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of 1D textures.
        // All textures must have the same width and number of mipmaps.

        let dimension = 1;
        let width = data[0][0].length;
        let height = 1;
        let depth = 1;
        let levelCount = data[0].length;
        let layerCount = data.length;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0][level].length;
    }

    heightAtLevel(level)
    {
        return 1;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[layer][level][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[layer][level][i] = value;
    }
}

class Texture2D extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of mipmaps.
        // The first mipmap is a rectangular array of rows, where each row is an array of elements.
        // Each element may be a scalar or an array of 2-4 scalars.
        // The width and height must be powers-of-two.
        // Each mipmap is half-width and half-height of the previous size.

        let dimension = 2;
        let width = data[0][0].length;
        let height = data[0].length;
        let depth = 1;
        let levelCount = data.length;
        let layerCount = 1;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[level][0].length;
    }

    heightAtLevel(level)
    {
        return this.data[level].length;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[level][j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[level][j][i] = value;
    }
}

let TextureDepth2D = Texture2D;

class Texture2DArray extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of 2D textures.
        // All textures must have the same width, height and number of mipmaps.

        let dimension = 2;
        let width = data[0][0][0].length;
        let height = data[0][0].length;
        let depth = 1;
        let levelCount = data[0].length;
        let layerCount = data.length;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0][level][0].length;
    }

    heightAtLevel(level)
    {
        return this.data[0][level].length;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[layer][level][j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[layer][level][j][i] = value;
    }
}

let TextureDepth2DArray = Texture2DArray;

class Texture3D extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of mipmaps.
        // The first mipmap is an array of depth slices, each depth slice is an array of rows, and each row is an array of elements.
        // Each element may be a scalar or an array of 2-4 scalars.
        // The width, height, and depth must be powers-of-two.
        // Each mipmap is half-width, half-height, and half-depth of the previous size.

        let dimension = 3;
        let width = data[0][0][0].length;
        let height = data[0][0].length;
        let depth = data[0].length;
        let levelCount = data.length;
        let layerCount = 1;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[level][0][0].length;
    }

    heightAtLevel(level)
    {
        return this.data[level][0].length;
    }

    depthAtLevel(level)
    {
        return this.data[level].length;
    }

    element(layer, level, k, j, i)
    {
        return this.data[level][k][j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[level][k][j][i] = value;
    }
}

class TextureCube extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of 6 2D textures.
        // All textures must have the same width, height, depth, and number of mipmaps.

        let dimension = 2;
        let width = data[0][0][0].length;
        let height = data[0][0].length;
        let depth = 1;
        let levelCount = data[0].length;
        let layerCount = data.length;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0][level][0].length;
    }

    heightAtLevel(level)
    {
        return this.data[0][level].length;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[layer][level][j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[layer][level][j][i] = value;
    }
}

let TextureDepthCube = TextureCube;

class Texture1DRW extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of elements.
        // Each element may be a scalar or an array of 2-4 scalars.
        // The width must be a power-of-two.

        let dimension = 1;
        let width = data.length;
        let height = 1;
        let depth = 1;
        let levelCount = 1;
        let layerCount = 1;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data.length;
    }

    heightAtLevel(level)
    {
        return 1;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[i] = value;
    }
}

class Texture1DArrayRW extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of 1D RW textures.
        // All textures must have the same size.

        let dimension = 1;
        let width = data[0].length;
        let height = 1;
        let depth = 1;
        let levelCount = 1;
        let layerCount = data.length;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0].length;
    }

    heightAtLevel(level)
    {
        return 1;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[layer][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[layer][i] = value;
    }
}

class Texture2DRW extends Texture {
    constructor(innerType, data)
    {
        // "data" is a rectangular array of rows, where each row is an array of elements.
        // Each element may be a scalar or an array of 2-4 scalars.
        // The width and height must be powers-of-two.

        let dimension = 2;
        let width = data[0].length;
        let height = data.length;
        let depth = 1;
        let levelCount = 1;
        let layerCount = 1;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0].length;
    }

    heightAtLevel(level)
    {
        return this.data.length;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[j][i] = value;
    }
}

let TextureDepth2DRW = Texture2DRW;

class Texture2DArrayRW extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of 2D RW textures.
        // All textures must have the same width and height.

        let dimension = 2;
        let width = data[0][0].length;
        let height = data[0].length;
        let depth = 1;
        let levelCount = 1;
        let layerCount = data.length;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0][0].length;
    }

    heightAtLevel(level)
    {
        return this.data[0].length;
    }

    depthAtLevel(level)
    {
        return 1;
    }

    element(layer, level, k, j, i)
    {
        return this.data[layer][j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[layer][j][i] = value;
    }
}

let TextureDepth2DArrayRW = Texture2DArrayRW

class Texture3DRW extends Texture {
    constructor(innerType, data)
    {
        // "data" is an array of depth slices, each depth slice is an array of rows, and each row is an array of elements.
        // Each element may be a scalar or an array of 2-4 scalars.
        // The width, height, and depth must be powers-of-two.

        let dimension = 3;
        let width = data[0][0].length;
        let height = data[0].length;
        let depth = data.length;
        let levelCount = 1;
        let layerCount = 1;
        super(dimension, width, height, depth, levelCount, layerCount, innerType, data);
    }

    widthAtLevel(level)
    {
        return this.data[0][0].length;
    }

    heightAtLevel(level)
    {
        return this.data[0].length;
    }

    depthAtLevel(level)
    {
        return this.data.length;
    }

    element(layer, level, k, j, i)
    {
        return this.data[k][j][i];
    }

    setElement(layer, level, k, j, i, value)
    {
        this.data[k][j][i] = value;
    }
}

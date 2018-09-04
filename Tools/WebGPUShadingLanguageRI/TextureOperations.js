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

function depthCompareOperation(dref, d, compareFunction)
{
    // Vulkan 1.1.83 section 15.3.4
    switch (compareFunction) {
        case "never":
            return 0;
        case "less":
            return dref < d ? 1 : 0;
        case "equal":
            return dref == d ? 1 : 0;
        case "lessEqual":
            return dref <= d ? 1 : 0;
        case "greater":
            return dref > d ? 1 : 0;
        case "notEqual":
            return dref != d ? 1 : 0;
        case "greaterEqual":
            return dref >= d ? 1 : 0;
        case "always":
            return 1;
        default:
            throw new Error("Unknown depth comparison function");
    }
}

function conversionToRGBA(value)
{
    // Vulkan 1.1.83 section 15.3.5
    if (value instanceof Array) {
        let result = [];
        for (let i = 0; i < value.length; ++i)
            result.push(value[i]);
        while (result.length != 4)
            result.push(result.length == 3 ? 1 : 0);
        return result;
    } else
        return [value, 0, 0, 1];
}

function projectionOperation(s, t, r, dref, q)
{
    // Vulkan 1.1.83 section 15.6.1
    return [s / q, t / q, r / q, dref != undefined ? dref / q : undefined];
}

function cubeMapFaceSelection(s, t, r, partialRxWithRespectToX, partialRxWithRespectToY, partialRyWithRespectToX, partialRyWithRespectToY, partialRzWithRespectToX, partialRzWithRespectToY)
{
    // Vulkan 1.1.83 section 15.6.4
    let rx = s;
    let ry = t;
    let rz = r;
    let winner = 0;
    if (Math.abs(rz) >= Math.abs(ry) && Math.abs(rz) >= Math.abs(rx))
        winner = 2;
    else if (Math.abs(ry) >= Math.abs(rx) && Math.abs(ry) >= Math.abs(rz))
        winner = 1;

    let layerNumber;
    let sc;
    let tc;
    let rc;
    let partialScWithRespectToX;
    let partialScWithRespectToY;
    let partialTcWithRespectToX;
    let partialTcWithRespectToY;
    let partialRcWithRespectToX;
    let partialRcWithRespectToY;
    if (winner == 0 && rx >= 0) {
        layerNumber = 0;
        sc = -rz;
        tc = -ry;
        rc = rx;
        partialScWithRespectToX = -partialRzWithRespectToX;
        partialScWithRespectToY = -partialRzWithRespectToY;
        partialTcWithRespectToX = -partialRyWithRespectToX;
        partialTcWithRespectToY = -partialRyWithRespectToY;
        partialRcWithRespectToX = partialRxWithRespectToX;
        partialRcWithRespectToY = partialRxWithRespectToY;
    } else if (winner == 0) {
        layerNumber = 1;
        sc = rz;
        tc = -ry;
        rc = rx;
        partialScWithRespectToX = partialRzWithRespectToX;
        partialScWithRespectToY = partialRzWithRespectToY;
        partialTcWithRespectToX = -partialRyWithRespectToX;
        partialTcWithRespectToY = -partialRyWithRespectToY;
        partialRcWithRespectToX = -partialRxWithRespectToX;
        partialRcWithRespectToY = -partialRxWithRespectToY;
    } else if (winner == 1 && ry >= 0) {
        layerNumber = 2;
        sc = rx;
        tc = -rz;
        rc = ry;
        partialScWithRespectToX = partialRxWithRespectToX;
        partialScWithRespectToY = partialRxWithRespectToY;
        partialTcWithRespectToX = partialRzWithRespectToX;
        partialTcWithRespectToY = partialRzWithRespectToY;
        partialRcWithRespectToX = partialRyWithRespectToX;
        partialRcWithRespectToY = partialRyWithRespectToY;
    } else if (winner == 1) {
        layerNumber = 3;
        sc = rx;
        tc = -rz;
        rc = ry;
        partialScWithRespectToX = partialRxWithRespectToX;
        partialScWithRespectToY = partialRxWithRespectToY;
        partialTcWithRespectToX = -partialRzWithRespectToX;
        partialTcWithRespectToY = -partialRzWithRespectToY;
        partialRcWithRespectToX = -partialRyWithRespectToX;
        partialRcWithRespectToY = -partialRyWithRespectToY;
    } else if (winner == 2 && rz >= 0) {
        layerNumber = 4;
        sc = rx;
        tc = -ry;
        rc = rz;
        partialScWithRespectToX = partialRxWithRespectToX;
        partialScWithRespectToY = partialRxWithRespectToY;
        partialTcWithRespectToX = -partialRyWithRespectToX;
        partialTcWithRespectToY = -partialRyWithRespectToY;
        partialRcWithRespectToX = partialRzWithRespectToX;
        partialRcWithRespectToY = partialRzWithRespectToY;
    } else {
        layerNumber = 5;
        sc = -rx;
        tc = -ry;
        rc = rz;
        partialScWithRespectToX = -partialRxWithRespectToX;
        partialScWithRespectToY = -partialRxWithRespectToY;
        partialTcWithRespectToX = -partialRyWithRespectToX;
        partialTcWithRespectToY = -partialRyWithRespectToY;
        partialRcWithRespectToX = -partialRzWithRespectToX;
        partialRcWithRespectToY = -partialRzWithRespectToY;
    }
    return [layerNumber, sc, tc, rc, partialScWithRespectToX, partialScWithRespectToY, partialTcWithRespectToX, partialTcWithRespectToY, partialRcWithRespectToX, partialRcWithRespectToY]
}

function cubeMapCoordinateTransformation(sc, tc, rc)
{
    // Vulkan 1.1.83 section 15.6.5
    let sFace = (1 / 2) * sc / Math.abs(rc) + (1 / 2);
    let tFace = (1 / 2) * tc / Math.abs(rc) + (1 / 2);
    return [sFace, tFace];
}

function cubeMapDerivativeTransformation(sc, tc, rc, partialScWithRespectToX, partialScWithRespectToY, partialTcWithRespectToX, partialTcWithRespectToY, partialRcWithRespectToX, partialRcWithRespectToY)
{
    // Vulkan 1.1.83 section 15.6.6
    let partialSFaceWithRespectToX = (1 / 2) * ((Math.abs(rc) * partialScWithRespectToX - sc * partialRcWithRespectToX) / Math.pow(rc, 2));
    let partialSFaceWithRespectToY = (1 / 2) * ((Math.abs(rc) * partialScWithRespectToY - sc * partialRcWithRespectToY) / Math.pow(rc, 2));
    let partialTFaceWithRespectToX = (1 / 2) * ((Math.abs(rc) * partialTcWithRespectToX - tc * partialRcWithRespectToX) / Math.pow(rc, 2));
    let partialTFaceWithRespectToY = (1 / 2) * ((Math.abs(rc) * partialTcWithRespectToY - tc * partialRcWithRespectToY) / Math.pow(rc, 2));
    return [partialSFaceWithRespectToX, partialSFaceWithRespectToY, partialTFaceWithRespectToX, partialTFaceWithRespectToY];
}

function scaleFactorOperation(deviceMaxAnisotropy, width, height, depth, samplerMaxAnisotropy, partialSWithRespectToX, partialTWithRespectToX, partialRWithRespectToX, partialSWithRespectToY, partialTWithRespectToY, partialRWithRespectToY)
{
    // Vulkan 1.1.83 section 15.6.7
    let mux = Math.abs(partialSWithRespectToX) * width;
    let mvx = Math.abs(partialTWithRespectToX) * height;
    let mwx = Math.abs(partialRWithRespectToX) * depth;
    let muy = Math.abs(partialSWithRespectToY) * width;
    let mvy = Math.abs(partialTWithRespectToY) * height;
    let mwy = Math.abs(partialRWithRespectToY) * depth;

    // I made this up. The formula isn't listed in the spec.
    let fx = Math.sqrt(mux * mux + mvx * mvx + mwx * mwx);
    let fy = Math.sqrt(muy * muy + mvy * mvy + mwy * mwy);

    let rhoX = fx;
    let rhoY = fy;
    let rhoMax = Math.max(rhoX, rhoY);
    let rhoMin = Math.min(rhoX, rhoY);

    let maxAniso = Math.min(samplerMaxAnisotropy, deviceMaxAnisotropy);

    let eta;
    if (rhoMax == 0 && rhoMin == 0)
        eta = 1;
    else if (rhoMin == 0)
        eta = maxAniso;
    else
        eta = Math.min(rhoMax / rhoMin, maxAniso);

    let N = Math.ceil(eta);
    return [rhoMax, rhoMin, eta, N];
}

function levelOfDetailOperation(deviceMaxSamplerLodBias, lodFromFunctionOperand, lodMinFromFunctionOperand, biasFromFunctionOperand, samplerBias, samplerLodMin, samplerLodMax, rhoMax, eta)
{
    // Vulkan 1.1.83 section 15.6.7
    if (biasFromFunctionOperand == undefined)
        biasFromFunctionOperand = 0;

    let lambdaBase = lodFromFunctionOperand != undefined ? lodFromFunctionOperand : Math.log2(rhoMax / eta);
    let lambdaPrime = lambdaBase + Math.min(Math.max(samplerBias + biasFromFunctionOperand, -deviceMaxSamplerLodBias), deviceMaxSamplerLodBias);
    let lodMin = Math.max(samplerLodMin, lodMinFromFunctionOperand);
    let lodMax = samplerLodMax;
    let lambda;
    if (lambdaPrime > lodMax)
        lambda = lodMax;
    else if (lodMin <= lambdaPrime && lambdaPrime <= lodMax)
        lambda = lambdaPrime;
    else if (lambdaPrime < lodMin)
        lambda = lodMin;
    else
        throw new Error("lodMin >= lodMax");
    return lambda;
}

function imageLevelSelection(baseMipLevel, levelCount, mipmapMode, lambda)
{
    // Vulkan 1.1.83 section 15.6.7
    function nearest(dPrime)
    {
        return Math.ceil(dPrime + 0.5) - 1;
    }

    let q = levelCount - 1;
    let dPrime = baseMipLevel + Math.min(Math.max(lambda, 0), q);
    let dl;
    if (mipmapMode == "nearest")
        dl = nearest(dPrime);
    else
        dl = dPrime;
    if (mipmapMode == "nearest")
        return dl;
    else {
        let dHi = Math.floor(dl);
        let dLow = Math.min(dHi + 1, q);
        let delta = dl - dHi;
        return [dHi, dLow, delta];
    }
}

function strqaToUVWATransformation(s, t, r, width, height, depth)
{
    // Vulkan 1.1.83 section 15.6.8
    let u = s * width;
    let v = t * height;
    let w = r * depth;
    return [u, v, w];
}

function uvwaToIJKLNTransformationAndArrayLayerSelection(layerCount, baseArrayLayer, filter, u, v, w, a, deltaI, deltaJ, deltaK)
{
    // Vulkan 1.1.83 section 15.7.1

    if (deltaI == undefined)
        deltaI = 0;
    if (deltaJ == undefined)
        deltaJ = 0;
    if (deltaK == undefined)
        deltaK = 0;

    function rne(a)
    {
        return Math.floor(a + 0.5);
    }
    let l = Math.min(Math.max(rne(a), 0), layerCount - 1) + baseArrayLayer;
    let i;
    let j;
    let k;
    if (filter == "nearest") {
        i = Math.floor(u);
        j = Math.floor(v);
        k = Math.floor(w);
        
        i += deltaI;
        j += deltaJ;
        k += deltaK;
        
        return [i, j, k, l];
    } else {
        if (filter != "linear")
            throw new Error("Unknown filter");
        let i0 = Math.floor(u - 0.5);
        let i1 = i0 + 1;
        let j0 = Math.floor(v - 0.5);
        let j1 = j0 + 1;
        let k0 = Math.floor(w - 0.5);
        let k1 = k0 + 1;
        let alpha = (u - 0.5) - i0;
        let beta = (v - 0.5) - j0;
        let gamma = (w - 0.5) - k0;
        
        i0 += deltaI;
        i1 += deltaI;
        j0 += deltaJ;
        j1 += deltaJ;
        k0 += deltaK;
        k1 += deltaK;
        
        return [i0, i1, j0, j1, k0, k1, l, alpha, beta, gamma];
    }
}

function integerTexelCoordinateOperations(baseMipLevel, levelCount, levelBase, lodFromFunctionOperand)
{
    // Vulkan 1.1.83 section 15.8
    let d = levelBase + lodFromFunctionOperand;
    if (d < baseMipLevel || d >= baseMipLevel + levelCount)
        throw new Error("Selected level does not lie inside the image");
    return d;
}

function wrappingOperation(width, height, depth, addressModeU, addressModeV, addressModeW, i, j, k)
{
    // Vulkan 1.1.83 section 15.9.1
    // FIXME: "Cube images ignore the wrap modes specified in the sampler. Instead, if VK_FILTER_NEAREST is used within a mip level then VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE is used, and if VK_FILTER_LINEAR is used within a mip level then sampling at the edges is performed as described earlier in the Cube map edge handling section."
    function transform(addressMode, size, i)
    {
        function mirror(n) {
            if (n >= 0)
                return n;
            else
                return -(1 + n);
        }
        switch (addressMode) {
            case "clampToEdge":
                return Math.min(Math.max(i, 0), size - 1);
            case "repeat":
                return i % size;
            case "mirrorRepeat":
                return (size - 1) - mirror((i % (2 * size)) - size);
            case "clampToBorderColor":
                return Math.min(Math.max(i, -1), size);
            default:
                throw new Error("Unknown address mode");
        }
    }
    return [transform(addressModeU, width, i), transform(addressModeV, height, j), transform(addressModeW, depth, k)];
}

function calculateLambda(deviceMaxAnisotropy, deviceMaxSamplerLodBias, texture, sampler, samplerBias, s, t, r, a, lodFromFunctionOperand, lodMinFromFunctionOperand, biasFromFunctionOperand, ddx, ddy)
{
    let partialSWithRespectToX = 0;
    let partialTWithRespectToX = 0;
    let partialRWithRespectToX = 0;
    if (ddx != undefined) {
        partialSWithRespectToX = ddx[0];
        partialTWithRespectToX = ddx[1];
        partialRWithRespectToX = ddx[2];
    }
    let partialSWithRespectToY = 0;
    let partialTWithRespectToY = 0;
    let partialRWithRespectToY = 0;
    if (ddy != undefined) {
        partialSWithRespectToY = ddy[0];
        partialTWithRespectToY = ddy[1];
        partialRWithRespectToY = ddy[2];
    }

    if (texture instanceof TextureCube) {
        // FIXME: Handle cubemap edge/corners correctly
        let [layerNumber, sc, tc, rc, partialScWithRespectToX, partialScWithRespectToY, partialTcWithRespectToX, partialTcWithRespectToY, partialRcWithRespectToX, partialRcWithRespectToY] = cubeMapFaceSelection(s, t, r, partialSWithRespectToX, partialSWithRespectToY, partialTWithRespectToX, partialTWithRespectToY, partialRWithRespectToX, partialRWithRespectToY)
        let [sFace, tFace] = cubeMapCoordinateTransformation(sc, tc, rc)
        let [partialSFaceWithRespectToX, partialSFaceWithRespectToY, partialTFaceWithRespectToX, partialTFaceWithRespectToY] = cubeMapDerivativeTransformation(sc, tc, rc, partialScWithRespectToX, partialScWithRespectToY, partialTcWithRespectToX, partialTcWithRespectToY, partialRcWithRespectToX, partialRcWithRespectToY)
        s = sFace;
        t = tFace;
        r = 0;
        a = layerNumber;
        partialSWithRespectToX = partialSFaceWithRespectToX;
        partialSWithRespectToY = partialSFaceWithRespectToY;
        partialTWithRespectToX = partialTFaceWithRespectToX;
        partialTWithRespectToY = partialTFaceWithRespectToY;
        partialRWithRespectToX = 0;
        partialRWithRespectToY = 0;
    }

    let [rhoMax, rhoMin, eta, N] = scaleFactorOperation(deviceMaxAnisotropy, texture.width, texture.height, texture.depth, sampler.maxAnisotropy, partialSWithRespectToX, partialTWithRespectToX, partialRWithRespectToX, partialSWithRespectToY, partialTWithRespectToY, partialRWithRespectToY);
    let lambda = levelOfDetailOperation(deviceMaxSamplerLodBias, lodFromFunctionOperand, lodMinFromFunctionOperand, biasFromFunctionOperand, samplerBias, sampler.lodMinClamp, sampler.lodMaxClamp, rhoMax, eta);
    return [s, t, r, a, lambda];
}

function computeTau(level, baseArrayLayer, texture, sampler, addressModeU, addressModeV, addressModeW, filter, s, t, r, a, deltaI, deltaJ, deltaK)
{
    function accessColor(width, height, depth, l, i, j, k)
    {
        function shouldBeBorder(value, max)
        {
            if (value < -1 || value > max)
                throw new Error("Out-of-bounds texture read");
            return value == -1 || value == max;
        }

        let [wrappedI, wrappedJ, wrappedK] = wrappingOperation(width, height, depth, addressModeU, addressModeV, addressModeW, i, j, k)
        if (shouldBeBorder(wrappedI, width) || shouldBeBorder(wrappedJ, height) || shouldBeBorder(wrappedK, depth))
            return sampler.calculateBorderColor(texture.innerType);
        else
            return texture.element(l, level, wrappedK, wrappedJ, wrappedI);
    }

    let [u, v, w] = strqaToUVWATransformation(s, t, r, texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level));
    if (filter == "nearest") {
        let [i, j, k, l] = uvwaToIJKLNTransformationAndArrayLayerSelection(texture.layerCount, baseArrayLayer, filter, u, v, w, a, deltaI, deltaJ, deltaK);
        return accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i, j, k);
    } else {
        if (filter != "linear")
            throw new Error("Unknown filter");
        let [i0, i1, j0, j1, k0, k1, l, alpha, beta, gamma] = uvwaToIJKLNTransformationAndArrayLayerSelection(texture.layerCount, baseArrayLayer, filter, u, v, w, a, deltaI, deltaJ, deltaK);
        let color000 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i0, j0, k0);
        let color100 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i1, j0, k0);
        let color010 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i0, j1, k0);
        let color110 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i1, j1, k0);
        let color001 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i0, j0, k1);
        let color101 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i1, j0, k1);
        let color011 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i0, j1, k1);
        let color111 = accessColor(texture.widthAtLevel(level), texture.heightAtLevel(level), texture.depthAtLevel(level), l, i1, j1, k1);
        return [color000, color100, color010, color110, color001, color101, color011, color111, alpha, beta, gamma];
    }
}

function reduce(...values)
{
    if (values.length % 2 != 0)
        throw new Error("Don't have a weight corresponding to each value");
    if (values.length < 2)
        throw new Error("Don't have enough things to calculate an average");
    let sum;
    if (values[1] instanceof Array) {
        sum = [];
        for (let i = 0; i < values[1].length; ++i)
            sum.push(0);
    } else
        sum = 0;
    for (let i = 0; i < values.length / 2; ++i) {
        let weight = values[2 * i];
        let color = values[2 * i + 1];
        if (color instanceof Array) {
            for (let i = 0; i < color.length; ++i)
                sum[i] += weight * color[i];
        } else
            sum += weight * color;
    }
    return sum;
}

function texelGathering(
    deviceMaxAnisotropy,
    deviceMaxSamplerLodBias,
    baseMipLevel,
    baseArrayLayer,
    texture,
    sampler,
    samplerBias,
    s, t, r, a,
    deltaI, deltaJ, deltaK,
    lodFromFunctionOperand,
    lodMinFromFunctionOperand,
    biasFromFunctionOperand,
    ddx, ddy,
    channel)
{
    // Vulkan 1.1.83 section 15.9.2

    // The spec uses different names than WebGPU does.
    let addressModeU = sampler.rAddressMode;
    let addressModeV = sampler.sAddressMode;
    let addressModeW = sampler.tAddressMode;

    let lambda;
    [s, t, r, a, lambda] = calculateLambda(deviceMaxAnisotropy, deviceMaxSamplerLodBias, texture, sampler, samplerBias, s, t, r, a, lodFromFunctionOperand, lodMinFromFunctionOperand, biasFromFunctionOperand, ddx, ddy);

    let filter = "linear";

    let imageLevel = imageLevelSelection(baseMipLevel, texture.levelCount, "nearest", lambda);

    let [color000, color100, color010, color110, color001, color101, color011, color111, alpha, beta, gamma] = computeTau(imageLevel, baseArrayLayer, texture, sampler, addressModeU, addressModeV, addressModeW, filter, s, t, r, a, deltaI, deltaJ, deltaK);

    if (texture.dimension != 2)
        throw new Error("Cannot gather from a non-2D image");

    let color00 = color001;
    let color10 = color101;
    let color01 = color011;
    let color11 = color111;

    switch (channel) {
        case 0:
            if (color00 instanceof Array)
                return [color01[0], color11[0], color10[0], color00[0]];
            else
                return [color01, color11, color10, color00];
        case 1:
            return [color01[1], color11[1], color10[1], color00[1]];
        case 2:
            return [color01[2], color11[2], color10[2], color00[2]];
        case 3:
            return [color01[3], color11[3], color10[3], color00[3]];
        default:
            throw new Error("Unknown channel to gather from");
    }
}

function texelFiltering(
    deviceMaxAnisotropy,
    deviceMaxSamplerLodBias,
    baseMipLevel,
    baseArrayLayer,
    texture,
    sampler,
    samplerBias,
    s, t, r, a,
    deltaI, deltaJ, deltaK,
    lodFromFunctionOperand,
    lodMinFromFunctionOperand,
    biasFromFunctionOperand,
    ddx, ddy)
{
    // Vulkan 1.1.83 section 15.9.3

    // The spec uses different names than WebGPU does.
    let addressModeU = sampler.rAddressMode;
    let addressModeV = sampler.sAddressMode;
    let addressModeW = sampler.tAddressMode;

    let lambda;
    [s, t, r, a, lambda] = calculateLambda(deviceMaxAnisotropy, deviceMaxSamplerLodBias, texture, sampler, samplerBias, s, t, r, a, lodFromFunctionOperand, lodMinFromFunctionOperand, biasFromFunctionOperand, ddx, ddy);

    let filter;
    if (lambda <= 0)
        filter = sampler.magFilter;
    else
        filter = sampler.minFilter;

    function computeColorFromLevel(level)
    {
        if (filter == "nearest") {
            return computeTau(level, baseArrayLayer, texture, sampler, addressModeU, addressModeV, addressModeW, filter, s, t, r, a, deltaI, deltaJ, deltaK);
        } else {
            if (filter != "linear")
                throw new Error("Unknown filter");
            let [color000, color100, color010, color110, color001, color101, color011, color111, alpha, beta, gamma] = computeTau(level, baseArrayLayer, texture, sampler, addressModeU, addressModeV, addressModeW, filter, s, t, r, a, deltaI, deltaJ, deltaK);
            switch (texture.dimension) {
                case 1:
                    let color0 = color011;
                    let color1 = color111;
                    return reduce(1 - alpha, color0, alpha, color1);
                case 2:
                    let color00 = color001;
                    let color10 = color101;
                    let color01 = color011;
                    let color11 = color111;
                    return reduce((1 - alpha) * (1 - beta), color00, alpha * (1 - beta), color10, (1 - alpha) * beta, color01, alpha * beta, color11);
                case 3:
                    return reduce(
                        (1 - alpha) * (1 - beta) * (1 - gamma), color000,
                        alpha * (1 - beta) * (1 - gamma), color100,
                        (1 - alpha) * beta * (1 - gamma), color010,
                        alpha * beta * (1 - gamma), color110,
                        (1 - alpha) * (1 - beta) * gamma, color001,
                        alpha * (1 - beta) * gamma, color101,
                        (1 - alpha) * beta * gamma, color011,
                        alpha * beta * gamma, color111);
                default:
                    throw new Error("Unknown dimension");
            }
        }
    }

    if (sampler.mipmapFilter == "nearest") {
        let imageLevel = imageLevelSelection(baseMipLevel, texture.levelCount, sampler.mipmapFilter, lambda);
        return computeColorFromLevel(imageLevel);
    } else {
        if (sampler.mipmapFilter != "linear")
            throw new Error("Unknown filter");
        let [dHi, dLow, delta] = imageLevelSelection(baseMipLevel, texture.levelCount, sampler.mipmapFilter, lambda);
        return reduce(1 - delta, computeColorFromLevel(dHi), delta, computeColorFromLevel(dLow));
    }
}

function castToInnerTypeForGather(value, innerType)
{
    switch (innerType) {
        case "uchar":
        case "uchar2":
        case "uchar3":
        case "uchar4":
            return value.map(castToUchar);
        case "ushort":
        case "ushort2":
        case "ushort3":
        case "ushort4":
            return value.map(castToUshort);
        case "uint":
        case "uint2":
        case "uint3":
        case "uint4":
            return value.map(castToUint);
        case "char":
        case "char2":
        case "char3":
        case "char4":
            return value.map(castToChar);
        case "short":
        case "short2":
        case "short3":
        case "short4":
            return value.map(castToShort);
        case "int":
        case "int2":
        case "int3":
        case "int4":
            return value.map(castToInt);
        case "half":
        case "half2":
        case "half3":
        case "half4":
            return value.map(castToHalf);
        case "float":
        case "float2":
        case "float3":
        case "float4":
            return value.map(castToFloat);
        default:
            throw new Error("Unknown texture inner type");
    }
}

function gatherTexture(texture, sampler, location, delta, biasFromFunctionOperand, ddx, ddy, lodFromFunctionOperand, dref, channel)
{
    let s = location[0];
    let t = location[1];
    let r = location[2];
    let q = location[3];
    let a = location[4];

    let deltaI = undefined;
    let deltaJ = undefined;
    let deltaK = undefined;
    if (delta != undefined) {
        deltaI = delta[0];
        deltaJ = delta[1];
        deltaK = delta[2];
    }

    [s, t, r, dref] = projectionOperation(s, t, r, dref, q);
    let result = texelGathering(
        Number.MAX_VALUE,
        Number.MAX_VALUE,
        0,
        0,
        texture,
        sampler,
        0,
        s, t, r, a,
        deltaI, deltaJ, deltaK,
        lodFromFunctionOperand,
        -Number.MAX_VALUE,
        biasFromFunctionOperand,
        ddx, ddy,
        channel);
    if (dref != undefined)
        result = result.map(v => depthCompareOperation(dref, v, sampler.compareFunction));
    return castToInnerTypeForGather(result, texture.innerType);
    // FIXME: Figure out what to do with anisotropy
    // FIXME: Figure out how to do multisampling.
}

function castToInnerType(value, innerType)
{
    switch (innerType) {
        case "uchar":
            return castToUchar(value);
        case "ushort":
            return castToUshort(value);
        case "uint":
            return castToUint(value);
        case "char":
            return castToChar(value);
        case "short":
            return castToShort(value);
        case "int":
            return castToInt(value);
        case "half":
            return castToHalf(value);
        case "float":
            return castToFloat(value);
        
        case "uchar2":
        case "uchar3":
        case "uchar4":
            return value.map(castToUchar);
        case "ushort2":
        case "ushort3":
        case "ushort4":
            return value.map(castToUshort);
        case "uint2":
        case "uint3":
        case "uint4":
            return value.map(castToUint);
        case "char2":
        case "char3":
        case "char4":
            return value.map(castToChar);
        case "short2":
        case "short3":
        case "short4":
            return value.map(castToShort);
        case "int2":
        case "int3":
        case "int4":
            return value.map(castToInt);
        case "half2":
        case "half3":
        case "half4":
            return value.map(castToHalf);
        case "float2":
        case "float3":
        case "float4":
            return value.map(castToFloat);
        default:
            throw new Error("Unknown texture inner type");
    }
}

function sampleTexture(texture, sampler, location, delta, biasFromFunctionOperand, ddx, ddy, lodFromFunctionOperand, dref)
{
    let s = location[0];
    let t = location[1];
    let r = location[2];
    let q = location[3];
    let a = location[4];

    let deltaI = undefined;
    let deltaJ = undefined;
    let deltaK = undefined;
    if (delta != undefined) {
        deltaI = delta[0];
        deltaJ = delta[1];
        deltaK = delta[2];
    }

    [s, t, r, dref] = projectionOperation(s, t, r, dref, q);
    let result = texelFiltering(
        Number.MAX_VALUE,
        Number.MAX_VALUE,
        0,
        0,
        texture,
        sampler,
        0,
        s, t, r, a,
        deltaI, deltaJ, deltaK,
        lodFromFunctionOperand,
        -Number.MAX_VALUE,
        biasFromFunctionOperand,
        ddx, ddy);
    if (dref != undefined) {
        if (result instanceof Array)
            throw new Error("Depth comparison not supported on multichannel textures");
        result = depthCompareOperation(dref, result, sampler.compareFunction);
    }
    return castToInnerType(result, texture.innerType);
    // FIXME: Figure out what to do with anisotropy
    // FIXME: Figure out how to do multisampling.
}

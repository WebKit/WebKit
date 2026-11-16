/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#import "config.h"
#import "MeshGradientRendererMetal.h"

#import "ColorInterpolationMethod.h"
#import "ColorSpace.h"
#import "ColorSpaceName.h"
#import "IntSize.h"
#import "Logging.h"
#import "MeshGradient.h"
#import "MetalColorConversion.h"
#import "PixelBuffer.h"
#import "PixelBufferConversion.h"
#import <Metal/Metal.h>
#import <simd/simd.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/MakeString.h>

namespace WebCore {

static constexpr NSUInteger sampleCount = 4;

// The largest width or height of a 2D texture on every GPU family that WebKit supports. Metal aborts, rather than
// failing, when asked for a bigger one.
//
// Metal Feature Set Tables
// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
static constexpr int maximumTextureDimension = 16384;

// Triangles only carry the position and (u, v). Each fragment evaluates the bicubic color surface in the interpolation
// color space, so its color doesn't depend on the size of the triangles. It then converts that color to the destination.
static constexpr ASCIILiteral shaderSource = R"(
struct Vertex {
    packed_float2 position;
    packed_float2 parameters;
    uint patchIndex;
};

struct RasterizerData {
    float4 position [[position]];
    // At the center of the pixel, even when the triangle only partly covers it. Two triangles sharing an edge then
    // agree on the pixels along it.
    float2 parameters [[center_no_perspective]];
    uint patchIndex [[flat]];
};

struct MeshUniforms {
    uint clampsToAlpha;
    uint hasPointsMissingAlpha;
};

// Equivalent to MeshGradient::WeightedColor in MeshGradient.h.
struct WeightedColor {
    float4 weightedValues;
    float4 weights;
};

struct WeightedMean {
    float4 weightedValues;
    float4 weights;
};

// The weights of a cubic Bezier curve's four control points. The 3s are the binomial coefficients of (s + t)^3.
static float4 cubicBernsteinPolynomialsAt(float t)
{
    // Equivalent to cubicBernsteinPolynomialsAt() in MeshGradient.cpp.

    float s = 1 - t;
    return float4(s * s * s, 3 * s * s * t, 3 * s * t * t, t * t * t);
}

// `basisU` and `basisV` are the cubic Bernstein polynomials at u and at v.
static WeightedColor evaluate(constant WeightedColor* controlValues, float4 basisU, float4 basisV)
{
    // Equivalent to MeshGradient::BicubicBezierPatch::evaluate() in MeshGradient.cpp.

    WeightedColor result = { float4(0), float4(0) };
    for (uint row = 0; row < 4; ++row) {
        for (uint column = 0; column < 4; ++column) {
            constant WeightedColor& controlValue = controlValues[row * 4 + column];
            float weight = basisV[row] * basisU[column];
            result.weightedValues += controlValue.weightedValues * weight;
            result.weights += controlValue.weights * weight;
        }
    }
    return result;
}

static WeightedMean weightedMean(constant WeightedColor* controlValues, constant WeightedColor* controlValuesMissingAlpha, bool hasPointsMissingAlpha, float u, float v)
{
    // Equivalent to MeshGradient::weightedMean() in MeshGradient.cpp.

    float4 basisU = cubicBernsteinPolynomialsAt(u);
    float4 basisV = cubicBernsteinPolynomialsAt(v);
    WeightedColor color = evaluate(controlValues, basisU, basisV);

    if (hasPointsMissingAlpha) {
        float alpha = color.weights.a > 0 ? saturate(color.weightedValues.a / color.weights.a) : 1;
        WeightedColor missingAlpha = evaluate(controlValuesMissingAlpha, basisU, basisV);
        color.weightedValues += missingAlpha.weightedValues * alpha;
        color.weights += missingAlpha.weights * alpha;
    }

    return { color.weightedValues, color.weights };
}

static float4 interpolatedColor(constant WeightedColor* controlValues, constant WeightedColor* controlValuesMissingAlpha, bool hasPointsMissingAlpha, float u, float v)
{
    // Equivalent to MeshGradient::interpolatedColor() in MeshGradient.cpp.

    WeightedMean mean = weightedMean(controlValues, controlValuesMissingAlpha, hasPointsMissingAlpha, u, v);

    float4 color = float4(0);
    bool hasComponentWithoutWeight = false;
    for (int component = 0; component < 4; ++component) {
        if (mean.weights[component] > 0)
            color[component] = mean.weightedValues[component] / mean.weights[component];
        else
            hasComponentWithoutWeight = true;
    }
    if (!hasComponentWithoutWeight)
        return color;

    const float nudge = 1.0 / 1024;
    WeightedMean nudgedMean = weightedMean(controlValues, controlValuesMissingAlpha, hasPointsMissingAlpha, u + (0.5 - u) * nudge, v + (0.5 - v) * nudge);

    for (int component = 0; component < 4; ++component) {
        if (mean.weights[component] <= 0 && nudgedMean.weights[component] > 0)
            color[component] = nudgedMean.weightedValues[component] / nudgedMean.weights[component];
    }
    return color;
}

static float4 toPremultipliedColor(float4 components, constant ColorConversionUniforms& colorConversion)
{
    // Equivalent to toPremultipliedColor() in MeshGradient.cpp.

    components.a = saturate(components.a);

    // Hues were moved by whole turns before interpolating, so normalize them back. normalizedColorComponents() does
    // this as well as clamping, and convertColorComponents() is equivalent to convertAndResolveColorComponents().
    float3 color = convertColorComponents(normalizedColorComponents(components.rgb, colorConversion), colorConversion);
    return float4(color * components.a, components.a);
}

static float4 premultipliedColor(constant WeightedColor* controlValues, constant WeightedColor* controlValuesMissingAlpha, bool hasPointsMissingAlpha, uint patchIndex, float u, float v, constant ColorConversionUniforms& colorConversion)
{
    // Equivalent to MeshGradient::premultipliedColor() in MeshGradient.cpp.

    return toPremultipliedColor(interpolatedColor(controlValues + patchIndex * 16, controlValuesMissingAlpha + patchIndex * 16, hasPointsMissingAlpha, u, v), colorConversion);
}

static float4 clampedToAlpha(float4 color)
{
    // Equivalent to clampedToAlpha() in ColorTypes.h.

    return float4(clamp(color.rgb, float3(0), float3(color.a)), color.a);
}

// MARK: - Entry Points

// Called for each vertex. Transforms the vertex's position from pixels to normalized device coordinates, and passes its
// (u, v) and patch index on to the rasterizer. The CPU renderer has no equivalent, since it works in pixels throughout.
vertex RasterizerData meshGradientVertex(uint vertexID [[vertex_id]], constant Vertex* vertices [[buffer(0)]], constant float2& viewportSize [[buffer(1)]])
{
    float2 pixel = vertices[vertexID].position;

    RasterizerData out;
    out.position = float4(pixel.x / viewportSize.x * 2 - 1, 1 - pixel.y / viewportSize.y * 2, 0, 1);
    out.parameters = vertices[vertexID].parameters;
    out.patchIndex = vertices[vertexID].patchIndex;
    return out;
}

// Called for each pixel a triangle covers (even partly). Evaluates the color at the pixel's center from the (u, v) the
// rasterizer interpolated there, and returns it premultiplied in the destination color space.
fragment float4 meshGradientFragment(RasterizerData in [[stage_in]], constant WeightedColor* controlValues [[buffer(0)]], constant ColorConversionUniforms& colorConversion [[buffer(1)]], constant MeshUniforms& uniforms [[buffer(2)]], constant WeightedColor* controlValuesMissingAlpha [[buffer(3)]])
{
    // Equivalent to the body of the loop in drawTriangle() in MeshGradientRendererCPU.cpp, and writePixel().

    // Unlike the CPU renderer, which only evaluates pixel centers inside the triangle, a pixel the triangle only partly
    // covers still has its center evaluated, which can be outside the patch.
    float2 parameters = saturate(in.parameters);

    float4 color = premultipliedColor(controlValues, controlValuesMissingAlpha, uniforms.hasPointsMissingAlpha, in.patchIndex, parameters.x, parameters.y, colorConversion);

    if (uniforms.clampsToAlpha)
        color = clampedToAlpha(color);

    return color;
}
)"_s;

// Matches `Vertex` in the shader.
struct ShaderVertex {
    float position[2];
    float parameters[2];
    uint32_t patchIndex;
};
static_assert(sizeof(ShaderVertex) == 20);

// Matches `MeshUniforms` in the shader.
struct ShaderMeshUniforms {
    uint32_t clampsToAlpha;
    uint32_t hasPointsMissingAlpha;
};

// Matches `WeightedColor` in the shader.
struct ShaderWeightedColor {
    simd_float4 weightedValues;
    simd_float4 weights;
};
static_assert(sizeof(ShaderWeightedColor) == 32);

static RetainPtr<id<MTLBuffer>> makeControlValueBuffer(id<MTLDevice> device, const MeshGradient::ColorControlValues& controlValues)
{
    RetainPtr buffer = adoptNS([device newBufferWithLength:controlValues.size() * sizeof(ShaderWeightedColor) options:MTLResourceStorageModeShared]);
    if (!buffer)
        return nullptr;

    auto toSIMD = [](auto& components) {
        return simd_float4 { components[0], components[1], components[2], components[3] };
    };
    auto contents = unsafeMakeSpan(static_cast<ShaderWeightedColor*>([buffer contents]), controlValues.size());
    std::ranges::copy(controlValues | std::views::transform([&](auto& color) {
        return ShaderWeightedColor { toSIMD(color.weightedValues), toSIMD(color.weights) };
    }), contents.begin());
    return buffer;
}

static std::optional<MTLPixelFormat> texturePixelFormat(PixelFormat format)
{
    if (format == PixelFormat::RGBA8)
        return MTLPixelFormatRGBA8Unorm;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (format == PixelFormat::RGBA16F)
        return MTLPixelFormatRGBA16Float;
#endif
    return std::nullopt;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(MeshGradientRendererMetal);

static RetainPtr<MTLRenderPipelineState> makePipelineState(id<MTLDevice> device, id<MTLLibrary> library, MTLPixelFormat pixelFormat)
{
    RetainPtr vertexFunction = adoptNS([library newFunctionWithName:@"meshGradientVertex"]);
    RetainPtr fragmentFunction = adoptNS([library newFunctionWithName:@"meshGradientFragment"]);

    RetainPtr pipelineDescriptor = adoptNS([[MTLRenderPipelineDescriptor alloc] init]);
    [pipelineDescriptor setVertexFunction:vertexFunction.get()];
    [pipelineDescriptor setFragmentFunction:fragmentFunction.get()];
    [pipelineDescriptor setRasterSampleCount:sampleCount];
    [pipelineDescriptor colorAttachments][0].pixelFormat = pixelFormat;

    NSError *error = nil;
    RetainPtr<MTLRenderPipelineState> pipelineState = adoptNS([device newRenderPipelineStateWithDescriptor:pipelineDescriptor.get() error:&error]);
    if (!pipelineState)
        LOG_ERROR("Failed to create mesh gradient pipeline: %s", error.localizedDescription.UTF8String);
    return pipelineState;
}

MeshGradientRendererMetal* MeshGradientRendererMetal::singleton()
{
    static NeverDestroyed<std::unique_ptr<MeshGradientRendererMetal>> renderer = create();
    return renderer.get().get();
}

std::unique_ptr<MeshGradientRendererMetal> MeshGradientRendererMetal::create()
{
    RetainPtr<MTLDevice> device = adoptNS(MTLCreateSystemDefaultDevice());
    if (!device || ![device supportsTextureSampleCount:sampleCount])
        return nullptr;

    NSError *error = nil;
    RetainPtr library = adoptNS([device newLibraryWithSource:makeString(metalColorConversionShaderSource, shaderSource).createNSString().get() options:nil error:&error]);
    if (!library) {
        LOG_ERROR("Failed to compile mesh gradient shaders: %s", error.localizedDescription.UTF8String);
        return nullptr;
    }

    auto eightBitPipelineState = makePipelineState(device.get(), library.get(), MTLPixelFormatRGBA8Unorm);
    auto floatPipelineState = makePipelineState(device.get(), library.get(), MTLPixelFormatRGBA16Float);
    if (!eightBitPipelineState || !floatPipelineState)
        return nullptr;

    RetainPtr<MTLCommandQueue> commandQueue = adoptNS([device newCommandQueue]);
    if (!commandQueue)
        return nullptr;

    return makeUnique<MeshGradientRendererMetal>(WTF::move(device), WTF::move(commandQueue), WTF::move(eightBitPipelineState), WTF::move(floatPipelineState));
}

MeshGradientRendererMetal::MeshGradientRendererMetal(RetainPtr<MTLDevice>&& device, RetainPtr<MTLCommandQueue>&& commandQueue, RetainPtr<MTLRenderPipelineState>&& eightBitPipelineState, RetainPtr<MTLRenderPipelineState>&& floatPipelineState)
    : m_device { WTF::move(device) }
    , m_commandQueue { WTF::move(commandQueue) }
    , m_eightBitPipelineState { WTF::move(eightBitPipelineState) }
    , m_floatPipelineState { WTF::move(floatPipelineState) }
{
}

static RetainPtr<id<MTLTexture>> makeTexture(id<MTLDevice> device, MTLPixelFormat pixelFormat, const IntSize& size, NSUInteger samples)
{
    RetainPtr descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat width:size.width() height:size.height() mipmapped:NO];
    [descriptor setTextureType:samples > 1 ? MTLTextureType2DMultisample : MTLTextureType2D];
    [descriptor setSampleCount:samples];
    [descriptor setUsage:MTLTextureUsageRenderTarget];
    [descriptor setStorageMode:MTLStorageModePrivate];
    return adoptNS([device newTextureWithDescriptor:descriptor.get()]);
}

static Vector<ShaderVertex> shaderVertices(const MeshGradient& gradient, float scale)
{
    Vector<ShaderVertex> vertices;
    for (auto& triangle : gradient.triangles(scale)) {
        for (auto& vertex : { triangle.a, triangle.b, triangle.c })
            vertices.append({ { vertex.position.x(), vertex.position.y() }, { vertex.u, vertex.v }, vertex.patchIndex });
    }
    return vertices;
}

bool MeshGradientRendererMetal::render(const MeshGradient& gradient, const PixelBufferConversionView& destination, const IntSize& size, float scale)
{
    ASSERT(destination.format.alphaFormat == AlphaPremultiplication::Premultiplied);

    auto& method = gradient.colorInterpolationMethod();
    auto sourceColorSpace = WTF::switchOn(method.colorSpace, []<typename MethodColorSpace>(const MethodColorSpace&) {
        return ColorSpaceFor<typename MethodColorSpace::ColorType>;
    });

    auto colorSpace = MeshGradient::renderingColorSpace(destination.format.colorSpace);
    auto pixelFormat = texturePixelFormat(destination.format.pixelFormat);
    auto colorConversion = makeMetalColorConversionUniforms(sourceColorSpace, destination.format.colorSpace);

    // Unlike the CPU renderer, this doesn't assert, so that a caller can fall back to the CPU renderer.
    if (!pixelFormat || !colorSpace || !colorConversion)
        return false;

    if (size.isEmpty())
        return true;

    // Unlike the CPU renderer, this can't draw past the maximum texture size, so a caller falls back to the CPU renderer.
    if (size.width() > maximumTextureDimension || size.height() > maximumTextureDimension)
        return false;

    NSUInteger bytesPerRow = size.width() * PixelBuffer::bytesPerPixel(destination.format.pixelFormat);
    if (destination.bytesPerRow < bytesPerRow || destination.rows.size() / destination.bytesPerRow < static_cast<size_t>(size.height()))
        return false;

    bool clampsToAlpha = destination.format.pixelFormat == PixelFormat::RGBA8;
    auto controlValuesMissingAlpha = gradient.colorControlValuesMissingAlpha();
    ShaderMeshUniforms meshUniforms { clampsToAlpha, controlValuesMissingAlpha.size() > 0 };

    @autoreleasepool {
        RetainPtr multisampledTexture = makeTexture(m_device.get(), *pixelFormat, size, sampleCount);
        RetainPtr texture = makeTexture(m_device.get(), *pixelFormat, size, 1);
        if (!multisampledTexture || !texture)
            return false;

        // Fill with the background color by clearing to it.
        auto background = gradient.premultipliedBackgroundColor(*colorSpace);
        if (clampsToAlpha)
            background = clampedToAlpha(background);
        auto [red, green, blue, alpha] = background;

        RetainPtr renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        [renderPassDescriptor colorAttachments][0].texture = multisampledTexture.get();
        [renderPassDescriptor colorAttachments][0].resolveTexture = texture.get();
        [renderPassDescriptor colorAttachments][0].loadAction = MTLLoadActionClear;
        [renderPassDescriptor colorAttachments][0].clearColor = MTLClearColorMake(red, green, blue, alpha);
        [renderPassDescriptor colorAttachments][0].storeAction = MTLStoreActionMultisampleResolve;

        RetainPtr commandBuffer = [m_commandQueue commandBuffer];
        RetainPtr renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor.get()];

        // Draw the triangles. Triangles replace what is under them, and where a patch folds over itself the last one drawn
        // shows. Unlike the CPU renderer, they are multisampled, so the edges of the mesh are antialiased.
        const auto vertices = shaderVertices(gradient, scale);

        if (!vertices.isEmpty()) {
            auto vertexBytes = asByteSpan(vertices.span());
            RetainPtr vertexBuffer = adoptNS([m_device newBufferWithBytes:vertexBytes.data() length:vertexBytes.size() options:MTLResourceStorageModeShared]);

            RetainPtr controlValueBuffer = makeControlValueBuffer(m_device.get(), gradient.colorControlValues());

            // The shader only reads the buffer for the points missing their alpha when there are some. Otherwise any buffer
            // will do there.
            RetainPtr controlValueBufferMissingAlpha = meshUniforms.hasPointsMissingAlpha ? makeControlValueBuffer(m_device.get(), controlValuesMissingAlpha) : controlValueBuffer;

            if (!vertexBuffer || !controlValueBuffer || !controlValueBufferMissingAlpha) {
                [renderEncoder endEncoding];
                return false;
            }

            simd_float2 viewportSize { static_cast<float>(size.width()), static_cast<float>(size.height()) };

            [renderEncoder setRenderPipelineState:*pixelFormat == MTLPixelFormatRGBA8Unorm ? m_eightBitPipelineState.get() : m_floatPipelineState.get()];
            [renderEncoder setVertexBuffer:vertexBuffer.get() offset:0 atIndex:0];
            [renderEncoder setVertexBytes:&viewportSize length:sizeof(viewportSize) atIndex:1];
            [renderEncoder setFragmentBuffer:controlValueBuffer.get() offset:0 atIndex:0];
            [renderEncoder setFragmentBytes:&*colorConversion length:sizeof(MetalColorConversionUniforms) atIndex:1];
            [renderEncoder setFragmentBytes:&meshUniforms length:sizeof(meshUniforms) atIndex:2];
            [renderEncoder setFragmentBuffer:controlValueBufferMissingAlpha.get() offset:0 atIndex:3];
            [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];
        }
        [renderEncoder endEncoding];

        // Unlike the CPU renderer, which draws into the destination, this draws into a texture. Copy the texture into a
        // buffer the CPU can read, wait for the GPU, then copy the pixels to the destination.
        NSUInteger byteLength = bytesPerRow * size.height();
        RetainPtr readbackBuffer = adoptNS([m_device newBufferWithLength:byteLength options:MTLResourceStorageModeShared]);
        if (!readbackBuffer)
            return false;

        RetainPtr blitEncoder = [commandBuffer blitCommandEncoder];
        [blitEncoder copyFromTexture:texture.get() sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(size.width(), size.height(), 1) toBuffer:readbackBuffer.get() destinationOffset:0 destinationBytesPerRow:bytesPerRow destinationBytesPerImage:byteLength];
        [blitEncoder endEncoding];

        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        if ([commandBuffer status] != MTLCommandBufferStatusCompleted)
            return false;

        auto readbackBytes = unsafeMakeSpan(static_cast<const uint8_t*>([readbackBuffer contents]), byteLength);
        for (int y = 0; y < size.height(); ++y) {
            auto row = readbackBytes.subspan(static_cast<size_t>(y) * bytesPerRow, bytesPerRow);
            memcpySpan(destination.rows.subspan(static_cast<size_t>(y) * destination.bytesPerRow, bytesPerRow), row);
        }
    }

    return true;
}

} // namespace WebCore

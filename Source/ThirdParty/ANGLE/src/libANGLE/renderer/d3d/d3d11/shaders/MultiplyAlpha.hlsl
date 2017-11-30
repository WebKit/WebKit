Texture2D<float4> TextureF  : register(t0);
Texture2D<uint4>  TextureUI : register(t0);

SamplerState Sampler        : register(s0);

// Notation:
// PM: premultiply, UM: unmulitply, PT: passthrough
// F: float, U: uint

// Float to float LUMA
float4 PS_FtoF_PM_LUMA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    color.rgb = color.r * color.a;
    color.a = 1.0f;
    return color;
}
float4 PS_FtoF_UM_LUMA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    if (color.a > 0.0f)
    {
        color.rgb = color.r / color.a;
    }
    color.a = 1.0f;
    return color;
}

// Float to float LUMAALPHA
float4 PS_FtoF_PM_LUMAALPHA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    color.rgb = color.r * color.a;
    return color;
}

float4 PS_FtoF_UM_LUMAALPHA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    if (color.a > 0.0f)
    {
        color.rgb = color.r / color.a;
    }
    return color;
}

// Float to float RGBA
float4 PS_FtoF_PM_RGBA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    color.rgb *= color.a;
    return color;
}

float4 PS_FtoF_UM_RGBA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    if (color.a > 0.0f)
    {
        color.rgb /= color.a;
    }
    return color;
}

// Float to float RGB
float4 PS_FtoF_PM_RGB(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    color.rgb *= color.a;
    color.a = 1.0f;
    return color;
}

float4 PS_FtoF_UM_RGB(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    if (color.a > 0.0f)
    {
        color.rgb /= color.a;
    }
    color.a = 1.0f;
    return color;
}

// Float to uint RGBA
uint4 PS_FtoU_PT_RGBA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    return uint4(color * 255);
}

uint4 PS_FtoU_PM_RGBA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    color.rgb *= color.a;
    return uint4(color * 255);
}

uint4 PS_FtoU_UM_RGBA(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    if (color.a > 0.0f)
    {
        color.rgb /= color.a;
    }
    return uint4(color * 255);
}

// Float to uint RGB
uint4 PS_FtoU_PT_RGB(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    return uint4(color.rgb * 255, 1);
}

uint4 PS_FtoU_PM_RGB(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    color.rgb *= color.a;
    return uint4(color.rgb * 255, 1);
}

uint4 PS_FtoU_UM_RGB(in float4 inPosition : SV_POSITION, in float2 inTexCoord : TEXCOORD0) : SV_TARGET0
{
    float4 color = TextureF.Sample(Sampler, inTexCoord).rgba;
    if (color.a > 0.0f)
    {
        color.rgb /= color.a;
    }
    return uint4(color.rgb * 255, 1);
}

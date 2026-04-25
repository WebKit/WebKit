RWTexture2D<float4> t : register(u0, space0);

static float4 sk_FragColor;

struct SPIRV_Cross_Output
{
    float4 sk_FragColor : SV_Target0;
};

uint2 spvImageSize(RWTexture2D<float4> Tex, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(ret.x, ret.y);
    Param = 0u;
    return ret;
}

void frag_main()
{
    uint _21_dummy_parameter;
    uint2 _21 = spvImageSize(t, _21_dummy_parameter);
    uint2 dims = _21;
    uint _23 = _21.x;
    uint _25 = _21.y;
    sk_FragColor = float4(float2(float(_23), float(_25)), float2(float(_23), float(_25)));
}

SPIRV_Cross_Output main()
{
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.sk_FragColor = sk_FragColor;
    return stage_output;
}

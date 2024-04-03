static uint gl_SampleMaskIn[1];
static uint gl_SampleMask[1];
static float4 sk_FragColor;

struct SPIRV_Cross_Input
{
    uint gl_SampleMaskIn : SV_Coverage;
};

struct SPIRV_Cross_Output
{
    float4 sk_FragColor : SV_Target0;
    uint gl_SampleMask : SV_Coverage;
};

void clear_samplemask_v()
{
    gl_SampleMask[0] = 0u;
}

void reset_samplemask_v()
{
    gl_SampleMask[0] = uint(gl_SampleMaskIn[0]);
}

float4 samplemaskin_as_color_h4()
{
    return float(uint(gl_SampleMaskIn[0])).xxxx;
}

void frag_main()
{
    clear_samplemask_v();
    reset_samplemask_v();
    gl_SampleMask[0] = 4294967295u;
    sk_FragColor = samplemaskin_as_color_h4() * 0.00390625f;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_SampleMaskIn[0] = stage_input.gl_SampleMaskIn;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_SampleMask = gl_SampleMask[0];
    stage_output.sk_FragColor = sk_FragColor;
    return stage_output;
}

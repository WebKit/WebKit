// Pixel shader to invert color of texture
texture sceneTexture;

sampler2D sceneTextureSampler = sampler_state {
    Texture = <sceneTexture>;
};

float4 InvertColorPS(float2 textureCoordinate : TEXCOORD0) : COLOR0
{
    float4 color = tex2D(sceneTextureSampler, textureCoordinate);
    color.rgb = 1.0f - color.rgb;
    return color;
};

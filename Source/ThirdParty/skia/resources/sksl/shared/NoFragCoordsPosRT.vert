/*#pragma settings CannotUseFragCoord*/

uniform float4 sk_RTAdjust;
layout(location=0) in float4 pos;

void main() {
    sk_Position = pos;
}

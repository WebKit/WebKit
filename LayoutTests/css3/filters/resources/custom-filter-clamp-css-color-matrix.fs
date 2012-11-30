precision mediump float;

void main(void)
{
    css_ColorMatrix = mat4(10.0, 0.0, 0.0, 0.0,
                           0.0, 10.0, 0.0, 0.0,
                           0.0, 0.0, 10.0, 0.0,
                           0.0, 0.0, 0.0, 10.0);
    css_MixColor = vec4(0.5, 0.5, 0.5, 1.0);
}

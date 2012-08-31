precision mediump float;

varying vec2 v_texCoord;

void main()
{
    // Produce a blue square in the lower right-hand part of the image.
    css_MixColor = vec4(0.0, 0.0, 1.0, float(v_texCoord.x > 0.33 && v_texCoord.y > 0.33));
}

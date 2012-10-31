// If this shader's related test passes, the custom filter does not execute because u_tileSize's type is invalid.

precision mediump float;

// u_tileSize should be a vec2, not a mat2.
uniform mat2 u_tileSize;

void main()
{
    css_MixColor = vec4(1.0, 0.0, 0.0, 1.0);
}

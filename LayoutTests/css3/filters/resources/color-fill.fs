precision mediump float;
uniform vec4 color;
varying vec2 v_texCoord;
uniform sampler2D u_texture;
void main()
{
    vec4 sourceColor = texture2D(u_texture, v_texCoord);
    // Note that gl_FragColor and sourceColor are unmultiplied.
    // Multiply the texture with the color specified as an uniform. Also apply a 
    // gradient to check for the right direction. The result should be transparent on top.
    gl_FragColor = vec4(sourceColor.xyz * color.xyz, sourceColor.a * color.a * v_texCoord.y);
}

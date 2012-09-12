// This test checks that a shader cannot read from the internal "css_u_texture" sampler, which references the DOM element texture.
// This shader should fail to validate because "css_u_texture" should be undefined to this code.

precision mediump float;

varying vec2 v_texCoord;

void main()
{
	// Try to sample the DOM element texture.
	vec4 color = texture2D(css_u_texture, v_texCoord);

	// Swap the red and green channels.
	color.rg = color.gr;

	css_MixColor = color;
}

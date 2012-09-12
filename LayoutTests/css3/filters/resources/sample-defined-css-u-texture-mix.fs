// This test checks that a shader cannot read from the internal "css_u_texture" sampler, which references the DOM element texture.
// This shader is referenced from the CSS mix function and should fail to validate because it defines "css_u_texture",
// using the restricted "css_" prefix.

precision mediump float;

uniform sampler2D css_u_texture;

varying vec2 v_texCoord;

void main()
{
	// Try to sample the DOM element texture.
	vec4 color = texture2D(css_u_texture, v_texCoord);

	// Swap the red and green channels.
	color.rg = color.gr;

	css_MixColor = color;
}

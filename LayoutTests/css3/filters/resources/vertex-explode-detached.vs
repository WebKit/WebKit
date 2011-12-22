precision mediump float;

attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec3 a_triangleCoord;

uniform mat4 u_projectionMatrix;
varying vec2 v_texCoord;

void main()
{
    // a_triangleCoord.xy is the position of the tile in the mesh, so we offset the vertices
    // with the position in mesh. The tiles should be detached from each other.
    gl_Position = u_projectionMatrix * (a_position + vec4(a_triangleCoord.x / 4.0, a_triangleCoord.y / 4.0, 0.0, 0.0));
    v_texCoord = a_texCoord;
}

attribute vec4 a_vertex;
attribute vec3 a_normal;

uniform mat4 u_modelViewProjMatrix;

struct MyStruct
{
  int x;
  int y;
};

uniform MyStruct u_struct;
uniform float u_array[4];

varying vec3 v_normal;

void main()
{ 
    v_normal = a_normal;
    gl_Position = u_modelViewProjMatrix * a_vertex +
        vec4(u_struct.x, u_struct.y, 0, 1) +
        vec4(u_array[0], u_array[1], u_array[2], u_array[3]);
}

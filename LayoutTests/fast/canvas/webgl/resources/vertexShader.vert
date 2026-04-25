attribute vec4 a_vertex;
attribute vec3 a_normal;

uniform mat4 u_modelViewProjMatrix;

varying vec3 v_normal;

void main()
{ 
    v_normal = a_normal;
    gl_Position =  u_modelViewProjMatrix * a_vertex;
}

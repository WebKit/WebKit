// Non ascii data in source should fail
// See GLSL ES Spec 1.0.17 section 3.1 and 3.2
// ����͂`�r�b�h�h�ł͂Ȃ��ł��B
// �s�������@�h���@�m�����@�`�r�b�h�h
uniform mat4 �m�����`�r�b�h�h;
void main() {
  gl_Position = vec4(1,1,1,1);
}

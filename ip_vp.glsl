#version 130

varying out vec4 pos;

void main() {
     pos = gl_Vertex;
     gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

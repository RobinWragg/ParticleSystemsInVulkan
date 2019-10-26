#version 450

layout(location = 0) in vec3 vertPosition;
layout(location = 1) in float vertBrightness;

layout(location = 0) out vec3 fragmentColor;

void main() {
	gl_PointSize = 16;
    gl_Position = vec4(vertPosition, 1.0);
	fragmentColor = vec3(vertBrightness, vertBrightness, 1);
}
#version 450

layout(location = 0) in vec3 vertPosition;
layout(location = 1) in float vertBrightness;

layout(location = 0) out vec3 fragmentColor;

void main() {
	gl_PointSize = 2;
    gl_Position = vec4(vertPosition, 1.0);

	// Set the water shade based on vertBrightness and dim the fragment based on the particle's depth (vertPosition.z).
	float depthDarkening = (1 - vertPosition.z*vertPosition.z) * 1.1;
	fragmentColor = vec3(vertBrightness, vertBrightness, 1) * depthDarkening;
}
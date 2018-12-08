#version 430 core

#define SHADOW_MAP_CASCADE_COUNT 4

layout (triangles) in;
//layout (invocations = 4) in;
layout (triangle_strip, max_vertices = 3) out;

out vec4 FragPos; // FragPos from GS (output per emitvertex)

void main()
{
	//gl_Layer = gl_InvocationID;
	for(int i = 0; i < gl_in.length(); ++i) // for each triangle's vertices
	{
		FragPos = gl_in[i].gl_Position;
		gl_Position = FragPos;
		EmitVertex();
	}    
	EndPrimitive();
} 
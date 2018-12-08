#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
	vec3 smc;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;

const float distortion = 1.0;
const float mult = 1.0 + distortion;

#define toNDC(in) 2.0 * (in - 0.5)
#define fromNDC(in) 0.5 * (in + 1.0)

#define pos vs_out.FragPosLightSpace

void main()
{
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
    vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);

	pos.z = toNDC(pos.z);
	pos.z = mult * pos.z / (abs(pos.z) + distortion);
	pos.z = fromNDC(pos.z);

	/*
	//vec4 lsfp = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
	//lsfp.xy = mult * lsfp.xy / (abs(lsfp.xy) + distortion);
	//vs_out.FragPosLightSpace = lsfp;

	vec4 lpos = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);

	lpos.x=lpos.x/(abs(lpos.x)+1.0); 
	lpos.y=lpos.y/(abs(lpos.y)+1.0);

	vs_out.smc = vec3(lpos.xyz*0.5+0.5);
	vs_out.smc.z -= - 0.0001;
	*/

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
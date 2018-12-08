#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

const float distortion = 1.0;
const float mult = 1.0 + distortion;

#define toNDC(in) 2.0 * (in - 0.5)
#define fromNDC(in) 0.5 * (in + 1.0)

void main()
{
    //gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
	//gl_Position.xy = mult * gl_Position.xy / (abs(gl_Position.xy) + distortion);
	vec4 pos = lightSpaceMatrix * model * vec4(aPos, 1.0);

	//pos.x -= 1.5;
	pos.z = toNDC(pos.z);
	pos.z = mult * pos.z / (abs(pos.z) + distortion);
	pos.z = fromNDC(pos.z);

	//pos /= pos.w;
	//pos.z = pos.x;

	gl_Position = pos;
}
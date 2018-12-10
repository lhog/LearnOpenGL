#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

// texture sampler
uniform sampler2D texture1;

#define KERNEL_HS 5
#define KERNEL_HS_SIZE (2 * KERNEL_HS)
#define KERNEL_HS_SIZE1 (2 * KERNEL_HS + 1)

#define KERNEL_DIV float(KERNEL_HS_SIZE1*KERNEL_HS_SIZE1)

void main()
{
	for (int i = -KERNEL_HS; i <= KERNEL_HS; ++i)
		for (int j = -KERNEL_HS; j <= KERNEL_HS; ++j)
			FragColor += textureOffset(texture1, TexCoord, ivec2(i,j));
	FragColor /= KERNEL_DIV;
}
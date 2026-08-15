
#if __VERSION__ >= 300
#define varying in
out vec4 FragColor;
#define gl_FragColor FragColor
#endif

void main()
{
	gl_FragColor = vec4(1.0, 1.0, 1.0, 0.0);
}


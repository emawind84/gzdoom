
varying vec2 TexCoord;

uniform sampler2D InputTexture;
uniform sampler2D DitherTexture;

vec4 ApplyGamma(vec4 c)
{
	vec3 valgray;

	valgray = mix(vec3(dot(c.rgb, vec3(0.3,0.56,0.14))), c.rgb, Saturation);

	vec3 val = valgray * Contrast - (Contrast - 1.0) * 0.5;
	val += Brightness * 0.5;
	val = pow(max(val, vec3(0.0)), vec3(InvGamma));
	// Unlike the desktop GL backend, this GLES pipeline has no HDR/tonemap
	// pass to force alpha back to 1 beforehand, so translucent draws (e.g.
	// the sky dome) can leave it <1 here. That's harmless for the 2D
	// window backbuffer (alpha is ignored), but SteamVR's compositor/
	// overlay does composite on submitted alpha, causing a black VR sky.
	// Force opaque unconditionally to match backend GL's effective output.
	return vec4(val, 1.0);
}

//vec4 Dither(vec4 c)
//{
//	if (ColorScale == 0.0)
//		return c;
//	vec2 texSize = vec2(textureSize(DitherTexture, 0));
//	float threshold = texture2D(DitherTexture, gl_FragCoord.xy / texSize).r;
//	return vec4(floor(c.rgb * ColorScale + threshold) / ColorScale, c.a);
//}


void main()
{
	gl_FragColor =  ApplyGamma(texture2D(InputTexture, UVOffset + TexCoord * UVScale));
}

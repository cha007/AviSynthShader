#include "ConvertFromFloat.h"

ConvertFromFloat::ConvertFromFloat(PClip _child, const char* _format, bool _convertYuv, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), precisionShift(_precision + 1), format(_format), convertYUV(_convertYuv) {
	if (!vi.IsRGB32())
		env->ThrowError("Source must be float-precision RGB");
	if (strcmp(format, "YV24") != 0 && strcmp(format, "YV12") != 0 && strcmp(format, "RGB32") != 0)
		env->ThrowError("Destination format must be YV24, YV12 or RGB32");
	if (precision != 1 && precision != 2)
		env->ThrowError("Precision must be 1 or 2");

	// Convert from float-precision RGB to YV24
	viYV = vi;
	if (strcmp(format, "RGB32") == 0)
		viYV.pixel_type = VideoInfo::CS_BGR32;
	else
		viYV.pixel_type = VideoInfo::CS_YV24;

	if (precision == 2) // Half-float frame has its width twice larger than normal
		viYV.width >>= 1;
}

ConvertFromFloat::~ConvertFromFloat() {
}


PVideoFrame __stdcall ConvertFromFloat::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV24
	PVideoFrame dst = env->NewVideoFrame(viYV);
	if (viYV.pixel_type == VideoInfo::CS_BGR32)
		convFloatToRGB32(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), viYV.width, viYV.height);
	else
		convFloatToYV24(src->GetReadPtr(),
			dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V),
			src->GetPitch(), dst->GetPitch(PLANAR_Y), dst->GetPitch(PLANAR_U), viYV.width, viYV.height);
	return dst;
}

void ConvertFromFloat::convFloatToYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	unsigned char U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			convFloat(src + (x << precisionShift), &py[x], &pu[x], &pv[x]);
		}
		src += pitch1;
		py += pitch2Y;
		pu += pitch2UV;
		pv += pitch2UV;
	}
}

void ConvertFromFloat::convFloatToRGB32(const byte *src, unsigned char *dst,
	int pitchSrc, int pitchDst, int width, int height) {
	ZeroMemory(dst, pitchDst * height);
	dst += height * pitchDst;
	for (int y = 0; y < height; ++y) {
		dst -= pitchDst;
		for (int x = 0; x < width; ++x) {
			convFloat(src + (x << precisionShift), &dst[x * 4 + 2], &dst[x * 4 + 1], &dst[x * 4]);
		}
		src += pitchSrc;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertFromFloat::convFloat(const byte* src, byte* outY, unsigned char* outU, unsigned char* outV) {
	float r, g, b;
	D3DXFLOAT16 r2, g2, b2;

	if (precision == 1) {
		unsigned char r1, g1, b1;
		memcpy(&b1, src, precision);
		memcpy(&g1, src + precision, precision);
		memcpy(&r1, src + precision * 2, precision);
		b = float(b2);
		g = float(g2);
		r = float(r2);
	}
	else {
		memcpy(&r2, src + precision * 0, precision);
		memcpy(&g2, src + precision * 1, precision);
		memcpy(&b2, src + precision * 2, precision);
		// rgb are in the 0 to 1 range
		r = float(r2) * 255;
		b = float(b2) * 255;
		g = float(g2) * 255;
	}

	float y2, u2, v2;
	short y, u, v;
	if (convertYUV) {
		y2 = (0.257f * r) + (0.504f * g) + (0.098f * b) + 16;
		v2 = (0.439f * r) - (0.368f * g) - (0.071f * b) + 128;
		u2 = -(0.148f * r) - (0.291f * g) + (0.439f * b) + 128;
	}
	else {
		y2 = r;
		u2 = g;
		v2 = b;
	}

	y = short(y2 + 0.5f);
	u = short(u2 + 0.5f);
	v = short(v2 + 0.5f);

	if (y > 255) y = 255;
	if (u > 255) u = 255;
	if (v > 255) v = 255;
	if (y < 0) y = 0;
	if (u < 0) u = 0;
	if (v < 0) v = 0;

	// Store YUV
	outY[0] = unsigned char(y);
	outU[0] = unsigned char(u);
	outV[0] = unsigned char(v);
}
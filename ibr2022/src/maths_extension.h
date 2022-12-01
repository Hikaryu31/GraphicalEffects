#pragma once

// NOTE: Add your own maths functions

inline int Rng(const int min, const int max)
{
	return rand() % (max - min + 1) + min;
}

inline float Rng(const float min, const float max)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = max - min;
	float r = random * diff;
	return min + r;
}

class Transform
{
public:
	v3 t = { 0.f, 0.f, 0.f };
	v3 r = { 0.f, 0.f, 0.f };
	v3 s = { 1.f, 1.f, 1.f };

	inline mat4 GetModelMatrix() const
	{
		return Mat4::Translate(t) * Mat4::RotateX(r.x) * Mat4::RotateY(r.y) * Mat4::RotateZ(r.z) * Mat4::Scale(s);
	}
};

namespace Mat4
{
	inline mat4 Orthographic(float Left, float Right, float Bottom, float Top, float Near, float Far)
	{
		return
		{
			2.f / (Right - Left),				0.f,								0.f,							0.f,
			0.f,								2.f / (Top - Bottom),				0.f,							0.f,
			0.f,								0.f,								-2.f / (Far - Near),			0.f,
			-(Right + Left) / (Right - Left),	-(Top + Bottom) / (Top - Bottom),	-(Far + Near) / (Far - Near),	1.f
		};
	}
}
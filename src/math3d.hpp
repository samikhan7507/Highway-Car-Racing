// math3d.hpp - hand-written CG math used by BOTH the game and the CG Learning Mode.
// Convention: row-major matrices, column vectors (p' = M * p). toGL() transposes for OpenGL.
#pragma once
#include <cmath>
namespace cg {
constexpr float PI = 3.14159265358979f;
inline float rad(float d) { return d * PI / 180.f; }
inline float deg(float r) { return r * 180.f / PI; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec4 { float x = 0, y = 0, z = 0, w = 1; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline Vec3 normalize(Vec3 a) { float l = std::sqrt(dot(a, a)); return l > 1e-9f ? a * (1.f / l) : a; }

// ---------- 2D homogeneous transforms (3x3) ----------
struct Mat3 {
  float m[3][3];
  static Mat3 identity() { Mat3 r{}; r.m[0][0] = r.m[1][1] = r.m[2][2] = 1; return r; }
  static Mat3 translate(float tx, float ty) { auto r = identity(); r.m[0][2] = tx; r.m[1][2] = ty; return r; }
  static Mat3 rotate(float a) { auto r = identity(); float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c; r.m[0][1] = -s; r.m[1][0] = s; r.m[1][1] = c; return r; }
  static Mat3 scale(float sx, float sy) { auto r = identity(); r.m[0][0] = sx; r.m[1][1] = sy; return r; }
  // mode: 0 about x-axis, 1 about y-axis, 2 about origin, 3 about y=x, 4 about y=-x
  static Mat3 reflect(int mode) { auto r = identity();
    switch (mode) {
      case 0: r.m[1][1] = -1; break;
      case 1: r.m[0][0] = -1; break;
      case 2: r.m[0][0] = r.m[1][1] = -1; break;
      case 3: r.m[0][0] = 0; r.m[1][1] = 0; r.m[0][1] = 1; r.m[1][0] = 1; break;
      default: r.m[0][0] = 0; r.m[1][1] = 0; r.m[0][1] = -1; r.m[1][0] = -1; }
    return r; }
  static Mat3 shear(float shx, float shy) { auto r = identity(); r.m[0][1] = shx; r.m[1][0] = shy; return r; }
  Mat3 operator*(const Mat3& o) const { Mat3 r{};
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) for (int k = 0; k < 3; k++) r.m[i][j] += m[i][k] * o.m[k][j];
    return r; }
  Vec2 apply(Vec2 p) const { return {m[0][0] * p.x + m[0][1] * p.y + m[0][2], m[1][0] * p.x + m[1][1] * p.y + m[1][2]}; }
};

// ---------- 3D homogeneous transforms (4x4) ----------
struct Mat4 {
  float m[4][4];
  static Mat4 identity() { Mat4 r{}; for (int i = 0; i < 4; i++) r.m[i][i] = 1; return r; }
  static Mat4 translate(float x, float y, float z) { auto r = identity(); r.m[0][3] = x; r.m[1][3] = y; r.m[2][3] = z; return r; }
  static Mat4 scale(float x, float y, float z) { auto r = identity(); r.m[0][0] = x; r.m[1][1] = y; r.m[2][2] = z; return r; }
  static Mat4 rotateX(float a) { auto r = identity(); float c = std::cos(a), s = std::sin(a);
    r.m[1][1] = c; r.m[1][2] = -s; r.m[2][1] = s; r.m[2][2] = c; return r; }
  static Mat4 rotateY(float a) { auto r = identity(); float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c; r.m[0][2] = s; r.m[2][0] = -s; r.m[2][2] = c; return r; }
  static Mat4 rotateZ(float a) { auto r = identity(); float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c; r.m[0][1] = -s; r.m[1][0] = s; r.m[1][1] = c; return r; }
  // x' = x + xy*y + xz*z ; y' = y + yx*x + yz*z ; z' = z + zx*x + zy*y
  static Mat4 shear(float xy, float xz, float yx, float yz, float zx, float zy) { auto r = identity();
    r.m[0][1] = xy; r.m[0][2] = xz; r.m[1][0] = yx; r.m[1][2] = yz; r.m[2][0] = zx; r.m[2][1] = zy; return r; }
  static Mat4 reflectX() { return scale(-1, 1, 1); }  // mirror about the YZ plane
  static Mat4 perspective(float fovy, float aspect, float n, float f) { Mat4 r{}; float t = 1.f / std::tan(fovy / 2);
    r.m[0][0] = t / aspect; r.m[1][1] = t; r.m[2][2] = (f + n) / (n - f); r.m[2][3] = 2 * f * n / (n - f); r.m[3][2] = -1; return r; }
  static Mat4 ortho(float l, float rr, float b, float t, float n, float f) { auto r = identity();
    r.m[0][0] = 2 / (rr - l); r.m[1][1] = 2 / (t - b); r.m[2][2] = -2 / (f - n);
    r.m[0][3] = -(rr + l) / (rr - l); r.m[1][3] = -(t + b) / (t - b); r.m[2][3] = -(f + n) / (f - n); return r; }
  static Mat4 lookAt(Vec3 e, Vec3 c, Vec3 up) { Vec3 f = normalize(c - e), s = normalize(cross(f, up)), u = cross(s, f);
    auto r = identity();
    r.m[0][0] = s.x; r.m[0][1] = s.y; r.m[0][2] = s.z; r.m[0][3] = -dot(s, e);
    r.m[1][0] = u.x; r.m[1][1] = u.y; r.m[1][2] = u.z; r.m[1][3] = -dot(u, e);
    r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = dot(f, e); return r; }
  Mat4 operator*(const Mat4& o) const { Mat4 r{};
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) r.m[i][j] += m[i][k] * o.m[k][j];
    return r; }
  Vec4 mul(Vec3 p, float w = 1) const { return {
    m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3] * w, m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3] * w,
    m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3] * w, m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3] * w}; }
  void toGL(float* o) const { for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) o[c * 4 + r] = m[r][c]; }
};
}  // namespace cg

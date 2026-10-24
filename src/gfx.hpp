// gfx.hpp - small OpenGL/GLUT helpers (2D HUD drawing, text, primitive solids).
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include "math3d.hpp"
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

namespace gfx {
constexpr float VW = 1280.f, VH = 720.f;  // logical HUD resolution (viewport is letterboxed to 16:9)
inline void col(float r, float g, float b, float a = 1.f) { glColor4f(r, g, b, a); }
inline void text(float x, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) {
  glRasterPos2f(x, y); for (char c : s) glutBitmapCharacter(font, (unsigned char)c); }
inline float textW(const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) { float w = 0; for (char c : s) w += glutBitmapWidth(font, (unsigned char)c); return w; }
inline void textC(float cx, float y, const std::string& s, void* font = GLUT_BITMAP_HELVETICA_18) { text(cx - textW(s, font) / 2, y, s, font); }
// large scalable text using the stroke font (height ~ 119 * scale px)
inline float bigW(const std::string& s, float sc) { float w = 0; for (char c : s) w += glutStrokeWidth(GLUT_STROKE_ROMAN, c); return w * sc; }
inline void big(float x, float y, const std::string& s, float sc, float lw = 2.5f) {
  glPushMatrix(); glTranslatef(x, y, 0); glScalef(sc, -sc, 1); glLineWidth(lw); for (char c : s) glutStrokeCharacter(GLUT_STROKE_ROMAN, c); glPopMatrix(); }
inline void bigC(float cx, float y, const std::string& s, float sc, float lw = 2.5f) { big(cx - bigW(s, sc) / 2, y, s, sc, lw); }
inline void fillRect(float x, float y, float w, float h) { glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd(); }
inline void strokeRect(float x, float y, float w, float h, float lw = 1.5f) { glLineWidth(lw); glBegin(GL_LINE_LOOP); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd(); }
inline void line(float x0, float y0, float x1, float y1, float w = 1.5f) { glLineWidth(w); glBegin(GL_LINES); glVertex2f(x0, y0); glVertex2f(x1, y1); glEnd(); }
inline void dashed(float x0, float y0, float x1, float y1, float w = 1.5f, float dash = 8) {
  float dx = x1 - x0, dy = y1 - y0, L = std::sqrt(dx * dx + dy * dy); int n = (int)(L / dash); glLineWidth(w); glBegin(GL_LINES);
  for (int i = 0; i < n; i += 2) { float a = i / (float)n, b = std::min(1.f, (i + 1) / (float)n); glVertex2f(x0 + dx * a, y0 + dy * a); glVertex2f(x0 + dx * b, y0 + dy * b); } glEnd(); }
inline void circle(float cx, float cy, float r, bool fill = false, int seg = 40) {
  glBegin(fill ? GL_TRIANGLE_FAN : GL_LINE_LOOP); if (fill) glVertex2f(cx, cy); for (int i = 0; i <= seg; i++) { float a = 2 * cg::PI * i / seg; glVertex2f(cx + r * std::cos(a), cy + r * std::sin(a)); } glEnd(); }
inline void arrow(float x0, float y0, float x1, float y1, float w = 2.f) {
  line(x0, y0, x1, y1, w); float a = std::atan2(y1 - y0, x1 - x0); glBegin(GL_TRIANGLES);
  glVertex2f(x1, y1); glVertex2f(x1 - 12 * std::cos(a - 0.4f), y1 - 12 * std::sin(a - 0.4f)); glVertex2f(x1 - 12 * std::cos(a + 0.4f), y1 - 12 * std::sin(a + 0.4f)); glEnd(); }
inline void point(float x, float y, float sz) { glPointSize(sz); glBegin(GL_POINTS); glVertex2f(x, y); glEnd(); }
inline std::vector<std::string> wrap(const std::string& s, size_t cols) {  // word-wrap, honours '\n'
  std::vector<std::string> out; std::string line_, word; auto flushWord = [&]() { if (word.empty()) return;
    if (!line_.empty() && line_.size() + 1 + word.size() > cols) { out.push_back(line_); line_.clear(); } if (!line_.empty()) line_ += ' '; line_ += word; word.clear(); };
  for (char c : s) { if (c == '\n') { flushWord(); out.push_back(line_); line_.clear(); } else if (c == ' ') flushWord(); else word += c; }
  flushWord(); if (!line_.empty()) out.push_back(line_); return out; }
inline float block(float x, float y, const std::string& s, size_t cols, float lh, void* font = GLUT_BITMAP_HELVETICA_12) {
  for (auto& l : wrap(s, cols)) { text(x, y, l, font); y += lh; } return y; }

// ---- 3D solids (axis +Y), no dependency on freeglut-only functions ----
inline void solidCone(float r, float h, int seg = 12) {
  glBegin(GL_TRIANGLE_FAN); glNormal3f(0, 0.4f, 1); glVertex3f(0, h, 0);
  for (int i = 0; i <= seg; i++) { float a = 2 * cg::PI * i / seg; glNormal3f(std::cos(a), 0.4f, std::sin(a)); glVertex3f(r * std::cos(a), 0, r * std::sin(a)); } glEnd(); }
inline void solidCyl(float r, float h, int seg = 12) {
  glBegin(GL_QUAD_STRIP); for (int i = 0; i <= seg; i++) { float a = 2 * cg::PI * i / seg, c = std::cos(a), s = std::sin(a); glNormal3f(c, 0, s); glVertex3f(r * c, 0, r * s); glVertex3f(r * c, h, r * s); } glEnd();
  for (int k = 0; k < 2; k++) { glBegin(GL_TRIANGLE_FAN); glNormal3f(0, k ? 1.f : -1.f, 0); glVertex3f(0, k * h, 0); for (int i = 0; i <= seg; i++) { float a = 2 * cg::PI * i / seg; glVertex3f(r * std::cos(a), k * h, r * std::sin(a)); } glEnd(); } }
inline void box(float cx, float cy, float cz, float sx, float sy, float sz) { glPushMatrix(); glTranslatef(cx, cy, cz); glScalef(sx, sy, sz); glutSolidCube(1.0); glPopMatrix(); }
inline void loadMat(const cg::Mat4& m, bool mult = true) { float g[16]; m.toGL(g); if (mult) glMultMatrixf(g); else glLoadMatrixf(g); }
}  // namespace gfx

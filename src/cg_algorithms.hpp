// cg_algorithms.hpp - raster algorithms. Used by the in-game HUD AND demonstrated step by step in Learning Mode.
#pragma once
#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm>
namespace cg {
struct Pix { int x, y; };

// DDA: floating-point incremental line. steps = max(|dx|,|dy|).
inline std::vector<Pix> ddaLine(int x0, int y0, int x1, int y1) {
  std::vector<Pix> out; int dx = x1 - x0, dy = y1 - y0;
  int steps = std::max(std::abs(dx), std::abs(dy));
  if (steps == 0) { out.push_back({x0, y0}); return out; }
  float xinc = dx / (float)steps, yinc = dy / (float)steps, x = (float)x0, y = (float)y0;
  for (int i = 0; i <= steps; i++) { out.push_back({(int)std::lround(x), (int)std::lround(y)}); x += xinc; y += yinc; }
  return out;
}

// Bresenham (all octants, integer only). Used by the speedometer.
inline std::vector<Pix> bresenhamLine(int x0, int y0, int x1, int y1) {
  std::vector<Pix> out; int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) { out.push_back({x0, y0}); if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
  return out;
}

// Textbook Bresenham for x1>=x0, y1>=y0 (first octant; steep lines swap roles). Records decision parameters p_k.
inline std::vector<Pix> bresenhamTextbook(int x0, int y0, int x1, int y1, std::vector<int>* pk = nullptr) {
  std::vector<Pix> out; int dx = x1 - x0, dy = y1 - y0; bool steep = dy > dx;
  if (steep) std::swap(dx, dy);
  int p = 2 * dy - dx, x = x0, y = y0, n = dx;
  for (int i = 0; i <= n; i++) {
    out.push_back({x, y}); if (pk) pk->push_back(p);
    if (p < 0) { p += 2 * dy; if (steep) y++; else x++; }
    else { p += 2 * dy - 2 * dx; x++; y++; }
  }
  return out;
}

struct Rect { float xmin, ymin, xmax, ymax; };
enum { OC_INSIDE = 0, OC_LEFT = 1, OC_RIGHT = 2, OC_BOTTOM = 4, OC_TOP = 8 };
inline int outcode(float x, float y, const Rect& r) {
  int c = OC_INSIDE; if (x < r.xmin) c |= OC_LEFT; else if (x > r.xmax) c |= OC_RIGHT;
  if (y < r.ymin) c |= OC_BOTTOM; else if (y > r.ymax) c |= OC_TOP; return c;
}
// Cohen-Sutherland. Returns true if any part is visible; endpoints are replaced by the clipped ones.
inline bool cohenSutherland(float& x0, float& y0, float& x1, float& y1, const Rect& r, int* iterations = nullptr) {
  int c0 = outcode(x0, y0, r), c1 = outcode(x1, y1, r), it = 0; bool accept = false;
  for (;;) {
    if (!(c0 | c1)) { accept = true; break; }
    if (c0 & c1) break;
    int co = c0 ? c0 : c1; float x = 0, y = 0; it++;
    if (co & OC_TOP)         { x = x0 + (x1 - x0) * (r.ymax - y0) / (y1 - y0); y = r.ymax; }
    else if (co & OC_BOTTOM) { x = x0 + (x1 - x0) * (r.ymin - y0) / (y1 - y0); y = r.ymin; }
    else if (co & OC_RIGHT)  { y = y0 + (y1 - y0) * (r.xmax - x0) / (x1 - x0); x = r.xmax; }
    else                     { y = y0 + (y1 - y0) * (r.xmin - x0) / (x1 - x0); x = r.xmin; }
    if (co == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0, r); } else { x1 = x; y1 = y; c1 = outcode(x1, y1, r); }
  }
  if (iterations) *iterations = it;
  return accept;
}
}  // namespace cg

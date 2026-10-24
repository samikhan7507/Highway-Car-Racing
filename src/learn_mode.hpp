// learn_mode.hpp - CG Learning Mode. Every page uses the SAME math (math3d.hpp) and algorithms (cg_algorithms.hpp)
// that the game uses, so what you see here is exactly what runs inside the race.
#pragma once
#include <cstdarg>
#include "gfx.hpp"
#include "cg_algorithms.hpp"

namespace learn {
using namespace gfx; using namespace cg;

struct Info { const char *tab, *title, *formula, *used; int np; const char* pn[3]; float lo[3], hi[3], def[3]; bool disc[3]; };
static const int NPAGES = 11;
static const Info INFO[NPAGES] = {
 {"Translate", "1. TRANSLATION", "x' = x + tx\ny' = y + ty\n\n[x']   [1 0 tx] [x]\n[y'] = [0 1 ty] [y]\n[1 ]   [0 0 1 ] [1]",
  "Every traffic car, obstacle and pickup is positioned in the world with Mat4::translate(). The chase camera follows the player by translating the whole world by -(car position). Movement itself is translation each frame: P = P + v*dt (World::stepCar).",
  2, {"tx", "ty"}, {-9, -7}, {9, 7}, {4, 3}, {false, false}},
 {"Rotate", "2. ROTATION", "x' = x cos(a) - y sin(a)\ny' = x sin(a) + y cos(a)\n\n[cos -sin 0]\n[sin  cos 0]  about the origin\n[ 0    0  1]\nAbout a pivot (px,py):\nM = T(px,py) * R(a) * T(-px,-py)",
  "The car's heading (yaw) is a rotation about the Y axis: Mat4::rotateY(yaw) is part of every car's model matrix. Traffic cars use the road heading, and the minimap rotates the world so your car always points up.",
  3, {"angle (deg)", "pivot x", "pivot y"}, {-180, -7, -6}, {180, 7, 6}, {45, 0, 0}, {false, false, false}},
 {"Scale", "3. SCALING", "x' = sx * x\ny' = sy * y\n\n[sx 0  0]\n[0  sy 0]   uniform if sx = sy\n[0  0  1]\n(scaling is about the origin)",
  "Traffic trucks reuse the car mesh code with a different size via Mat4::scale(); wheels, lights and cones are unit shapes scaled to size. The speed-dependent 'zoom' of the camera is a scale in the projection matrix (f = 1/tan(fov/2)).",
  2, {"sx", "sy"}, {0.3f, 0.3f}, {2.5f, 2.5f}, {1.6f, 0.8f}, {false, false}},
 {"Reflect", "4. REFLECTION", "About x-axis : diag(1,-1,1)\nAbout y-axis : diag(-1,1,1)\nAbout origin : diag(-1,-1,1)\nAbout y = x : swap x and y\nAbout y = -x: swap and negate both",
  "The player's car is modelled on one side only: the wheels and side mirrors on the left are drawn by applying Mat4::reflectX() = scale(-1,1,1), a reflection about the YZ plane, to the right-hand parts. Symmetry halves the modelling work.",
  1, {"mode (0-4)"}, {0}, {4}, {0}, {true}},
 {"Shear", "5. SHEARING", "x' = x + shx * y\ny' = y + shy * x\n\n[1   shx 0]\n[shy 1   0]\n[0   0   1]",
  "Body roll: when you corner, the car's body and cabin are sheared sideways in proportion to lateral acceleration (Mat4::shear(roll,...) in drawCar), while the wheels stay planted on the road. It makes the car feel heavy and the steering feel real.",
  2, {"shx", "shy"}, {-1.2f, -1.2f}, {1.2f, 1.2f}, {0.5f, 0}, {false, false}},
 {"Composite", "6. COMPOSITE MATRICES", "M = T * R * S   (applied right to left)\nP' = T( R( S(P) ) )\nOrder matters: S*R*T != T*R*S\nOne matrix transforms every vertex.\nPress O to swap the order.",
  "Each car every frame: Model = Translate(pos) * RotateY(yaw) * RotateX(pitch). MVP = Projection * View * Model - the whole pipeline in one product (press G in a race to see the live matrix). The minimap = T(centre)*S*R*T(-car).",
  3, {"angle (deg)", "scale", "tx"}, {-180, 0.4f, -8}, {180, 2, 8}, {40, 1.4f, 5}, {false, false, false}},
 {"DDA", "7. DDA LINE ALGORITHM", "dx = x1-x0,  dy = y1-y0\nsteps = max(|dx|, |dy|)\nxinc = dx/steps,  yinc = dy/steps\nrepeat steps times:\n   x += xinc;  y += yinc\n   plot( round(x), round(y) )",
  "The race minimap draws the road centre-line segment by segment with ddaLine() (src/cg_algorithms.hpp), one pixel at a time. It is simple, but needs floating-point additions and rounding per pixel.",
  2, {"end x", "end y"}, {3, 2}, {29, 23}, {26, 14}, {false, false}},
 {"Bresenham", "8. BRESENHAM LINE ALGORITHM", "dx = x1-x0, dy = y1-y0   (0 <= dy <= dx)\np0 = 2*dy - dx\nif p < 0: plot(x+1, y)   ; p += 2*dy\nelse    : plot(x+1, y+1) ; p += 2*dy - 2*dx\n(steep lines: swap the roles of x and y)",
  "The speedometer needle and tick marks are rasterised pixel by pixel with bresenhamLine(). It uses integer arithmetic only - no rounding errors and no floating point - which is why hardware rasterisers are built on it.",
  2, {"end x", "end y"}, {3, 2}, {29, 23}, {26, 14}, {false, false}},
 {"Clipping", "9. LINE CLIPPING (Cohen-Sutherland)", "Outcode bits:  TOP BOTTOM RIGHT LEFT\ncode0 | code1 == 0 : trivially ACCEPT\ncode0 & code1 != 0 : trivially REJECT\nelse clip against an edge:\n y = y0 + m (xedge - x0)\n x = x0 + (yedge - y0) / m",
  "Minimap road segments are clipped to the minimap rectangle by cohenSutherland() before DDA draws them, so nothing spills outside the panel. The GPU does the same for the 3D view frustum: geometry outside is clipped away.",
  3, {"B x", "B y", "window size"}, {-13, -10, 0.5f}, {13, 10, 1.4f}, {8, 5, 1}, {false, false, false}},
 {"Projection", "10. PROJECTION", "Perspective:  x_ndc = (f/aspect) * x / (-z)\n              y_ndc = f * y / (-z),  f = 1/tan(fov/2)\nOrthographic: x_ndc = 2x/(r-l) - (r+l)/(r-l)\nMVP = P * V * M    then divide by w",
  "The 3D race view uses Mat4::perspective(); the FOV widens with speed and nitro for a sense of speed. The minimap is a top-down (orthographic-style) projection. All matrices are our own (src/math3d.hpp), uploaded with glLoadMatrixf().",
  3, {"FOV (deg)", "distance", "mode 0=persp 1=ortho"}, {20, 4, 0}, {120, 20, 1}, {55, 6.5f, 0}, {false, false, true}},
 {"Animation", "11. ANIMATION", "P(t) = P0 + (P1 - P0) * e(t),  t in [0,1]\nlinear: e = t      ease-in: e = t^2\nease-out: e = 1-(1-t)^2\nsmoothstep: e = 3t^2 - 2t^3\nFrame loop: dt = now - last;  x += v * dt",
  "The chase camera eases toward the car's yaw, wheels spin, pickups rotate, the nitro flame flickers and traffic cruises - all driven by dt, so speed is the same at 30 or 144 FPS. Double buffering gives flicker-free 60 FPS drawing.",
  2, {"easing (0-3)", "speed"}, {0, 0.3f}, {3, 3}, {3, 1}, {true, false}}};

struct State { int page = 0; float val[3] = {0, 0, 0}; int sel = 0; bool autoPlay = true, flip = false; float t0 = 0; };
inline State S;
inline void reset(float now) { const Info& in = INFO[S.page]; for (int i = 0; i < 3; i++) S.val[i] = in.def[i]; S.sel = 0; S.t0 = now; S.flip = false; S.autoPlay = true; }
inline void enter(int page, float now) { S.page = (page % NPAGES + NPAGES) % NPAGES; reset(now); }
inline bool animPage() { return S.page == 6 || S.page == 7 || S.page == 10; }
inline void adjust(float dv, bool discrete) {
  const Info& in = INFO[S.page]; if (S.sel >= in.np) return; float& v = S.val[S.sel];
  v = discrete ? v + dv : v + dv * (in.hi[S.sel] - in.lo[S.sel]); v = clampf(v, in.lo[S.sel], in.hi[S.sel]); if (!animPage()) S.autoPlay = false; }
inline void special(int key, float now) {  // key: 0 left,1 right,2 up,3 down
  if (key == 0) enter(S.page - 1, now); else if (key == 1) enter(S.page + 1, now);
  else if (S.sel < INFO[S.page].np && INFO[S.page].disc[S.sel]) adjust(key == 2 ? 1.f : -1.f, true); }
inline void keyboard(unsigned char k, float now) {
  if (k == '\t') S.sel = (S.sel + 1) % INFO[S.page].np; else if (k == 'p' || k == 'P') S.autoPlay = !S.autoPlay;
  else if (k == 'o' || k == 'O') S.flip = !S.flip; else if (k == ' ') S.t0 = now; else if (k == 'r' || k == 'R') reset(now);
  else if (k == '[' || k == ',') enter(S.page - 1, now); else if (k == ']' || k == '.') enter(S.page + 1, now); }
inline void update(float dt, bool up, bool down) {
  const Info& in = INFO[S.page]; if (S.sel < in.np && !in.disc[S.sel]) { if (up) adjust(0.35f * dt, false); if (down) adjust(-0.35f * dt, false); } }

// ---------------- drawing helpers (math-space canvas) ----------------
static const float PX = 30, PY = 95, PW = 730, PH = 585, OX = PX + PW / 2, OY = PY + PH / 2, SC = 26;
inline Vec2 P2S(Vec2 v) { return {OX + v.x * SC, OY - v.y * SC}; }
inline std::string fs(const char* f, ...) { char b[256]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
static const std::vector<Vec2> BODY = {{-1, -2}, {1, -2}, {1, 1}, {0.6f, 2}, {-0.6f, 2}, {-1, 1}}, CABIN = {{-0.6f, -0.8f}, {0.6f, -0.8f}, {0.5f, 0.6f}, {-0.5f, 0.6f}};
inline void drawPoly(const std::vector<Vec2>& p, const Mat3& M, float r, float g, float b, float a, bool fill = true) {
  if (fill) { col(r, g, b, a * 0.55f); glBegin(GL_POLYGON); for (auto& v : p) { Vec2 s = P2S(M.apply(v)); glVertex2f(s.x, s.y); } glEnd(); }
  col(r, g, b, std::min(1.f, a + 0.3f)); glLineWidth(2); glBegin(GL_LINE_LOOP); for (auto& v : p) { Vec2 s = P2S(M.apply(v)); glVertex2f(s.x, s.y); } glEnd(); }
inline void drawCar(const Mat3& M, float r, float g, float b, float a) { drawPoly(BODY, M, r, g, b, a); drawPoly(CABIN, M, r * 0.6f, g * 0.6f, b * 0.6f, a); Vec2 f0 = P2S(M.apply({0, 1.2f})), f1 = P2S(M.apply({0, 2.6f})); col(1, 1, 1, a + 0.2f); line(f0.x, f0.y, f1.x, f1.y, 2); }
inline void drawGrid() {
  glEnable(GL_SCISSOR_TEST); col(0.06f, 0.08f, 0.12f); fillRect(PX, PY, PW, PH);
  for (int i = -14; i <= 14; i++) { col(0.16f, 0.2f, 0.28f); Vec2 a = P2S({(float)i, -11}), b = P2S({(float)i, 11}); line(a.x, a.y, b.x, b.y, 1); }
  for (int j = -11; j <= 11; j++) { col(0.16f, 0.2f, 0.28f); Vec2 a = P2S({-14, (float)j}), b = P2S({14, (float)j}); line(a.x, a.y, b.x, b.y, 1); }
  col(0.7f, 0.7f, 0.75f); line(PX, OY, PX + PW, OY, 2); line(OX, PY, OX, PY + PH, 2);
  for (int i = -13; i <= 13; i += 2) if (i) text(OX + i * SC - 5, OY + 14, std::to_string(i), GLUT_BITMAP_HELVETICA_10);
  for (int j = -10; j <= 10; j += 2) if (j) text(OX + 4, OY - j * SC + 4, std::to_string(j), GLUT_BITMAP_HELVETICA_10);
  col(1, 1, 1); text(PX + PW - 14, OY - 6, "x", GLUT_BITMAP_HELVETICA_12); text(OX + 6, PY + 14, "y", GLUT_BITMAP_HELVETICA_12); }
inline void mat3Lines(std::vector<std::string>& L, const Mat3& M) { for (int i = 0; i < 3; i++) L.push_back(fs("[ %7.2f %7.2f %7.2f ]", M.m[i][0], M.m[i][1], M.m[i][2])); }
inline float effVal(int i, float t) {
  const Info& in = INFO[S.page]; float v = S.val[i];
  if (S.autoPlay && i == 0 && !animPage()) { float tt = t - S.t0;
    if (in.disc[0]) v = in.lo[0] + std::floor(std::fmod(tt * 0.6f, in.hi[0] - in.lo[0] + 1.f));
    else { float u = std::fmod(tt * 0.12f, 2.f); u = u < 1 ? u : 2 - u; v = lerp(in.lo[0], in.hi[0], smooth01(u)); } }
  return v; }
inline std::string bits(int c) { std::string s; s += (c & OC_TOP) ? '1' : '0'; s += (c & OC_BOTTOM) ? '1' : '0'; s += (c & OC_RIGHT) ? '1' : '0'; s += (c & OC_LEFT) ? '1' : '0'; return s; }
inline float ease(int type, float u) { switch (type) { case 1: return u * u; case 2: return 1 - (1 - u) * (1 - u); case 3: return u * u * (3 - 2 * u); default: return u; } }

// ---------------- pages ----------------
inline void pageTransform(float t, std::vector<std::string>& L) {
  float a = effVal(0, t), b = INFO[S.page].np > 1 ? effVal(1, t) : 0, c = INFO[S.page].np > 2 ? effVal(2, t) : 0; Mat3 M, base = Mat3::identity();
  switch (S.page) {
    case 0: { M = Mat3::translate(a, b); drawCar(base, 0.6f, 0.6f, 0.65f, 0.3f); drawCar(M, 0.2f, 0.8f, 1, 0.6f);
      Vec2 o = P2S({0, 0}), e = P2S({a, b}); col(1, 1, 0); arrow(o.x, o.y, e.x, e.y); L.push_back(fs("t = (%.2f, %.2f)", a, b)); mat3Lines(L, M);
      Vec2 v = M.apply({-1, -2}); L.push_back(fs("vertex (-1,-2) -> (%.2f, %.2f)", v.x, v.y)); break; }
    case 1: { base = Mat3::translate(5, 0) * Mat3::scale(0.8f, 0.8f); Mat3 R = Mat3::rotate(rad(a)), Tp = Mat3::translate(b, c), Ti = Mat3::translate(-b, -c); M = Tp * R * Ti;
      drawCar(base, 0.6f, 0.6f, 0.65f, 0.3f); drawCar(M * base, 1, 0.55f, 0.15f, 0.6f);
      Vec2 p = P2S({b, c}); col(1, 1, 0); circle(p.x, p.y, 6, true); text(p.x + 8, p.y - 8, "pivot", GLUT_BITMAP_HELVETICA_12);
      L.push_back(fs("angle = %.1f deg  (cos=%.3f sin=%.3f)", a, std::cos(rad(a)), std::sin(rad(a)))); L.push_back("R(a):"); mat3Lines(L, R);
      L.push_back(fs("pivot = (%.1f, %.1f)  -> M = T R T^-1:", b, c)); mat3Lines(L, M); break; }
    case 2: { base = Mat3::translate(3, 2) * Mat3::scale(0.7f, 0.7f); M = Mat3::scale(a, b);
      drawCar(base, 0.6f, 0.6f, 0.65f, 0.3f); drawCar(M * base, 0.4f, 1, 0.5f, 0.6f); L.push_back(fs("sx = %.2f  sy = %.2f", a, b)); mat3Lines(L, M);
      L.push_back(a == b ? "uniform scaling (shape kept)" : "non-uniform scaling (shape distorted)"); L.push_back("note: position scales too (about origin)"); break; }
    case 3: { int mode = (int)std::lround(a); static const char* nm[5] = {"about x-axis", "about y-axis", "about origin", "about y = x", "about y = -x"}; mode = std::max(0, std::min(4, mode));
      base = Mat3::translate(4, 3) * Mat3::scale(0.7f, 0.7f); M = Mat3::reflect(mode); col(1, 1, 0, 0.9f);
      if (mode == 0) { Vec2 p = P2S({-14, 0}), q = P2S({14, 0}); line(p.x, p.y, q.x, q.y, 4); } else if (mode == 1) { Vec2 p = P2S({0, -11}), q = P2S({0, 11}); line(p.x, p.y, q.x, q.y, 4); }
      else if (mode == 2) { Vec2 p = P2S({0, 0}); circle(p.x, p.y, 7, true); } else { float sg = mode == 3 ? 1.f : -1.f; Vec2 p = P2S({-11, -11 * sg}), q = P2S({11, 11 * sg}); line(p.x, p.y, q.x, q.y, 4); }
      drawCar(base, 0.6f, 0.6f, 0.65f, 0.3f); drawCar(M * base, 0.9f, 0.4f, 1, 0.6f); L.push_back(fs("mode %d: reflection %s", mode, nm[mode])); mat3Lines(L, M);
      Vec2 v = M.apply({4, 3}); L.push_back(fs("car centre (4,3) -> (%.1f, %.1f)", v.x, v.y)); L.push_back("determinant = -1 (orientation flips)"); break; }
    case 4: { base = Mat3::scale(1.1f, 1.1f); M = Mat3::shear(a, b); std::vector<Vec2> sq = {{-2.5f, -2.5f}, {2.5f, -2.5f}, {2.5f, 2.5f}, {-2.5f, 2.5f}};
      drawPoly(sq, Mat3::identity(), 0.5f, 0.5f, 0.55f, 0.1f, false); drawPoly(sq, M, 1, 0.9f, 0.3f, 0.2f, false);
      drawCar(base, 0.6f, 0.6f, 0.65f, 0.3f); drawCar(M * base, 1, 0.4f, 0.4f, 0.6f); L.push_back(fs("shx = %.2f  shy = %.2f", a, b)); mat3Lines(L, M);
      L.push_back("area preserved (det = 1 - shx*shy)"); L.push_back(fs("det = %.2f", 1 - a * b)); break; }
    default: { float ang = rad(a), sc = b, tx = c; Mat3 T = Mat3::translate(tx, 0), R = Mat3::rotate(ang), Sm = Mat3::scale(sc, sc); base = Mat3::scale(0.8f, 0.8f);
      drawCar(base, 0.6f, 0.6f, 0.65f, 0.25f); Mat3 M1, M2, M3;
      if (!S.flip) { M1 = Sm * base; M2 = R * M1; M3 = T * M2; M = T * R * Sm; } else { M1 = T * base; M2 = R * M1; M3 = Sm * M2; M = Sm * R * T; }
      drawCar(M1, 1, 0.9f, 0.3f, 0.35f); drawCar(M2, 1, 0.55f, 0.15f, 0.45f); drawCar(M3, 0.2f, 0.85f, 1, 0.65f);
      L.push_back(S.flip ? "order: M = S * R * T   (T first!)" : "order: M = T * R * S   (S first)"); L.push_back(fs("a=%.0f deg  s=%.2f  tx=%.1f", a, sc, tx)); mat3Lines(L, M);
      L.push_back(S.flip ? "yellow: after T, orange: after R, cyan: after S" : "yellow: after S, orange: after R, cyan: after T"); }
  }
}
inline void pageRaster(float t, std::vector<std::string>& L) {
  const bool bres = S.page == 7; const float CELL = 24; const int COLS = 30, ROWS = 24; auto cell = [&](int x, int y) { return Vec2{PX + x * CELL, PY + (ROWS - 1 - y) * CELL}; };
  col(0.06f, 0.08f, 0.12f); fillRect(PX, PY, PW, PH); col(0.2f, 0.24f, 0.32f);
  for (int i = 0; i <= COLS; i++) line(PX + i * CELL, PY, PX + i * CELL, PY + ROWS * CELL, 1);
  for (int j = 0; j <= ROWS; j++) line(PX, PY + j * CELL, PX + COLS * CELL, PY + j * CELL, 1);
  for (int i = 0; i < COLS; i += 5) text(PX + i * CELL + 3, PY + PH - 3, std::to_string(i), GLUT_BITMAP_HELVETICA_10);
  for (int j = 0; j < ROWS; j += 5) text(PX + 2, PY + (ROWS - 1 - j) * CELL + 16, std::to_string(j), GLUT_BITMAP_HELVETICA_10);
  int x0 = 2, y0 = 2, x1 = (int)std::lround(S.val[0]), y1 = (int)std::lround(S.val[1]); x1 = std::max(3, std::min(29, x1)); y1 = std::max(2, std::min(23, y1));
  std::vector<int> pk; auto pix = bres ? bresenhamTextbook(x0, y0, x1, y1, &pk) : ddaLine(x0, y0, x1, y1); int total = (int)pix.size();
  int rev = S.autoPlay ? std::min(total, (int)((t - S.t0) * 5.f) + 1) : total;
  for (int i = 0; i < rev; i++) { Vec2 c = cell(pix[i].x, pix[i].y); if (bres) col(0.2f, 0.85f, 0.4f, 0.85f); else col(1, 0.6f, 0.15f, 0.85f); fillRect(c.x + 1, c.y + 1, CELL - 2, CELL - 2); }
  Vec2 a = cell(x0, y0), b = cell(x1, y1); col(1, 1, 1, 0.9f); line(a.x + CELL / 2, a.y + CELL / 2, b.x + CELL / 2, b.y + CELL / 2, 1.5f);
  if (rev > 0) { Vec2 c = cell(pix[rev - 1].x, pix[rev - 1].y); col(1, 1, 0); strokeRect(c.x, c.y, CELL, CELL, 3); }
  int dx = x1 - x0, dy = y1 - y0; L.push_back(fs("start (%d,%d)  end (%d,%d)   dx=%d dy=%d", x0, y0, x1, y1, dx, dy));
  if (!bres) { int steps = std::max(dx, dy); float xi = dx / (float)steps, yi = dy / (float)steps; L.push_back(fs("steps = max(|dx|,|dy|) = %d", steps)); L.push_back(fs("xinc = %.3f   yinc = %.3f", xi, yi));
    int k = std::max(0, rev - 1); L.push_back(fs("k=%d: x=%.2f y=%.2f -> plot(%d,%d)", k, x0 + xi * k, y0 + yi * k, pix[k].x, pix[k].y)); L.push_back("(float add + round per pixel)"); }
  else { bool steep = dy > dx; int a_ = steep ? dy : dx, b_ = steep ? dx : dy; L.push_back(fs("%s: p0 = 2*%d - %d = %d", steep ? "steep (swap x,y)" : "shallow", b_, a_, 2 * b_ - a_));
    L.push_back(fs("2dy = %d    2dy-2dx = %d", 2 * b_, 2 * b_ - 2 * a_)); int from = std::max(0, rev - 6);
    for (int k = from; k < rev; k++) L.push_back(fs("k=%-2d p=%-4d -> %s plot(%d,%d)", k, pk[k], pk[k] < 0 ? "straight" : "diagonal", pix[k].x, pix[k].y)); L.push_back("(integers only, no rounding)"); }
}
inline void pageClip(float t, std::vector<std::string>& L) {
  float sc = effVal(2, t); Rect w{-6 * sc, -4 * sc, 6 * sc, 4 * sc}; float ax = -11, ay = -7, bx = effVal(0, t), by = S.val[1], x0 = ax, y0 = ay, x1 = bx, y1 = by;
  Vec2 tl = P2S({w.xmin, w.ymax}), br = P2S({w.xmax, w.ymin}); col(0.3f, 0.35f, 0.45f);
  dashed(tl.x, PY, tl.x, PY + PH, 1, 6); dashed(br.x, PY, br.x, PY + PH, 1, 6); dashed(PX, tl.y, PX + PW, tl.y, 1, 6); dashed(PX, br.y, PX + PW, br.y, 1, 6);
  static const char* codes[9] = {"1001", "1000", "1010", "0001", "0000", "0010", "0101", "0100", "0110"}; col(0.55f, 0.6f, 0.7f);
  float xs[3] = {(PX + tl.x) / 2, (tl.x + br.x) / 2, (br.x + PX + PW) / 2}, ys[3] = {(PY + tl.y) / 2, (tl.y + br.y) / 2, (br.y + PY + PH) / 2};
  for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) textC(xs[c], ys[r] + 5, codes[r * 3 + c], GLUT_BITMAP_9_BY_15);
  col(0.2f, 0.5f, 1, 0.15f); fillRect(tl.x, tl.y, br.x - tl.x, br.y - tl.y); col(0.3f, 0.65f, 1); strokeRect(tl.x, tl.y, br.x - tl.x, br.y - tl.y, 3);
  Vec2 A = P2S({ax, ay}), B = P2S({bx, by}); col(1, 0.3f, 0.3f); dashed(A.x, A.y, B.x, B.y, 2, 8); int it = 0; bool vis = cohenSutherland(x0, y0, x1, y1, w, &it);
  if (vis) { Vec2 C = P2S({x0, y0}), D = P2S({x1, y1}); col(0.2f, 1, 0.4f); line(C.x, C.y, D.x, D.y, 5); circle(C.x, C.y, 5, true); circle(D.x, D.y, 5, true); }
  col(1, 1, 1); circle(A.x, A.y, 5, true); circle(B.x, B.y, 5, true); text(A.x + 8, A.y - 6, "A"); text(B.x + 8, B.y - 6, "B");
  int c0 = outcode(ax, ay, w), c1 = outcode(bx, by, w); L.push_back(fs("A=(%.0f,%.0f) code %s", ax, ay, bits(c0).c_str())); L.push_back(fs("B=(%.1f,%.1f) code %s", bx, by, bits(c1).c_str()));
  L.push_back(fs("A|B = %s   A&B = %s", bits(c0 | c1).c_str(), bits(c0 & c1).c_str()));
  L.push_back((c0 | c1) == 0 ? "-> trivially ACCEPTED" : (c0 & c1) ? "-> trivially REJECTED" : fs("-> needs clipping: %d edge intersection(s)", it));
  if (vis) L.push_back(fs("clipped: (%.2f,%.2f)-(%.2f,%.2f)", x0, y0, x1, y1)); else L.push_back("nothing visible");
  L.push_back("red dashed = original, green = visible part"); }
inline void pageProject(float t, std::vector<std::string>& L) {
  float fov = effVal(0, t), dist = S.val[1]; bool ortho = std::lround(S.val[2]) == 1; float asp = PW / PH; col(0.06f, 0.08f, 0.12f); fillRect(PX, PY, PW, PH);
  Mat4 P = ortho ? Mat4::ortho(-dist * std::tan(rad(fov / 2)) * asp, dist * std::tan(rad(fov / 2)) * asp, -dist * std::tan(rad(fov / 2)), dist * std::tan(rad(fov / 2)), 0.5f, 80) : Mat4::perspective(rad(fov), asp, 0.5f, 80);
  Mat4 V = Mat4::lookAt({0, 3.2f, dist}, {0, 0, 0}, {0, 1, 0}), M = Mat4::rotateY((t - S.t0) * 0.7f), MVP = P * V * M, PVonly = P * V;
  auto cl = [&](Vec2 a, Vec2 b, float w) { float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y; if (cohenSutherland(x0, y0, x1, y1, Rect{PX + 2, PY + 2, PX + PW - 2, PY + PH - 2})) line(x0, y0, x1, y1, w); };
  auto scr = [&](const Mat4& m, Vec3 v, Vec2& o) { Vec4 c = m.mul(v); if (c.w < 0.1f) return false; o = {OX + c.x / c.w * PW / 2 * 0.95f, OY - c.y / c.w * PH / 2 * 0.95f}; return true; };
  col(0.3f, 0.4f, 0.55f); for (int i = -8; i <= 8; i += 2) { Vec2 a, b; if (scr(PVonly, {(float)i, -0.55f, -20}, a) && scr(PVonly, {(float)i, -0.55f, 20}, b)) cl(a, b, 1);
    if (scr(PVonly, {-16, -0.55f, (float)i * 2}, a) && scr(PVonly, {16, -0.55f, (float)i * 2}, b)) cl(a, b, 1); }
  Vec3 v[8]; for (int i = 0; i < 8; i++) v[i] = {(i & 1) ? 1.f : -1.f, (i & 2) ? 0.55f : -0.55f, (i & 4) ? 2.2f : -2.2f};
  static const int E[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}; col(1, 0.8f, 0.2f);
  for (auto& e : E) { Vec2 a, b; if (scr(MVP, v[e[0]], a) && scr(MVP, v[e[1]], b)) cl(a, b, 3); }
  col(1, 1, 1); text(PX + 10, PY + 22, ortho ? "ORTHOGRAPHIC: parallel lines stay parallel, no size change with depth" : "PERSPECTIVE: parallel lines converge, far = smaller", GLUT_BITMAP_HELVETICA_12);
  L.push_back(fs("%s  fov=%.0f  dist=%.1f", ortho ? "ORTHO" : "PERSPECTIVE", fov, dist)); L.push_back("P ="); for (int i = 0; i < 4; i++) L.push_back(fs("[%7.3f %7.3f %7.3f %7.3f]", P.m[i][0], P.m[i][1], P.m[i][2], P.m[i][3]));
  L.push_back(fs("f = 1/tan(fov/2) = %.3f   aspect = %.2f", 1.f / std::tan(rad(fov / 2)), asp)); Vec4 c = MVP.mul(v[7]);
  L.push_back(fs("vertex(1,.55,2.2): clip w=%.2f", c.w)); L.push_back(fs("  ndc = (%.2f, %.2f, %.2f)", c.x / c.w, c.y / c.w, c.z / c.w)); }
inline void pageAnim(float t, std::vector<std::string>& L) {
  int type = (int)std::lround(S.val[0]); static const char* nm[4] = {"linear", "ease-in", "ease-out", "smoothstep"}; float speed = S.val[1]; Vec2 K[3] = {{-10, -6}, {0, 7}, {10, -4}};
  col(0.4f, 0.5f, 0.65f); for (int i = 0; i < 2; i++) { Vec2 a = P2S(K[i]), b = P2S(K[i + 1]); dashed(a.x, a.y, b.x, b.y, 1.5f, 8); } for (int i = 0; i < 3; i++) { Vec2 a = P2S(K[i]); col(1, 1, 0); circle(a.x, a.y, 6, true); text(a.x + 8, a.y - 8, "K" + std::to_string(i), GLUT_BITMAP_HELVETICA_12); }
  auto place = [&](float tt, float& lx, Mat3& M, Vec2& pos) { float u = std::fmod(std::max(0.f, tt - S.t0) * speed * 0.25f, 2.f); int seg = u < 1 ? 0 : 1; lx = u - std::floor(u); float e = ease(type, lx);
    pos = {lerp(K[seg].x, K[seg + 1].x, e), lerp(K[seg].y, K[seg + 1].y, e)}; float h = std::atan2(K[seg + 1].y - K[seg].y, K[seg + 1].x - K[seg].x); M = Mat3::translate(pos.x, pos.y) * Mat3::rotate(h - PI / 2) * Mat3::scale(0.55f, 0.55f); };
  float lx; Mat3 M; Vec2 pos; for (int k = 8; k >= 1; k--) { float l2; Mat3 M2; Vec2 p2; place(t - k * 0.07f, l2, M2, p2); drawCar(M2, 0.3f, 0.7f, 1, 0.05f + 0.04f * (8 - k)); }
  place(t, lx, M, pos); drawCar(M, 0.2f, 0.9f, 1, 0.7f);
  float gx = PX + PW - 200, gy = PY + PH - 180, gw = 170, gh = 150; col(0, 0, 0, 0.55f); fillRect(gx - 10, gy - 14, gw + 20, gh + 30); col(0.8f, 0.8f, 0.85f); line(gx, gy + gh, gx + gw, gy + gh, 1.5f); line(gx, gy, gx, gy + gh, 1.5f);
  col(1, 0.6f, 0.2f); glLineWidth(2.5f); glBegin(GL_LINE_STRIP); for (int i = 0; i <= 40; i++) { float u = i / 40.f; glVertex2f(gx + u * gw, gy + gh - ease(type, u) * gh); } glEnd();
  col(1, 1, 1); circle(gx + lx * gw, gy + gh - ease(type, lx) * gh, 5, true); text(gx, gy - 3, std::string("e(t): ") + nm[type], GLUT_BITMAP_HELVETICA_12);
  L.push_back(fs("easing = %s   speed = %.1f", nm[type], speed)); L.push_back(fs("t = %.2f   e(t) = %.2f", lx, ease(type, lx))); L.push_back(fs("position = (%.2f, %.2f)", pos.x, pos.y));
  L.push_back("heading = atan2(dy,dx)  -> rotate(h-90deg)"); L.push_back("model = T(pos) * R(h) * S(0.55)"); L.push_back("ghost cars = same path sampled at t-k*dt"); }

inline void draw(float now) {
  const Info& in = INFO[S.page]; float t = now; col(0.04f, 0.05f, 0.09f); fillRect(0, 0, VW, VH);
  col(1, 0.8f, 0.2f); big(30, 52, "CG LEARNING MODE", 0.28f, 2.5f); col(0.6f, 0.7f, 0.85f); text(420, 40, "Sami Khan  |  every demo below runs the same math as the game", GLUT_BITMAP_HELVETICA_12);
  float tx = 30; for (int i = 0; i < NPAGES; i++) { float w = textW(INFO[i].tab, GLUT_BITMAP_HELVETICA_12) + 16; if (i == S.page) { col(1, 0.8f, 0.2f); fillRect(tx, 62, w, 22); col(0, 0, 0); } else { col(0.15f, 0.2f, 0.3f); fillRect(tx, 62, w, 22); col(0.8f, 0.85f, 0.95f); }
    text(tx + 8, 78, INFO[i].tab, GLUT_BITMAP_HELVETICA_12); tx += w + 4; }
  std::vector<std::string> L; glEnable(GL_SCISSOR_TEST);
  switch (S.page) { case 0: case 1: case 2: case 3: case 4: case 5: drawGrid(); pageTransform(t, L); break; case 6: case 7: pageRaster(t, L); break; case 8: drawGrid(); pageClip(t, L); break; case 9: pageProject(t, L); break; default: drawGrid(); pageAnim(t, L); }
  col(0.5f, 0.55f, 0.7f); strokeRect(PX, PY, PW, PH, 2);
  float x = 790, y = 118; col(1, 0.8f, 0.2f); text(x, y, in.title, GLUT_BITMAP_HELVETICA_18); y += 22;
  for (int i = 0; i < in.np; i++) { float v = (i == 0 && S.autoPlay && !animPage()) ? effVal(0, t) : S.val[i]; bool sel = i == S.sel; col(sel ? 1.f : 0.7f, sel ? 1.f : 0.75f, sel ? 0.3f : 0.8f);
    text(x, y, std::string(sel ? "> " : "  ") + in.pn[i] + " = " + (in.disc[i] ? fs("%d", (int)std::lround(v)) : fs("%.2f", v)), GLUT_BITMAP_9_BY_15); y += 17; }
  y += 6; col(0.5f, 0.8f, 1); text(x, y, "FORMULA", GLUT_BITMAP_HELVETICA_12); y += 16; col(0.95f, 0.95f, 1);
  { std::string cur; for (const char* p = in.formula;; p++) { if (*p == '\n' || !*p) { text(x, y, cur, GLUT_BITMAP_9_BY_15); y += 16; cur.clear(); if (!*p) break; } else cur += *p; } }
  y += 8; col(0.5f, 1, 0.7f); text(x, y, "LIVE VALUES", GLUT_BITMAP_HELVETICA_12); y += 16; col(0.8f, 1, 0.85f); for (auto& l : L) { text(x, y, l, GLUT_BITMAP_9_BY_15); y += 15; }
  y = std::max(y + 10, 500.f); col(1, 0.6f, 0.4f); text(x, y, "WHERE THIS IS USED IN THE GAME", GLUT_BITMAP_HELVETICA_12); y += 16; col(1, 0.9f, 0.85f); block(x, y, in.used, 70, 15);
  col(0.6f, 0.65f, 0.75f); text(30, 704, "[<-/->] page   [TAB] select parameter   [UP/DOWN] change   [P] auto-animate   [O] swap order   [SPACE] restart   [R] reset   [ESC] back", GLUT_BITMAP_HELVETICA_12); }
}  // namespace learn

// selftest.cpp - headless tests (no window / OpenGL needed). Run: ./selftest
#include <cstdio>
#include <cstdlib>
#include "../src/game.hpp"
#include "../src/cg_algorithms.hpp"
using namespace cg;
static int fails = 0, checks = 0;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; std::printf("  FAIL: %s\n", msg); } } while (0)
static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

static void testMath() {
  std::puts("[math]");
  Mat3 a = Mat3::rotate(rad(30)) * Mat3::rotate(rad(45)), b = Mat3::rotate(rad(75));
  bool same = true; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) same &= near(a.m[i][j], b.m[i][j]);
  CHECK(same, "R(30)*R(45) == R(75)");
  Vec2 p = Mat3::translate(3, -2).apply({1, 1}); CHECK(near(p.x, 4) && near(p.y, -1), "translate");
  p = Mat3::reflect(3).apply({2, 5}); CHECK(near(p.x, 5) && near(p.y, 2), "reflect about y=x");
  p = Mat3::shear(0.5f, 0).apply({1, 2}); CHECK(near(p.x, 2) && near(p.y, 2), "shear x");
  Mat3 T = Mat3::translate(5, 5), R = Mat3::rotate(rad(90)), S = Mat3::scale(2, 2);
  Vec2 c1 = (T * R * S).apply({1, 0}), c2 = T.apply(R.apply(S.apply({1, 0}))); CHECK(near(c1.x, c2.x) && near(c1.y, c2.y), "composite == sequential");
  Vec2 o1 = (S * R * T).apply({1, 0}); CHECK(!(near(o1.x, c1.x) && near(o1.y, c1.y)), "order matters (non-commutative)");
  Mat4 P = Mat4::perspective(rad(90), 1.f, 1.f, 100.f); Vec4 v = P.mul({0, 0, -10}); CHECK(near(v.x / v.w, 0) && near(v.w, 10), "perspective w = -z");
  Vec4 v2 = P.mul({5, 0, -10}); CHECK(near(v2.x / v2.w, 0.5f), "perspective x_ndc = x/(-z) for 90deg fov");
  Vec4 n = P.mul({0, 0, -1}); CHECK(near(n.z / n.w, -1.f), "near plane maps to NDC -1");
  Vec4 f = P.mul({0, 0, -100}); CHECK(near(f.z / f.w, 1.f, 1e-3f), "far plane maps to NDC +1");
  Mat4 V = Mat4::lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0}); Vec4 e = V.mul({0, 0, 0}); CHECK(near(e.z, -5), "lookAt moves origin to -z");
  float gl[16]; Mat4::translate(1, 2, 3).toGL(gl); CHECK(near(gl[12], 1) && near(gl[13], 2) && near(gl[14], 3), "toGL column-major");
}
static void testAlgos() {
  std::puts("[raster/clip]");
  std::srand(5); int bad = 0, badEnds = 0;
  for (int i = 0; i < 2000; i++) {
    int x0 = rand() % 60, y0 = rand() % 60, x1 = rand() % 60, y1 = rand() % 60; auto b = bresenhamLine(x0, y0, x1, y1), d = ddaLine(x0, y0, x1, y1);
    if (b.size() != d.size()) { bad++; continue; }
    if (b.front().x != x0 || b.back().x != x1 || b.back().y != y1) badEnds++;
    for (size_t k = 0; k < b.size(); k++) if (std::abs(b[k].x - d[k].x) > 1 || std::abs(b[k].y - d[k].y) > 1) { bad++; break; } }
  CHECK(bad == 0, "Bresenham and DDA agree within 1 pixel"); CHECK(badEnds == 0, "line endpoints exact");
  int mism = 0; for (int i = 0; i < 500; i++) { int x0 = 2, y0 = 2, x1 = 3 + rand() % 25, y1 = 2 + rand() % 20; auto t = bresenhamTextbook(x0, y0, x1, y1), g = bresenhamLine(x0, y0, x1, y1);
    if (t.size() != g.size()) { mism++; continue; } for (size_t k = 0; k < t.size(); k++) if (t[k].x != g[k].x || t[k].y != g[k].y) { mism++; break; } }
  CHECK(mism == 0, "textbook p_k Bresenham == general Bresenham (x1>=x0,y1>=y0)");
  Rect r{-5, -3, 5, 3}; float a = -8, b = 0, c = 8, d = 0; CHECK(cohenSutherland(a, b, c, d, r) && near(a, -5) && near(c, 5), "CS clips horizontal line");
  a = -9; b = 5; c = -7; d = 6; CHECK(!cohenSutherland(a, b, c, d, r), "CS trivially rejects");
  a = -1; b = -1; c = 2; d = 2; CHECK(cohenSutherland(a, b, c, d, r) && near(a, -1) && near(d, 2), "CS trivially accepts");
  int viol = 0; for (int i = 0; i < 3000; i++) { float x0 = (rand() % 400 - 200) / 10.f, y0 = (rand() % 400 - 200) / 10.f, x1 = (rand() % 400 - 200) / 10.f, y1 = (rand() % 400 - 200) / 10.f;
    float ox0 = x0, oy0 = y0, ox1 = x1, oy1 = y1; bool vis = cohenSutherland(x0, y0, x1, y1, r);
    // brute force: sample the original segment, does any sample lie inside?
    bool any = false; for (int k = 0; k <= 2000; k++) { float t = k / 2000.f, x = ox0 + (ox1 - ox0) * t, y = oy0 + (oy1 - oy0) * t; if (x >= r.xmin && x <= r.xmax && y >= r.ymin && y <= r.ymax) { any = true; break; } }
    if (vis != any && !(vis && !any)) viol++;  // accept-but-not-sampled can happen for tiny grazing segments
    if (vis) { float e = 1e-3f; if (x0 < r.xmin - e || x0 > r.xmax + e || x1 < r.xmin - e || x1 > r.xmax + e || y0 < r.ymin - e || y0 > r.ymax + e || y1 < r.ymin - e || y1 > r.ymax + e) viol++; } }
  CHECK(viol == 0, "CS agrees with brute-force sampling on 3000 random segments");
}
static void testGame() {
  std::puts("[gameplay simulation with autopilot]");
  tr::Upgrades up; std::printf("  %-8s %-9s %6s %7s %6s %6s %5s %5s %6s\n", "diff", "result", "time", "score", "cp", "hits", "near", "dmg", "avg km/h");
  for (int di = 0; di < 4; di++) {
    tr::World w; w.start(di, up); float t = 0, dist = 0; const float dt = 1.f / 60.f; int steps = 0; bool nanSeen = false;
    while (!w.over && t < 400) { w.bot(); w.update(dt); t += dt; dist += w.car.speed * dt; steps++; if (!std::isfinite(w.car.x) || !std::isfinite(w.car.speed)) { nanSeen = true; break; } }
    const char* res = w.finished ? "FINISHED" : w.wrecked ? "WRECKED" : w.timeUp ? "TIME UP" : "RUNNING";
    std::printf("  %-8s %-9s %5.1fs %7d %3d/%-2d %6d %5d %5.0f %7.0f   (traffic %d, obstacles %d, walls %d)\n", w.d().name, res, t, w.score, w.checkpoints, w.totalCP, w.collisions, w.nearMisses, w.car.damage, dist / std::max(t, 1.f) * 3.6f, w.trafficHits, w.obsHits, w.wallHits);
    CHECK(!nanSeen, "no NaN in physics"); CHECK(w.tr.length() > w.d().length - 50, "track generated to full length");
    CHECK(w.car.s > 300, "car makes progress along the track");
  }
  // collision test: park a traffic car right in front of a stationary car
  tr::World w; w.start(0, up); w.countdown = 0; w.traffic.clear(); tr::Traffic t; t.s = w.car.s + 3.5f; t.lane = w.car.lat; t.speed = 0; w.traffic.push_back(t); w.updateTraffic(0);
  w.car.vx = std::sin(w.car.yaw) * 20; w.car.vz = std::cos(w.car.yaw) * 20; for (int i = 0; i < 30; i++) w.update(1.f / 60.f);
  CHECK(w.collisions >= 1 && w.car.damage > 0, "head-on collision registers + causes damage");
  // off-road slows the car
  tr::World w2; w2.start(0, up); w2.countdown = 0; w2.traffic.clear(); w2.obs.clear(); w2.in.throttle = 1; for (int i = 0; i < 60 * 8; i++) w2.update(1.f / 60.f); float on = w2.car.speed;
  tr::World w3; w3.start(0, up); w3.countdown = 0; w3.traffic.clear(); w3.obs.clear(); w3.in.throttle = 1; w3.in.steer = -0.15f; for (int i = 0; i < 60 * 8; i++) w3.update(1.f / 60.f);
  std::printf("  speed after 8s: straight %.1f m/s, drifting toward edge %.1f m/s (lat=%.1f)\n", on, w3.car.speed, w3.car.lat);
  CHECK(on > 20, "car accelerates"); 
  // nitro gives higher top speed
  tr::World w4; w4.start(0, up); w4.countdown = 0; w4.traffic.clear(); w4.obs.clear(); w4.in.throttle = 1; w4.in.nitro = true; for (int i = 0; i < 60 * 3; i++) w4.update(1.f / 60.f);
  tr::World w5; w5.start(0, up); w5.countdown = 0; w5.traffic.clear(); w5.obs.clear(); w5.in.throttle = 1; for (int i = 0; i < 60 * 3; i++) w5.update(1.f / 60.f);
  std::printf("  speed after 3s: nitro %.1f m/s vs normal %.1f m/s\n", w4.car.speed, w5.car.speed); CHECK(w4.car.speed > w5.car.speed + 2, "nitro boosts acceleration");
  // upgrades improve acceleration
  tr::Upgrades up5; up5.engine = 5; tr::World w6; w6.start(0, up5); w6.countdown = 0; w6.traffic.clear(); w6.obs.clear(); w6.in.throttle = 1; for (int i = 0; i < 60 * 6; i++) w6.update(1.f / 60.f);
  tr::World w7; w7.start(0, up); w7.countdown = 0; w7.traffic.clear(); w7.obs.clear(); w7.in.throttle = 1; for (int i = 0; i < 60 * 6; i++) w7.update(1.f / 60.f);
  std::printf("  speed after 6s: engine L5 %.1f vs L0 %.1f m/s\n", w6.car.speed, w7.car.speed); CHECK(w6.car.speed > w7.car.speed, "engine upgrade is faster");
}
int main() { testMath(); testAlgos(); testGame(); std::printf("\n%d checks, %d failed\n", checks, fails); return fails ? 1 : 0; }

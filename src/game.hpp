// game.hpp - all game logic (no OpenGL here, so it can be unit/self-tested headless).
#pragma once
#include <vector>
#include <string>
#include <random>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "math3d.hpp"

namespace tr {
using namespace cg;

enum class Weather { Clear, Rain, Storm, Snow };

struct Difficulty {
  const char *name, *env, *weatherName; Weather weather;
  float length, curveMul, slopeMul, trafficGap, obstacleGap, trafficSpeed, timeLimit, cpBonus, grip;
  float rough, mountain, reward, fogStart, fogEnd; bool night;
  float skyTop[3], skyHor[3], ground[3]; int treeKind;
};
inline const Difficulty& difficulty(int i) {
  static const Difficulty d[4] = {
    {"EASY", "Sunny Coastal Highway", "Clear", Weather::Clear, 3200, 0.7f, 0.5f, 70, 160, 24, 60, 20, 1.00f,
     0.0f, 0.0f, 1.0f, 300, 900, false, {0.25f, 0.5f, 0.9f}, {0.75f, 0.88f, 1.0f}, {0.25f, 0.55f, 0.20f}, 0},
    {"MEDIUM", "Misty Mountain Pass", "Rain + Fog", Weather::Rain, 4200, 1.0f, 1.0f, 55, 120, 26, 70, 22, 0.88f,
     0.15f, 0.5f, 1.4f, 80, 520, false, {0.35f, 0.40f, 0.5f}, {0.6f, 0.65f, 0.7f}, {0.20f, 0.38f, 0.20f}, 1},
    {"HARD", "Night Storm Canyon", "Heavy Rain", Weather::Storm, 5200, 1.2f, 1.4f, 42, 90, 28, 75, 28, 0.82f,
     0.5f, 0.35f, 1.9f, 40, 320, true, {0.02f, 0.03f, 0.08f}, {0.08f, 0.10f, 0.18f}, {0.35f, 0.25f, 0.15f}, 2},
    {"EXTREME", "Arctic Blizzard Ridge", "Snow Blizzard", Weather::Snow, 6000, 1.35f, 1.8f, 32, 70, 30, 80, 30, 0.68f,
     0.4f, 0.5f, 2.6f, 25, 220, false, {0.55f, 0.62f, 0.7f}, {0.82f, 0.86f, 0.9f}, {0.9f, 0.93f, 0.97f}, 3}};
  return d[i];
}

// ---------- persistent progress / upgrades ----------
struct Upgrades {
  int engine = 0, nitro = 0, tires = 0, handling = 0, money = 0, best[4] = {0, 0, 0, 0};
  static constexpr int MAXLVL = 5;
  int cost(int lvl) const { return 250 * (lvl + 1); }
  int& level(int i) { return i == 0 ? engine : i == 1 ? nitro : i == 2 ? tires : handling; }
  void save(const char* f = "turbo_racer_save.txt") const {
    if (FILE* fp = std::fopen(f, "w")) { std::fprintf(fp, "%d %d %d %d %d %d %d %d %d\n", engine, nitro, tires, handling, money, best[0], best[1], best[2], best[3]); std::fclose(fp); } }
  void load(const char* f = "turbo_racer_save.txt") {
    if (FILE* fp = std::fopen(f, "r")) { if (std::fscanf(fp, "%d %d %d %d %d %d %d %d %d", &engine, &nitro, &tires, &handling, &money, &best[0], &best[1], &best[2], &best[3]) != 9) *this = Upgrades(); std::fclose(fp); } }
};

// ---------- noise for rough terrain ----------
inline float hash01(int x, int y, int seed = 0) {
  unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16; return (h & 0xffffff) / 16777215.f;
}
inline float vnoise(float x, float y) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y); float fx = x - xi, fy = y - yi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
  return lerp(lerp(hash01(xi, yi), hash01(xi + 1, yi), fx), lerp(hash01(xi, yi + 1), hash01(xi + 1, yi + 1), fx), fy);
}

// ---------- track: curved highways, sharp turns, hairpins, hills, mountain/rough sections ----------
struct TrackPt { float x = 0, y = 0, z = 0, h = 0, curv = 0, ampL = 0, ampR = 0, rough = 0; int terrain = 0; };
inline Vec2 rightVec(float h) { return {-std::cos(h), std::sin(h)}; }  // +lateral = right of travel direction

struct Track {
  std::vector<TrackPt> p; float step = 3.f, halfWidth = 7.5f; int lanes = 4;
  float length() const { return (p.size() - 1) * step; }
  float laneX(int l) const { float w = 2 * halfWidth / lanes; return -halfWidth + w * (l + 0.5f); }
  TrackPt at(float s) const {
    float f = clampf(s / step, 0.f, (float)(p.size() - 2)); int i = (int)f; float t = f - i; TrackPt a = p[i], b = p[i + 1], r = a;
    r.x = lerp(a.x, b.x, t); r.y = lerp(a.y, b.y, t); r.z = lerp(a.z, b.z, t); r.h = lerp(a.h, b.h, t); r.curv = lerp(a.curv, b.curv, t); return r;
  }
  float terrainY(const TrackPt& q, float d, float wx, float wz) const {
    float a = std::fabs(d) - (halfWidth + 2.f); if (a <= 0) return q.y;
    float t = smooth01(a / 60.f), amp = d > 0 ? q.ampR : q.ampL, n = vnoise(wx * 0.012f, wz * 0.012f);
    float bump = (vnoise(wx * 0.25f, wz * 0.25f) - 0.5f) * (0.5f + 2.5f * q.rough) * clampf(a / 4.f, 0, 1);
    return q.y + t * amp * (0.35f + 0.9f * n) + bump;
  }
  void build(const Difficulty& d, unsigned seed) {
    std::mt19937 rng(seed); auto R = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    p.clear(); float x = 0, z = 0, y = 0, h = 0, slope = 0, aL = 6, aR = 6, rough = 0; int N = (int)(d.length / step) + 80, i = 0;
    auto emit = [&](float curv, int terrain, float tAL, float tAR, float tRough, float tSlope) {
      h += curv * step; x += std::sin(h) * step; z += std::cos(h) * step; slope += (tSlope - slope) * 0.03f; y += slope * step;
      aL += (tAL - aL) * 0.06f; aR += (tAR - aR) * 0.06f; rough += (tRough - rough) * 0.08f;
      TrackPt q; q.x = x; q.y = y; q.z = z; q.h = h; q.curv = curv; q.ampL = aL; q.ampR = aR; q.rough = rough; q.terrain = terrain; p.push_back(q); i++; };
    TrackPt s0; p.push_back(s0);
    for (int k = 0; k < 40; k++) emit(0, 0, 6, 6, 0, 0);  // straight start
    float w[5] = {40, 40, 10, 0, 10}; int diff = (int)(&d - &difficulty(0));
    if (diff == 1) { w[0] = 25; w[1] = 35; w[2] = 25; w[3] = 0; w[4] = 15; }
    if (diff == 2) { w[0] = 15; w[1] = 30; w[2] = 30; w[3] = 10; w[4] = 15; }
    if (diff == 3) { w[0] = 10; w[1] = 25; w[2] = 35; w[3] = 15; w[4] = 15; }
    float sumw = w[0] + w[1] + w[2] + w[3] + w[4]; float pendingS = 0;
    while (i < N) {
      float r = R(0, sumw), acc = 0; int type = 0; for (; type < 4; type++) { acc += w[type]; if (r < acc) break; }
      float len = 150, k = 0, sign = R(0, 1) < 0.5f ? -1.f : 1.f; if (h > 2.2f) sign = -1; if (h < -2.2f) sign = 1;
      if (pendingS != 0) { sign = pendingS; pendingS = 0; type = 1; }
      switch (type) {
        case 0: len = R(120, 260); k = 0; break;
        case 1: len = R(180, 320); k = sign * R(0.002f, 0.004f) * d.curveMul; break;
        case 2: len = R(90, 150); k = sign * R(0.009f, 0.014f) * d.curveMul; break;
        case 3: len = R(100, 140); k = sign * R(0.022f, 0.028f); break;
        default: len = R(180, 260); k = sign * R(0.003f, 0.005f) * d.curveMul; pendingS = -sign; }
      float tr_ = R(0, 1); int terrain = tr_ < d.mountain ? 1 : (tr_ < d.mountain + d.rough ? 2 : 0);
      float tAL = R(4, 10), tAR = R(4, 10), tRough = 0, tSlope = R(-0.03f, 0.03f) * d.slopeMul * 0.6f;
      if (terrain == 1) { bool f = R(0, 1) < 0.5f; float hi = R(50, 80), lo = -R(15, 30); tAL = f ? hi : lo; tAR = f ? lo : hi; tSlope = R(-0.07f, 0.07f) * d.slopeMul; }
      if (terrain == 2) { tAL = R(10, 25); tAR = R(10, 25); tRough = 1; }
      int n = std::max(2, (int)(len / step));
      for (int j = 0; j < n && i < N; j++) { float u = (j + 0.5f) / n, e = smooth01(std::min(1.f, std::min(u, 1 - u) / 0.3f)); emit(k * e, terrain, tAL, tAR, tRough, tSlope); }
    }
  }
};

// ---------- entities ----------
struct Traffic { int id = 0; float s = 0, lane = 0, speed = 0, len = 4.4f, wid = 1.9f, col[3] = {1, 0, 0}; bool truck = false, nearDone = false; float x = 0, z = 0, h = 0, y = 0; };
struct Obstacle { int kind = 0; float s = 0, lat = 0, x = 0, z = 0, y = 0, h = 0, hw = 0.4f, hl = 0.4f; bool hit = false; };  // 0 cone 1 barrel 2 rock 3 barrier
struct Pickup { float s = 0, lat = 0, x = 0, z = 0, y = 0; bool taken = false; };
struct Car {
  float x = 0, z = 0, y = 0, yaw = 0, vx = 0, vz = 0, speed = 0, steerState = 0, steerAngle = 0, roll = 0, pitch = 0;
  float nitro = 60, damage = 0, s = 0, lat = 0, wheelRot = 0, bump = 0, latAccel = 0; bool nitroOn = false, offroad = false, braking = false;
};
struct Input { float throttle = 0, brake = 0, steer = 0; bool nitro = false, handbrake = false; };  // steer: +1 = left, -1 = right

inline bool obbOverlap(float ax, float az, float ah, float ahw, float ahl, float bx, float bz, float bh, float bhw, float bhl, float& nx, float& nz, float& pen) {
  float sa = std::sin(ah), ca = std::cos(ah), sb = std::sin(bh), cb = std::cos(bh);
  float ax_[4][2] = {{sa, ca}, {-ca, sa}, {sb, cb}, {-cb, sb}}; float dx = bx - ax, dz = bz - az; pen = 1e9f;
  for (int i = 0; i < 4; i++) {
    float ux = ax_[i][0], uz = ax_[i][1];
    float ra = ahl * std::fabs(ux * sa + uz * ca) + ahw * std::fabs(-ux * ca + uz * sa);
    float rb = bhl * std::fabs(ux * sb + uz * cb) + bhw * std::fabs(-ux * cb + uz * sb);
    float dist = ux * dx + uz * dz, ov = ra + rb - std::fabs(dist); if (ov <= 0) return false;
    if (ov < pen) { pen = ov; float sg = dist >= 0 ? 1.f : -1.f; nx = ux * sg; nz = uz * sg; }
  }
  return true;
}

class World {
 public:
  int diffIdx = 0; Upgrades up; Track tr; Car car; Input in;
  std::vector<Traffic> traffic; std::vector<Obstacle> obs; std::vector<Pickup> pickups;
  float hitCD = 0, time = 0, timeLeft = 0, countdown = 3.f, shake = 0, msgT = 0, comboT = 0, distScore = 0, bonusScore = 0, nitroFlash = 0, hitFlash = 0, endT = 0;
  int checkpoints = 0, totalCP = 0, combo = 0, nearMisses = 0, collisions = 0, score = 0, earned = 0, idx = 0; float nextCP = 700;
  bool finished = false, wrecked = false, timeUp = false, over = false, newBest = false; std::string msg; unsigned seed = 1;
  int cpEvent = 0, wallHits = 0, trafficHits = 0, obsHits = 0;  // incremented when a checkpoint is crossed (renderer may react)

  const Difficulty& d() const { return difficulty(diffIdx); }
  void say(const std::string& m, float t = 1.6f) { msg = m; msgT = t; }

  void start(int di, const Upgrades& u, unsigned sd = 7) {
    *this = World(); diffIdx = di; up = u; seed = sd + di * 101; tr.build(d(), seed); std::mt19937 rng(seed * 7 + 3);
    auto R = [&](float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); };
    timeLeft = d().timeLimit; totalCP = (int)(d().length / 700.f); nextCP = 700;
    car.x = tr.p[3].x + rightVec(tr.p[3].h).x * tr.laneX(1); car.z = tr.p[3].z + rightVec(tr.p[3].h).y * tr.laneX(1); car.yaw = tr.p[3].h; car.y = tr.p[3].y; idx = 2;
    static const float pal[6][3] = {{0.8f, 0.1f, 0.1f}, {0.1f, 0.3f, 0.8f}, {0.9f, 0.8f, 0.1f}, {0.15f, 0.6f, 0.3f}, {0.85f, 0.85f, 0.85f}, {0.5f, 0.2f, 0.7f}};
    int id = 0; for (float s = 170; s < d().length - 60; s += R(0.6f, 1.4f) * d().trafficGap) {
      Traffic t; t.id = id++; t.s = s; t.lane = tr.laneX((int)R(0, 3.999f)); t.truck = R(0, 1) < 0.18f; t.speed = d().trafficSpeed * R(0.8f, 1.15f) * (t.truck ? 0.75f : 1.f);
      t.len = t.truck ? 8.f : 4.4f; t.wid = t.truck ? 2.5f : 1.9f; const float* c = pal[(int)R(0, 5.99f)]; for (int k = 0; k < 3; k++) t.col[k] = c[k]; traffic.push_back(t); }
    for (float s = 220; s < d().length - 60; s += R(0.6f, 1.4f) * d().obstacleGap) {
      Obstacle o; o.s = s; TrackPt q = tr.at(s); o.kind = (q.terrain >= 1 && R(0, 1) < 0.5f) ? 2 : (int)R(0, 3.999f); if (o.kind == 3 && q.terrain == 1) o.kind = 2;
      o.lat = o.kind == 2 ? (R(0, 1) < 0.5f ? -1 : 1) * R(3.f, 6.5f) : tr.laneX((int)R(0, 3.999f)); o.h = q.h;
      static const float hw[4] = {0.35f, 0.45f, 1.0f, 0.5f}, hl[4] = {0.35f, 0.45f, 1.0f, 1.8f}; o.hw = hw[o.kind]; o.hl = hl[o.kind];
      Vec2 r = rightVec(q.h); o.x = q.x + r.x * o.lat; o.z = q.z + r.y * o.lat; o.y = q.y; obs.push_back(o); }
    for (float s = 300; s < d().length - 60; s += R(250, 420)) { Pickup pk; pk.s = s; pk.lat = tr.laneX((int)R(0, 3.999f)); TrackPt q = tr.at(s); Vec2 r = rightVec(q.h);
      pk.x = q.x + r.x * pk.lat; pk.z = q.z + r.y * pk.lat; pk.y = q.y; pickups.push_back(pk); }
    updateTraffic(0); locate();
  }

  void locate() {
    auto& P = tr.p; int n = (int)P.size(); auto t_of = [&](int i) { float dx = P[i + 1].x - P[i].x, dz = P[i + 1].z - P[i].z; return ((car.x - P[i].x) * dx + (car.z - P[i].z) * dz) / (dx * dx + dz * dz); };
    float t = t_of(idx); while (t > 1 && idx < n - 2) { idx++; t = t_of(idx); } while (t < 0 && idx > 0) { idx--; t = t_of(idx); }
    car.s = (idx + clampf(t, 0, 1)) * tr.step; TrackPt q = tr.at(car.s); Vec2 r = rightVec(q.h); car.lat = (car.x - q.x) * r.x + (car.z - q.z) * r.y;
  }
  int curTerrain() const { return tr.p[std::min<int>(idx, (int)tr.p.size() - 1)].terrain; }
  float aMax() const { return (14.f + 1.2f * up.tires + 0.8f * up.handling) * d().grip * (car.offroad ? 0.6f : 1.f); }
  float maxSpeed() const { return (52.f + 5.5f * up.engine) * (1.f - 0.35f * car.damage / 100.f); }

  void stepCar(float dt) {
    const float L = 2.7f; TrackPt q = tr.at(car.s); int terr = curTerrain(); car.offroad = std::fabs(car.lat) > tr.halfWidth;
    float maxSpd = maxSpeed(); bool nOn = in.nitro && car.nitro > 0 && car.speed > 5 && !over; car.nitroOn = nOn;
    if (nOn) { maxSpd *= 1.35f; car.nitro = std::max(0.f, car.nitro - dt * 25.f / (1 + 0.2f * up.nitro)); }
    if (car.offroad) maxSpd *= 0.5f;
    if (terr == 2) maxSpd *= 0.92f;
    float sy = std::sin(car.yaw), cy = std::cos(car.yaw); Vec2 fwd{sy, cy}, rgt{-cy, sy};
    float vf = car.vx * fwd.x + car.vz * fwd.y, vr = car.vx * rgt.x + car.vz * rgt.y;
    float grip = 6.f * (0.6f + 0.1f * up.tires) * d().grip * (car.offroad ? 0.55f : 1.f) * (terr == 2 ? 0.85f : 1.f); if (in.handbrake) grip *= 0.25f;
    if (!over && in.throttle > 0) { float a = (16.f + 1.4f * up.engine) * in.throttle * (1.f - clampf(vf / maxSpd, 0, 1)); if (nOn) a += 20.f; vf += a * dt; }
    if (vf > maxSpd) vf -= (vf - maxSpd) * 1.5f * dt;
    float drag = 0.0025f * vf * std::fabs(vf) + (vf > 0 ? 0.8f : -0.8f) * (std::fabs(vf) > 0.2f ? 1.f : 0.f); if (car.offroad) drag += vf * 0.6f; if (terr == 2) drag += vf * 0.08f;
    vf -= drag * dt; car.braking = in.brake > 0.1f;
    if (in.brake > 0) { if (vf > 0) vf = std::max(0.f, vf - 34.f * in.brake * dt); else vf = std::max(-12.f, vf - 8.f * dt); }
    if (in.handbrake) vf -= vf * 0.35f * dt;
    vr *= std::exp(-grip * dt);
    // steering: smoothed input, speed-sensitive angle limited by lateral grip (a = v^2 tan(d) / L <= aMax)
    float tgt = in.steer, rate = (std::fabs(tgt) > std::fabs(car.steerState)) ? 6.f : 9.f; car.steerState += clampf(tgt - car.steerState, -rate * dt, rate * dt);
    float dmax = std::min(0.5f, std::atan(aMax() * L / std::max(vf * vf, 25.f)) * (1.f + 0.04f * up.handling));
    car.steerAngle = car.steerState * dmax; float omega = vf / L * std::tan(car.steerAngle); car.latAccel = vf * omega;
    car.vx = fwd.x * vf + rgt.x * vr; car.vz = fwd.y * vf + rgt.y * vr; car.yaw += omega * dt; car.x += car.vx * dt; car.z += car.vz * dt;
    car.speed = vf; car.wheelRot += vf * dt / 0.34f;
    car.roll += (clampf(car.latAccel / 20.f, -1, 1) * 0.35f - car.roll) * std::min(1.f, 6.f * dt);
    locate(); q = tr.at(car.s); terr = curTerrain();
    // barriers: guard rails (or invisible boundary posts in rough terrain)
    float lim = tr.halfWidth + (terr == 2 ? 14.f : 2.2f); Vec2 r2 = rightVec(q.h);
    if (std::fabs(car.lat) > lim) { float sg = car.lat > 0 ? 1.f : -1.f, ex = std::fabs(car.lat) - lim; car.x -= r2.x * sg * ex; car.z -= r2.y * sg * ex;
      float vl = car.vx * r2.x + car.vz * r2.y; if (vl * sg > 0) { car.vx -= r2.x * vl * 1.3f; car.vz -= r2.y * vl * 1.3f; if (std::fabs(vl) > 4 && hitCD <= 0) { hurt(std::fabs(vl) * 1.2f, true); wallHits++; hitCD = 0.4f; } }
      car.vx *= 0.97f; car.vz *= 0.97f; locate(); }
    float ty = tr.terrainY(q, car.lat, car.x, car.z); float roughAmp = (car.offroad ? 0.06f : 0.f) + (terr == 2 ? 0.05f : 0.f);
    car.bump = roughAmp * std::sin(time * 37.f + car.s * 0.9f) * clampf(std::fabs(car.speed) / 25.f, 0, 1); car.y += (ty - car.y) * std::min(1.f, 15.f * dt);
    if (roughAmp > 0) shake = std::max(shake, 0.25f * clampf(car.speed / 40.f, 0, 1));
    car.pitch += (std::atan(q.y - tr.at(std::max(0.f, car.s - 3)).y) / 3.f * 1.0f - car.pitch) * std::min(1.f, 8.f * dt);
  }
  void hurt(float amt, bool wall = false) { amt = std::min(amt, 35.f); car.damage += amt; shake = std::max(shake, 0.4f + amt * 0.03f); hitFlash = 0.35f; if (wall) { car.vx *= 0.9f; car.vz *= 0.9f; }
    if (car.damage >= 100) { car.damage = 100; wrecked = true; over = true; say("WRECKED!", 3.f); } }

  void updateTraffic(float dt) {
    for (auto& t : traffic) { t.s = std::min(t.s + t.speed * dt, tr.length() - 5); TrackPt q = tr.at(t.s); Vec2 r = rightVec(q.h); t.x = q.x + r.x * t.lane; t.z = q.z + r.y * t.lane; t.h = q.h; t.y = q.y; }
  }

  void update(float dt) {
    time += dt; hitCD = std::max(0.f, hitCD - dt); shake = std::max(0.f, shake - dt * 1.5f); hitFlash = std::max(0.f, hitFlash - dt); msgT = std::max(0.f, msgT - dt);
    if (countdown > 0) { countdown -= dt; if (countdown <= 0) say("GO!", 1.f); return; }
    if (over) { endT += dt; in.throttle = 0; in.brake = 0.6f; in.nitro = false; in.steer = 0; }
    if (!over) { timeLeft -= dt; if (timeLeft <= 0) { timeLeft = 0; timeUp = true; over = true; say("TIME UP!", 3.f); } }
    static float acc = 0; acc += dt; const float h = 1.f / 120.f; while (acc >= h) { stepCar(h); acc -= h; }
    updateTraffic(dt); nitroFlash += dt; if (comboT > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }
    if (!over) distScore += std::max(0.f, car.speed) * dt * 0.4f;
    float sy = std::sin(car.yaw), cy = std::cos(car.yaw); (void)sy; (void)cy;
    // traffic collisions + near misses
    for (auto& t : traffic) {
      if (std::fabs(t.s - car.s) > 14) continue;
      float nx = 0, nz = 0, pen = 0;
      if (obbOverlap(car.x, car.z, car.yaw, 0.95f, 2.2f, t.x, t.z, t.h, t.wid / 2, t.len / 2, nx, nz, pen)) {
        car.x -= nx * pen; car.z -= nz * pen; float rel = (car.vx * nx + car.vz * nz) - (std::sin(t.h) * nx + std::cos(t.h) * nz) * t.speed;
        if (rel > 0) { car.vx -= nx * rel * 1.25f; car.vz -= nz * rel * 1.25f; } car.vx *= 0.85f; car.vz *= 0.85f;
        if (!over && hitCD <= 0 && rel > 2.f) { collisions++; trafficHits++; combo = 0; hurt(std::max(4.f, rel * 1.1f)); bonusScore -= 50; hitCD = 0.5f; }
        t.nearDone = true; continue; }
      float dd = std::hypot(t.x - car.x, t.z - car.z);
      if (!t.nearDone && !over && dd < 3.4f && car.speed - t.speed > 8) { t.nearDone = true; nearMisses++; combo++; comboT = 4.f; bonusScore += 50.f * std::min(combo, 10); car.nitro = std::min(100.f, car.nitro + 6); say("NEAR MISS x" + std::to_string(combo), 1.f); }
    }
    for (auto& o : obs) {
      if (o.hit || std::fabs(o.s - car.s) > 10) continue;
      float nx = 0, nz = 0, pen = 0;
      if (obbOverlap(car.x, car.z, car.yaw, 0.95f, 2.2f, o.x, o.z, o.h, o.hw, o.hl, nx, nz, pen)) {
        if (o.kind <= 1) { o.hit = true; car.vx *= 0.93f; car.vz *= 0.93f; if (!over) { hurt(o.kind == 0 ? 2.f : 6.f); collisions++; obsHits++; combo = 0; } }
        else { car.x -= nx * pen; car.z -= nz * pen; float rel = car.vx * nx + car.vz * nz; if (rel > 0) { car.vx -= nx * rel * 1.3f; car.vz -= nz * rel * 1.3f; } car.vx *= 0.8f; car.vz *= 0.8f; if (!over && hitCD <= 0 && rel > 2.f) { hurt(std::max(5.f, rel * 1.2f)); collisions++; obsHits++; combo = 0; hitCD = 0.5f; } } }
    }
    for (auto& pk : pickups) if (!pk.taken && std::fabs(pk.s - car.s) < 6 && std::hypot(pk.x - car.x, pk.z - car.z) < 2.6f) { pk.taken = true; car.nitro = std::min(100.f, car.nitro + 30); say("NITRO +30", 1.f); bonusScore += 100; }
    if (!over && car.s >= nextCP && checkpoints < totalCP) { checkpoints++; nextCP += 700; timeLeft += d().cpBonus; bonusScore += 500; cpEvent++; say("CHECKPOINT " + std::to_string(checkpoints) + "/" + std::to_string(totalCP) + "  +" + std::to_string((int)d().cpBonus) + "s", 2.f); }
    if (!over && car.s >= d().length - 10) { finished = true; over = true; bonusScore += 2000 + timeLeft * 25; say("FINISH!", 3.f); }
    score = std::max(0, (int)(distScore + bonusScore));
    if (over && endT > 0.f && earned == 0) { earned = (int)(score * d().reward / 12.f); if (finished) earned = (int)(earned * 1.25f); }
  }
  void commitResult() { if (earned > 0 && !newBest) { up.money += earned; if (score > up.best[diffIdx]) { up.best[diffIdx] = score; } newBest = true; } }

  // ---------- simple autopilot used by --demo / --selftest (also a handy regression test of the physics) ----------
  void bot() {
    float spd = std::max(0.f, car.speed), hor = 45.f + spd * 1.9f; int best = 0; float bestScore = -1e9f, blockSpd = 999, blockDist = 999;
    for (int l = 0; l < tr.lanes; l++) { float lx = tr.laneX(l), sc = -std::fabs(lx - car.lat) * 0.5f, bd = 999, bs = 999;
      for (auto& t : traffic) if (t.s > car.s - 9 && t.s < car.s + hor && std::fabs(t.lane - lx) < 2.6f && t.speed < spd + 6) { float dist = t.s - car.s; sc -= 60.f * (1.f - std::max(0.f, dist) / hor) + 15; if (dist > 0 && dist < bd) { bd = dist; bs = t.speed; } }
      for (auto& o : obs) if (!o.hit && o.s > car.s - 5 && o.s < car.s + hor && std::fabs(o.lat - lx) < 2.6f) { float dist = o.s - car.s; sc -= 90.f * (1.f - std::max(0.f, dist) / hor) + 20; if (dist > 0 && dist < bd) { bd = dist; bs = 0; } }
      if (sc > bestScore) { bestScore = sc; best = l; blockDist = bd; blockSpd = bs; } }
    float la = car.s + 12 + spd * 0.5f; TrackPt q = tr.at(la); Vec2 r = rightVec(q.h); float lat = tr.laneX(best); float tx = q.x + r.x * lat, tz = q.z + r.y * lat;
    float ang = std::atan2(tx - car.x, tz - car.z) - car.yaw; while (ang > PI) ang -= 2 * PI; while (ang < -PI) ang += 2 * PI;
    in.steer = clampf(ang * 2.0f, -1, 1);
    float k = 0, target = maxSpeed();  // corner speed v=sqrt(aMax/k); allow braking distance: v_allowed = sqrt(vc^2 + 2*decel*dist)
    for (float a = 0; a <= 130; a += 10) { float ka = std::fabs(tr.at(car.s + a).curv); if (a < 40) k = std::max(k, ka);
      float vc = std::sqrt(0.95f * aMax() / std::max(ka, 1e-5f)); target = std::min(target, std::sqrt(vc * vc + 2.f * 20.f * a)); }
    if (blockDist < 999) { float gap = blockDist - 9; if (gap < spd * 1.2f) target = std::min(target, std::max(blockSpd, 4.f) + std::max(0.f, gap) * 0.3f); }
    in.throttle = spd < target ? 1.f : 0.f; in.brake = spd > target + 2 ? clampf((spd - target) / 8.f, 0.2f, 1.f) : 0.f;
    in.nitro = k < 0.002f && blockDist > 150 && car.nitro > 20 && spd < maxSpeed() * 0.97f && std::fabs(car.lat) < tr.halfWidth;
    if (spd < 3 && countdown <= 0) { in.throttle = 1; in.brake = 0; }
  }
};
}  // namespace tr

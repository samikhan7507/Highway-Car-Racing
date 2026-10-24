// 3D Turbo Racer - CG Edition | Author: Sami Khan | C++17 + OpenGL (fixed-function) + GLUT
// Own matrix library (math3d.hpp) builds model/view/projection; HUD uses Bresenham, DDA and Cohen-Sutherland.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "game.hpp"
#include "gfx.hpp"
#include "cg_algorithms.hpp"
#include "learn_mode.hpp"

using namespace gfx; using namespace cg; using tr::World;
enum class St { Menu, Select, Garage, Race, Pause, Result, Learn, Help };
static St st = St::Menu, learnReturn = St::Menu;
static World W; static tr::Upgrades UP;
static int winW = 1280, winH = 720, vpx = 0, vpy = 0, vpw = 1280, vph = 720;
static bool keys[256] = {}, sk[256] = {};
static int menuSel = 0, diffSel = 0, garSel = 0, camMode = 0, pauseSel = 0;
static bool showCG = false, botOn = false, demo = false;
static double lastT = 0; static float animT = 0, lightMul = 1.f;
static Mat4 lastModel = Mat4::identity(), lastProj = Mat4::identity();
static struct { float yaw = 0, fov = 62; } cam;
static std::string shotName, shotDir = "screenshots";
static const char* MENU_ITEMS[5] = {"START RACE", "GARAGE  (UPGRADES)", "CG LEARNING MODE", "CONTROLS", "QUIT"};
static const char* UPG_NAMES[4] = {"ENGINE", "NITRO TANK", "TIRES", "HANDLING"};
static const char* UPG_DESC[4] = {"+5.5 m/s top speed and stronger acceleration per level", "Nitro burns slower, so boosts last longer", "More grip: higher cornering limit, less sliding", "Sharper steering response at speed"};

// ------------------------------------------------------------------ 2D / 3D mode helpers
static void begin2D() { glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, VW, VH, 0, -1, 1); glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
  glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_POLYGON_OFFSET_FILL); }
static void end2D() { glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix(); }
static void setup3D(const Mat4& P, const Mat4& V) { glMatrixMode(GL_PROJECTION); loadMat(P, false); glMatrixMode(GL_MODELVIEW); loadMat(V, false); glEnable(GL_DEPTH_TEST); lastProj = P; }
static void setupLight(bool night) {
  glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); glEnable(GL_NORMALIZE);
  float pos[4] = {0.4f, 1, 0.3f, 0}, amb[4] = {night ? 0.28f : 0.45f, night ? 0.3f : 0.45f, night ? 0.4f : 0.5f, 1}, dif[4] = {night ? 0.5f : 0.85f, night ? 0.5f : 0.85f, night ? 0.65f : 0.8f, 1};
  glLightfv(GL_LIGHT0, GL_POSITION, pos); glLightfv(GL_LIGHT0, GL_AMBIENT, amb); glLightfv(GL_LIGHT0, GL_DIFFUSE, dif); glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb); }
static void glc(float r, float g, float b, float a = 1) { glColor4f(r * lightMul, g * lightMul, b * lightMul, a); }
static void drawSky(const float* top, const float* hor, float flash = 0) {
  begin2D(); glBegin(GL_QUADS); glColor3f(lerp(top[0], 1, flash), lerp(top[1], 1, flash), lerp(top[2], 1, flash)); glVertex2f(0, 0); glVertex2f(VW, 0);
  glColor3f(lerp(hor[0], 1, flash), lerp(hor[1], 1, flash), lerp(hor[2], 1, flash)); glVertex2f(VW, VH * 0.52f); glVertex2f(0, VH * 0.52f);
  glVertex2f(0, VH * 0.52f); glVertex2f(VW, VH * 0.52f); glVertex2f(VW, VH); glVertex2f(0, VH); glEnd(); end2D(); }

// ------------------------------------------------------------------ car models (use Mat4: translate*rotate*shear*reflect)
static void wheelAt(float x, float y, float z, float r, float w, float spin, float steer = 0) {
  glPushMatrix(); glTranslatef(x, y, z); loadMat(Mat4::rotateY(steer)); loadMat(Mat4::rotateX(spin)); glRotatef(90, 0, 0, 1); glTranslatef(0, -w / 2, 0);
  glc(0.07f, 0.07f, 0.08f); solidCyl(r, w, 14); glc(0.75f, 0.75f, 0.8f); box(r * 0.8f, w * 0.5f, 0, 0.12f, w * 1.05f, 0.1f); glPopMatrix(); }
static void drawCarModel(const Mat4& M, const float* c, bool player, bool brake, bool nitro, float roll, float steer, float spin) {
  glPushMatrix(); loadMat(M);
  glPushMatrix(); loadMat(Mat4::translate(0, 0.35f, 0) * Mat4::shear(roll * 0.5f, 0, 0, 0, 0, 0) * Mat4::translate(0, -0.35f, 0));  // SHEARING = body roll
  glc(c[0], c[1], c[2]); box(0, 0.62f, 0, 1.9f, 0.6f, 4.4f); box(0, 0.45f, 1.9f, 1.8f, 0.3f, 0.8f); glc(0.08f, 0.1f, 0.16f); box(0, 1.1f, -0.15f, 1.6f, 0.5f, 2.2f);
  glc(c[0] * 0.9f, c[1] * 0.9f, c[2] * 0.9f); box(0, 1.37f, -0.15f, 1.4f, 0.06f, 1.9f);
  if (player) { glc(0.1f, 0.1f, 0.1f); box(0, 1.25f, -2.1f, 1.8f, 0.08f, 0.45f); box(0.7f, 1.0f, -2.0f, 0.08f, 0.5f, 0.1f); box(-0.7f, 1.0f, -2.0f, 0.08f, 0.5f, 0.1f); }
  glc(1, 1, 0.8f); box(0.65f, 0.7f, 2.2f, 0.4f, 0.18f, 0.06f); box(-0.65f, 0.7f, 2.2f, 0.4f, 0.18f, 0.06f);
  if (brake) glColor3f(1, 0.05f, 0.05f); else glc(0.55f, 0.02f, 0.02f); box(0.7f, 0.75f, -2.2f, 0.45f, 0.16f, 0.06f); box(-0.7f, 0.75f, -2.2f, 0.45f, 0.16f, 0.06f);
  glPopMatrix();
  for (int side = 0; side < 2; side++) {  // REFLECTION: left wheels/mirror = right ones mirrored by scale(-1,1,1)
    glPushMatrix(); if (side) loadMat(Mat4::reflectX()); wheelAt(0.98f, 0.36f, 1.4f, 0.36f, 0.3f, spin, steer); wheelAt(0.98f, 0.36f, -1.4f, 0.36f, 0.3f, spin);
    glc(c[0], c[1], c[2]); box(1.05f, 1.0f, 0.55f, 0.22f, 0.12f, 0.16f); glPopMatrix(); }
  if (nitro) { glDisable(GL_LIGHTING); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); float fl = 1.f + 0.4f * std::sin(animT * 70.f) + 0.2f * std::sin(animT * 113.f);
    for (int k = -1; k <= 1; k += 2) { glPushMatrix(); glTranslatef(0.5f * k, 0.55f, -2.3f); glRotatef(-90, 1, 0, 0); glColor4f(0.2f, 0.5f, 1, 0.8f); solidCone(0.22f, 2.4f * fl, 10); glColor4f(1, 0.7f, 0.2f, 0.7f); solidCone(0.14f, 1.4f * fl, 10); glPopMatrix(); }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glEnable(GL_LIGHTING); }
  glPopMatrix(); }
static void drawTruck(const Mat4& M, const float* c) {
  glPushMatrix(); loadMat(M); glc(c[0], c[1], c[2]); box(0, 1.9f, -1.2f, 2.5f, 2.5f, 5.4f); glc(0.12f, 0.14f, 0.2f); box(0, 1.5f, 2.6f, 2.3f, 1.9f, 2.0f); glc(0.4f, 0.5f, 0.6f); box(0, 2.0f, 3.45f, 2.0f, 0.8f, 0.15f);
  glc(1, 0.1f, 0.1f); box(0.9f, 0.7f, -3.95f, 0.4f, 0.2f, 0.06f); box(-0.9f, 0.7f, -3.95f, 0.4f, 0.2f, 0.06f);
  for (int s = -1; s <= 1; s += 2) for (float z : {2.6f, -0.8f, -2.4f}) wheelAt(1.2f * s, 0.5f, z, 0.5f, 0.35f, animT * 20.f);
  glPopMatrix(); }

// ------------------------------------------------------------------ scenery
static void drawTree(int kind, float x, float y, float z, float s, float rnd) {
  glPushMatrix(); glTranslatef(x, y, z);
  if (kind == 2) {  // rocks / dead trees (canyon)
    if (rnd < 0.5f) { glc(0.45f, 0.3f, 0.2f); glScalef(s * 2, s * 1.6f, s * 2); glutSolidDodecahedron(); }
    else { glc(0.25f, 0.18f, 0.12f); solidCyl(0.25f * s, 5 * s, 6); glTranslatef(0, 3 * s, 0); glRotatef(35, 0, 0, 1); solidCyl(0.12f * s, 2.5f * s, 5); } }
  else if (kind == 1 && rnd < 0.25f) { glc(0.45f, 0.45f, 0.48f); glScalef(s * 2, s * 1.4f, s * 2); glutSolidDodecahedron(); }
  else if (kind == 3 && rnd < 0.3f) { glc(0.75f, 0.85f, 0.95f); glScalef(s * 2, s * 1.5f, s * 2); glutSolidDodecahedron(); }
  else { glc(0.3f, 0.2f, 0.1f); solidCyl(0.3f * s, 2 * s, 6); float g = kind == 3 ? 0.35f : 0.55f; glc(0.1f, g * 0.7f + 0.1f * rnd, 0.12f);
    glTranslatef(0, 1.5f * s, 0); solidCone(2.2f * s, 5 * s, 8); glTranslatef(0, 2.2f * s, 0); solidCone(1.6f * s, 4 * s, 8);
    if (kind == 3) { glc(0.95f, 0.97f, 1); glTranslatef(0, 1.8f * s, 0); solidCone(0.95f * s, 2.2f * s, 8); } }
  glPopMatrix(); }
static void drawObstacle(const tr::Obstacle& o) {
  glPushMatrix(); glTranslatef(o.x, o.y, o.z); loadMat(Mat4::rotateY(o.h));
  switch (o.kind) {
    case 0: glc(1, 0.45f, 0.05f); solidCone(0.35f, 0.8f, 10); glc(0.1f, 0.1f, 0.1f); box(0, 0.03f, 0, 0.8f, 0.06f, 0.8f); glc(1, 1, 1); glTranslatef(0, 0.3f, 0); solidCone(0.22f, 0.3f, 10); break;
    case 1: glc(0.85f, 0.1f, 0.1f); solidCyl(0.45f, 0.95f, 12); glc(1, 1, 1); glTranslatef(0, 0.4f, 0); solidCyl(0.46f, 0.15f, 12); break;
    case 2: glTranslatef(0, 0.6f, 0); glScalef(1.1f, 0.9f, 1.1f); glc(0.42f, 0.4f, 0.38f); glutSolidDodecahedron(); break;
    default: for (int k = 0; k < 3; k++) { if (k % 2) glc(1, 1, 1); else glc(0.9f, 0.15f, 0.1f); box(0, 0.45f, -1.2f + k * 1.2f, 1.0f, 0.9f, 1.2f); } }
  glPopMatrix(); }

static void drawTrack() {
  const auto& d = W.d(); const auto& T = W.tr; int n = (int)T.p.size(), i0 = std::max(0, W.idx - 10), i1 = std::min(n - 2, W.idx + 200); float hw = T.halfWidth; int dk = W.diffIdx;
  glDisable(GL_LIGHTING); static const float roadC[4][3] = {{0.22f, 0.22f, 0.24f}, {0.2f, 0.21f, 0.23f}, {0.17f, 0.17f, 0.19f}, {0.52f, 0.54f, 0.58f}};
  static const float rockC[4][3] = {{0.5f, 0.5f, 0.45f}, {0.45f, 0.45f, 0.48f}, {0.55f, 0.32f, 0.2f}, {0.72f, 0.78f, 0.88f}};
  const float offs[8] = {hw, hw + 1.f, hw + 2.2f, hw + 6, hw + 16, hw + 34, hw + 60, hw + 95};
  // terrain strips
  for (int side = -1; side <= 1; side += 2) for (int k = 2; k < 7; k++) {
    glBegin(GL_QUAD_STRIP); for (int i = i0; i <= i1; i++) { const auto& q = T.p[i]; Vec2 r = tr::rightVec(q.h);
      for (int e = 0; e < 2; e++) { float dd = side * offs[k + e], wx = q.x + r.x * dd, wz = q.z + r.y * dd, y = T.terrainY(q, dd, wx, wz); float nz = tr::hash01(i * 3 + (side > 0), k + e);
        float shade = 0.78f + 0.4f * nz, rockT = clampf((y - q.y) / 30.f, 0, 1); float cr = lerp(d.ground[0] * shade, rockC[dk][0] * (0.8f + 0.3f * nz), rockT), cg_ = lerp(d.ground[1] * shade, rockC[dk][1] * (0.8f + 0.3f * nz), rockT), cb = lerp(d.ground[2] * shade, rockC[dk][2] * (0.8f + 0.3f * nz), rockT);
        if (y < q.y - 2.f) { cr = 0.3f * shade; cg_ = 0.28f * shade; cb = 0.26f * shade; } glc(cr, cg_, cb); glVertex3f(wx, y, wz); } }
    glEnd(); }
  // shoulder + rumble strips
  for (int side = -1; side <= 1; side += 2) { for (int k = 0; k < 2; k++) { glBegin(GL_QUAD_STRIP); for (int i = i0; i <= i1; i++) { const auto& q = T.p[i]; Vec2 r = tr::rightVec(q.h);
      if (k == 0) { if ((i / 2) % 2) glc(0.9f, 0.1f, 0.1f); else glc(0.95f, 0.95f, 0.95f); } else glc(d.ground[0] * 0.6f + 0.2f, d.ground[1] * 0.55f + 0.18f, d.ground[2] * 0.5f + 0.15f);
      for (int e = 0; e < 2; e++) { float dd = side * offs[k + e]; glVertex3f(q.x + r.x * dd, q.y + 0.01f, q.z + r.y * dd); } } glEnd(); } }
  // road surface (gravel blend on rough terrain)
  glBegin(GL_QUAD_STRIP); for (int i = i0; i <= i1; i++) { const auto& q = T.p[i]; Vec2 r = tr::rightVec(q.h); float v = 0.9f + 0.1f * ((i / 3) % 2), g = q.rough;
    glc(lerp(roadC[dk][0], 0.38f, g) * v, lerp(roadC[dk][1], 0.32f, g) * v, lerp(roadC[dk][2], 0.26f, g) * v);
    glVertex3f(q.x - r.x * hw, q.y + 0.02f, q.z - r.y * hw); glVertex3f(q.x + r.x * hw, q.y + 0.02f, q.z + r.y * hw); } glEnd();
  // markings
  glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-2, -2);
  auto stripe = [&](float lat, float w, int i, float lift) { const auto& a = T.p[i]; const auto& b = T.p[i + 1]; Vec2 ra = tr::rightVec(a.h), rb = tr::rightVec(b.h);
    glBegin(GL_QUADS); glVertex3f(a.x + ra.x * (lat - w), a.y + lift, a.z + ra.y * (lat - w)); glVertex3f(a.x + ra.x * (lat + w), a.y + lift, a.z + ra.y * (lat + w));
    glVertex3f(b.x + rb.x * (lat + w), b.y + lift, b.z + rb.y * (lat + w)); glVertex3f(b.x + rb.x * (lat - w), b.y + lift, b.z + rb.y * (lat - w)); glEnd(); };
  glc(0.95f, 0.95f, 0.95f); for (int i = i0; i <= i1; i++) { stripe(-hw + 0.5f, 0.13f, i, 0.03f); stripe(hw - 0.5f, 0.13f, i, 0.03f); }
  glc(1, 0.85f, 0.2f); for (int i = i0; i <= i1; i++) if ((i % 4) < 2) { stripe(0, 0.1f, i, 0.03f); }
  glc(0.9f, 0.9f, 0.9f); for (int i = i0; i <= i1; i++) if ((i % 4) < 2) { stripe(-hw / 2, 0.08f, i, 0.03f); stripe(hw / 2, 0.08f, i, 0.03f); }
  glDisable(GL_POLYGON_OFFSET_FILL);
  // guard rails / boundary poles
  for (int side = -1; side <= 1; side += 2) {
    glBegin(GL_QUAD_STRIP); for (int i = i0; i <= i1; i++) { const auto& q = T.p[i]; Vec2 r = tr::rightVec(q.h); float dd = side * (hw + 2.2f); if (q.terrain == 2) { continue; }
      glc(0.7f, 0.72f, 0.75f); glVertex3f(q.x + r.x * dd, q.y + 0.55f, q.z + r.y * dd); glVertex3f(q.x + r.x * dd, q.y + 0.95f, q.z + r.y * dd); } glEnd();
    glLineWidth(3); glBegin(GL_LINES); for (int i = i0; i <= i1; i += 2) { const auto& q = T.p[i]; Vec2 r = tr::rightVec(q.h);
      if (q.terrain != 2) { float dd = side * (hw + 2.2f); glc(0.4f, 0.4f, 0.42f); glVertex3f(q.x + r.x * dd, q.y, q.z + r.y * dd); glVertex3f(q.x + r.x * dd, q.y + 0.9f, q.z + r.y * dd); }
      else if (i % 4 == 0) { float dd = side * (hw + 14.f); glc(1, 0.5f, 0.1f); glVertex3f(q.x + r.x * dd, q.y, q.z + r.y * dd); glVertex3f(q.x + r.x * dd, q.y + 1.6f, q.z + r.y * dd); } } glEnd(); }
  // finish checker + checkpoint / finish arches
  glEnable(GL_LIGHTING);
  auto arch = [&](float s, float r_, float g_, float b_) { if (std::fabs(s - W.car.s) > 450) return; auto q = T.at(s); Vec2 r = tr::rightVec(q.h); glDisable(GL_LIGHTING);
    glPushMatrix(); glTranslatef(q.x, q.y, q.z); loadMat(Mat4::rotateY(q.h)); glColor3f(r_, g_, b_); box(hw + 0.8f, 3.5f, 0, 0.7f, 7, 0.7f); box(-hw - 0.8f, 3.5f, 0, 0.7f, 7, 0.7f); box(0, 7.2f, 0, 2 * hw + 2.3f, 1.2f, 0.7f);
    glColor3f(1, 1, 1); box(0, 7.2f, -0.4f, 2 * hw + 1.6f, 0.5f, 0.1f); glPopMatrix(); glEnable(GL_LIGHTING); (void)r; };
  for (int k = 1; k <= W.totalCP; k++) { bool passed = k <= W.checkpoints; arch(700.f * k, passed ? 0.4f : 0.1f, passed ? 0.4f : 0.8f, passed ? 0.45f : 1.f); }
  arch(d.length - 10, 1, 1, 1);
  { float fs = d.length - 10; if (std::fabs(fs - W.car.s) < 450) { glDisable(GL_LIGHTING); auto q = T.at(fs); Vec2 r = tr::rightVec(q.h); glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-3, -3);
      for (int cx = 0; cx < 16; cx++) for (int cz = 0; cz < 2; cz++) { if ((cx + cz) % 2) glColor3f(0.05f, 0.05f, 0.05f); else glColor3f(1, 1, 1); float l0 = -hw + cx * (2 * hw / 16), l1 = l0 + 2 * hw / 16, z0 = cz * 1.5f, z1 = z0 + 1.5f; Vec2 f{std::sin(q.h), std::cos(q.h)};
        glBegin(GL_QUADS); for (int v = 0; v < 4; v++) { float l = (v == 0 || v == 3) ? l0 : l1, zz = v < 2 ? z0 : z1; glVertex3f(q.x + r.x * l + f.x * zz, q.y + 0.05f, q.z + r.y * l + f.y * zz); } glEnd(); }
      glDisable(GL_POLYGON_OFFSET_FILL); glEnable(GL_LIGHTING); } }
  // scenery
  for (int i = i0; i <= i1; i += 2) { float h = tr::hash01(i, 11); if (h > 0.4f) continue; const auto& q = T.p[i]; Vec2 r = tr::rightVec(q.h); int side = tr::hash01(i, 12) < 0.5f ? -1 : 1; float dd = side * (hw + 7 + tr::hash01(i, 13) * 42), wx = q.x + r.x * dd, wz = q.z + r.y * dd;
    drawTree(d.treeKind, wx, T.terrainY(q, dd, wx, wz) - 0.2f, wz, 0.8f + 0.8f * tr::hash01(i, 14), tr::hash01(i, 15)); }
}

static void drawWeather() {
  const auto& d = W.d(); if (d.weather == tr::Weather::Clear) return; glDisable(GL_LIGHTING); glDisable(GL_FOG); glEnable(GL_BLEND);
  int N = d.weather == tr::Weather::Storm ? 1500 : d.weather == tr::Weather::Rain ? 800 : 1100; float cx = W.car.x, cz = W.car.z, cy = W.car.y; Vec2 f{std::sin(cam.yaw), std::cos(cam.yaw)};
  if (d.weather == tr::Weather::Snow) { glColor4f(1, 1, 1, 0.9f); glPointSize(3.f); glBegin(GL_POINTS); }
  else { glColor4f(0.7f, 0.8f, 1, 0.45f); glLineWidth(1.3f); glBegin(GL_LINES); }
  for (int k = 0; k < N; k++) { float ux = tr::hash01(k, 1, 5), uy = tr::hash01(k, 2, 5), uz = tr::hash01(k, 3, 5); float fall = d.weather == tr::Weather::Snow ? 4.f : 28.f;
    float y = cy + 14 - std::fmod(uy * 28.f + animT * fall, 28.f), lx = (ux - 0.5f) * 50.f, lz = uz * 70.f - 8.f; if (d.weather == tr::Weather::Snow) lx += std::sin(animT * 1.3f + k) * 1.2f;
    float x = cx + f.x * lz - f.y * lx, z = cz + f.y * lz + f.x * lx;
    if (d.weather == tr::Weather::Snow) glVertex3f(x, y, z); else { float sl = 0.04f * std::max(0.f, W.car.speed); glVertex3f(x, y, z); glVertex3f(x - f.x * sl, y + 0.9f, z - f.y * sl); } }
  glEnd(); }

// ------------------------------------------------------------------ HUD (Bresenham speedometer, DDA + Cohen-Sutherland minimap)
static void plotPix(const std::vector<Pix>& px, float sz) { glPointSize(sz); glBegin(GL_POINTS); for (auto& p : px) glVertex2f(p.x + 0.5f, p.y + 0.5f); glEnd(); }
static void drawMinimap() {
  const float X0 = 1030, Y0 = 20, S = 230; Rect clip{X0, Y0, X0 + S, Y0 + S}; float cx = X0 + S / 2, cy = Y0 + S * 0.72f, sc = 0.30f; const auto& c = W.car; float sy = std::sin(c.yaw), cy_ = std::cos(c.yaw);
  Mat3 Rm = Mat3::identity(); Rm.m[0][0] = -cy_; Rm.m[0][1] = sy; Rm.m[1][0] = sy; Rm.m[1][1] = cy_;  // world(x,z) -> (right, forward)
  Mat3 M = Mat3::translate(cx, cy) * Mat3::scale(sc, -sc) * Rm * Mat3::translate(-c.x, -c.z);          // COMPOSITE transform
  col(0, 0, 0, 0.55f); fillRect(X0, Y0, S, S); const auto& P = W.tr.p; int a = std::max(0, W.idx - 60), b = std::min((int)P.size() - 2, W.idx + 160);
  for (int pass = 0; pass < 2; pass++) { col(pass ? 1.f : 0.5f, pass ? 0.9f : 0.5f, pass ? 0.3f : 0.55f); std::vector<Pix> all;
    for (int i = a; i < b; i += 2) { if ((i < W.idx) != (pass == 0)) continue; int j = std::min((int)P.size() - 1, i + 2); Vec2 p0 = M.apply({P[i].x, P[i].z}), p1 = M.apply({P[j].x, P[j].z}); float x0 = p0.x, y0 = p0.y, x1 = p1.x, y1 = p1.y;
      if (cohenSutherland(x0, y0, x1, y1, clip)) { auto seg = ddaLine((int)std::lround(x0), (int)std::lround(y0), (int)std::lround(x1), (int)std::lround(y1)); all.insert(all.end(), seg.begin(), seg.end()); } }
    plotPix(all, 3.f); }
  auto dot = [&](float wx, float wz, float r, float g, float b_, float sz) { Vec2 p = M.apply({wx, wz}); if (p.x > X0 + 3 && p.x < X0 + S - 3 && p.y > Y0 + 3 && p.y < Y0 + S - 3) { col(r, g, b_); point(p.x, p.y, sz); } };
  for (auto& t : W.traffic) if (std::fabs(t.s - c.s) < 350) dot(t.x, t.z, 1, 0.2f, 0.2f, 5);
  for (auto& pk : W.pickups) if (!pk.taken && std::fabs(pk.s - c.s) < 350) dot(pk.x, pk.z, 0.3f, 0.7f, 1, 6);
  for (int k = W.checkpoints + 1; k <= W.totalCP; k++) { auto q = W.tr.at(700.f * k); dot(q.x, q.z, 0.2f, 1, 1, 8); }
  col(1, 1, 1); glBegin(GL_TRIANGLES); glVertex2f(cx, cy - 9); glVertex2f(cx - 6, cy + 7); glVertex2f(cx + 6, cy + 7); glEnd(); col(0.7f, 0.8f, 1); strokeRect(X0, Y0, S, S, 2);
  col(1, 1, 1, 0.8f); text(X0 + 4, Y0 + S - 5, "MINIMAP: DDA + Cohen-Sutherland clip", GLUT_BITMAP_HELVETICA_10); }
static void drawSpeedo() {
  const float cx = 1120, cy = 595, R = 92; float kmh = std::max(0.f, W.car.speed) * 3.6f, maxK = 400.f; col(0, 0, 0, 0.55f); circle(cx, cy, R + 10, true, 48); col(0.7f, 0.8f, 1); circle(cx, cy, R + 10, false, 48);
  for (int k = 0; k <= 8; k++) { float ang = rad(225.f - 270.f * k / 8.f); int x0 = (int)(cx + (R - 14) * std::cos(ang)), y0 = (int)(cy - (R - 14) * std::sin(ang)), x1 = (int)(cx + R * std::cos(ang)), y1 = (int)(cy - R * std::sin(ang));
    col(k >= 7 ? 1.f : 0.9f, k >= 7 ? 0.3f : 0.9f, k >= 7 ? 0.3f : 0.9f); plotPix(bresenhamLine(x0, y0, x1, y1), 3.f);
    textC(cx + (R - 28) * std::cos(ang), cy - (R - 28) * std::sin(ang) + 4, std::to_string(k * 50), GLUT_BITMAP_HELVETICA_10); }
  float frac = clampf(kmh / maxK, 0, 1), ang = rad(225.f - 270.f * frac); int nx = (int)(cx + (R - 12) * std::cos(ang)), ny = (int)(cy - (R - 12) * std::sin(ang)); col(1, 0.25f, 0.15f);
  for (int o = -1; o <= 1; o++) plotPix(bresenhamLine((int)cx + o, (int)cy, nx + o, ny), 3.f);  // BRESENHAM needle
  col(1, 1, 1); circle(cx, cy, 8, true); bigC(cx, cy + 60, std::to_string((int)kmh), 0.3f, 2.5f); textC(cx, cy + 78, "km/h", GLUT_BITMAP_HELVETICA_12);
  col(1, 1, 1, 0.7f); textC(cx, cy - 36, "Bresenham", GLUT_BITMAP_HELVETICA_10); }
static void drawHUD() {
  const auto& d = W.d(); char b[128]; col(0, 0, 0, 0.5f); fillRect(20, 16, 330, 112); col(1, 1, 1);
  text(32, 42, "SCORE", GLUT_BITMAP_HELVETICA_12); col(1, 0.9f, 0.3f); big(32, 82, std::to_string(W.score), 0.25f, 2.f); col(1, 1, 1);
  int tm = (int)std::ceil(W.timeLeft); if (W.timeLeft < 10) col(1, 0.3f, 0.3f); std::snprintf(b, sizeof b, "TIME %d:%02d", tm / 60, tm % 60); text(32, 106, b); col(1, 1, 1);
  std::snprintf(b, sizeof b, "CHECKPOINT %d/%d", W.checkpoints, W.totalCP); text(32, 124, b, GLUT_BITMAP_HELVETICA_12);
  text(200, 106, std::string(d.name) + " - " + d.weatherName, GLUT_BITMAP_HELVETICA_12); if (W.combo > 1) { col(1, 0.6f, 0.2f); text(200, 124, "COMBO x" + std::to_string(W.combo), GLUT_BITMAP_HELVETICA_12); }
  // nitro + damage bars
  col(0, 0, 0, 0.5f); fillRect(20, 640, 300, 64); col(0.2f, 0.6f, 1); text(30, 660, "NITRO  [SPACE]", GLUT_BITMAP_HELVETICA_12); col(0.15f, 0.15f, 0.2f); fillRect(30, 666, 280, 12);
  col(0.2f, 0.65f + 0.3f * (W.car.nitroOn ? 1.f : 0.f), 1); fillRect(30, 666, 280 * W.car.nitro / 100.f, 12); col(1, 0.4f, 0.3f); text(30, 696, "DAMAGE", GLUT_BITMAP_HELVETICA_12);
  col(0.2f, 0.1f, 0.1f); fillRect(100, 686, 210, 10); col(1, 0.25f + 0.5f * (1 - W.car.damage / 100.f), 0.2f); fillRect(100, 686, 210 * W.car.damage / 100.f, 10);
  // progress bar
  float px = 400, pw = 480; col(0, 0, 0, 0.5f); fillRect(px - 10, 678, pw + 20, 30); col(0.3f, 0.3f, 0.35f); fillRect(px, 690, pw, 6);
  col(0.3f, 0.9f, 0.4f); fillRect(px, 690, pw * clampf(W.car.s / d.length, 0, 1), 6); for (int k = 1; k <= W.totalCP; k++) { col(0.2f, 1, 1); fillRect(px + pw * 700.f * k / d.length - 1, 684, 3, 18); }
  col(1, 1, 1); circle(px + pw * clampf(W.car.s / d.length, 0, 1), 693, 6, true); col(1, 1, 1); text(px, 676, std::to_string((int)W.car.s) + " / " + std::to_string((int)d.length) + " m", GLUT_BITMAP_HELVETICA_12);
  drawMinimap(); drawSpeedo();
  if (W.msgT > 0 && W.countdown <= 0) { col(1, 0.9f, 0.2f, clampf(W.msgT * 2.f, 0, 1)); bigC(640, 170, W.msg, 0.4f, 3.f); }
  if (W.countdown > 0) { int n = (int)std::ceil(W.countdown); col(1, 0.85f, 0.2f); bigC(640, 300, std::to_string(n), 1.2f, 6.f); }
  if (W.hitFlash > 0) { col(1, 0, 0, W.hitFlash * 0.8f); strokeRect(4, 4, VW - 8, VH - 8, 14); }
  if (W.car.nitroOn) { col(0.3f, 0.6f, 1, 0.25f); strokeRect(2, 2, VW - 4, VH - 4, 10); }
  if (showCG) {  // live view of the maths used this frame
    col(0, 0, 0, 0.72f); fillRect(20, 140, 520, 330); col(1, 0.8f, 0.2f); text(32, 162, "CG OVERLAY  (G to hide)", GLUT_BITMAP_HELVETICA_12); col(0.9f, 1, 0.9f); float y = 184;
    text(32, y, "Model = T(pos) * Ry(yaw) * Rx(-pitch)", GLUT_BITMAP_9_BY_15); y += 18;
    for (int i = 0; i < 4; i++) { std::snprintf(b, sizeof b, "[%7.2f %7.2f %7.2f %8.2f]", lastModel.m[i][0], lastModel.m[i][1], lastModel.m[i][2], lastModel.m[i][3]); text(32, y, b, GLUT_BITMAP_9_BY_15); y += 16; }
    y += 6; std::snprintf(b, sizeof b, "yaw %.1f deg  pitch %.1f  roll-shear %.3f", deg(W.car.yaw), deg(W.car.pitch), W.car.roll * 0.5f); text(32, y, b, GLUT_BITMAP_9_BY_15); y += 18;
    float f = 1.f / std::tan(rad(cam.fov) / 2); std::snprintf(b, sizeof b, "Perspective: fov %.1f  f=%.3f  P00=%.3f", cam.fov, f, lastProj.m[0][0]); text(32, y, b, GLUT_BITMAP_9_BY_15); y += 18;
    text(32, y, "v_clip = P * V * M * v   (own Mat4 -> glLoadMatrixf)", GLUT_BITMAP_9_BY_15); y += 24; col(0.6f, 0.9f, 1);
    const char* lines[] = {"Speedometer needle/ticks : Bresenham line", "Minimap road             : DDA + Cohen-Sutherland", "Minimap view             : Translate*Scale*Rotate", "Left wheels & mirror     : Reflection scale(-1,1,1)", "Body lean in corners     : Shear (roll)", "Camera/speed feel        : FOV (projection scale)", "Everything moves with    : dt animation"};
    for (auto l : lines) { text(32, y, l, GLUT_BITMAP_9_BY_15); y += 16; } }
}

// ------------------------------------------------------------------ scenes
static void updateCamera(float dt) { float dy = W.car.yaw - cam.yaw; while (dy > PI) dy -= 2 * PI; while (dy < -PI) dy += 2 * PI; cam.yaw += dy * std::min(1.f, dt * 4.5f);
  float tf = 60.f + std::max(0.f, W.car.speed) * 0.28f + (W.car.nitroOn ? 12.f : 0.f); cam.fov += (tf - cam.fov) * std::min(1.f, dt * 3.f); }
static void drawRace() {
  const auto& d = W.d(); const auto& c = W.car; lightMul = d.night ? 0.45f : 1.f; float flash = (d.weather == tr::Weather::Storm && std::fmod(animT, 11.f) < 0.15f) ? 0.6f : 0.f;
  drawSky(d.skyTop, d.skyHor, flash);
  Vec3 fwd{std::sin(cam.yaw), 0, std::cos(cam.yaw)}, pos{c.x, c.y, c.z}, eye, ctr; const Vec3 up{0, 1, 0};
  if (camMode == 0) { eye = pos - fwd * (8.5f + c.speed * 0.02f) + up * 3.2f; ctr = pos + fwd * 12 + up * 1.4f; } else if (camMode == 1) { eye = pos - fwd * 14.f + up * 6.5f; ctr = pos + fwd * 10; } else { eye = pos + fwd * 0.6f + up * 1.25f; ctr = pos + fwd * 20 + up * 1.2f; }
  eye = eye + Vec3{std::sin(animT * 47) * W.shake * 0.25f, std::cos(animT * 61) * W.shake * 0.2f, 0};
  Mat4 P = Mat4::perspective(rad(cam.fov), VW / VH, 0.5f, 1000), V = Mat4::lookAt(eye, ctr, up); setup3D(P, V); setupLight(d.night);
  glEnable(GL_FOG); float fc[4] = {d.skyHor[0] + flash, d.skyHor[1] + flash, d.skyHor[2] + flash, 1}; glFogfv(GL_FOG_COLOR, fc); glFogi(GL_FOG_MODE, GL_LINEAR); glFogf(GL_FOG_START, d.fogStart); glFogf(GL_FOG_END, d.fogEnd);
  drawTrack(); glEnable(GL_LIGHTING);
  for (auto& o : W.obs) if (!o.hit && o.s > c.s - 30 && o.s < c.s + 500) drawObstacle(o);
  for (auto& pk : W.pickups) if (!pk.taken && pk.s > c.s - 30 && pk.s < c.s + 500) { glDisable(GL_LIGHTING); glPushMatrix(); glTranslatef(pk.x, pk.y + 1.3f + 0.2f * std::sin(animT * 3), pk.z); glRotatef(animT * 120, 0, 1, 0); glColor3f(0.2f, 0.7f, 1); box(0, 0, 0, 0.7f, 0.7f, 0.7f); glRotatef(45, 1, 0, 1); glColor3f(0.8f, 0.95f, 1); box(0, 0, 0, 0.45f, 0.45f, 0.45f); glPopMatrix(); glEnable(GL_LIGHTING); }
  for (auto& t : W.traffic) if (t.s > c.s - 30 && t.s < c.s + 520) { Mat4 M = Mat4::translate(t.x, t.y, t.z) * Mat4::rotateY(t.h); if (t.truck) drawTruck(M, t.col); else drawCarModel(M, t.col, false, false, false, 0, 0, animT * 12.f); }
  static const float pc[3] = {0.9f, 0.12f, 0.08f}; lastModel = Mat4::translate(c.x, c.y + c.bump, c.z) * Mat4::rotateY(c.yaw) * Mat4::rotateX(-c.pitch);
  if (d.night) { glDisable(GL_LIGHTING); glDepthMask(GL_FALSE); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glPushMatrix(); loadMat(lastModel); glBegin(GL_TRIANGLE_FAN); glColor4f(1, 0.95f, 0.7f, 0.35f); glVertex3f(0, 0.6f, 2.3f); glColor4f(1, 0.95f, 0.7f, 0); glVertex3f(-9, 0.1f, 40); glVertex3f(9, 0.1f, 40); glEnd(); glPopMatrix();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_TRUE); glEnable(GL_LIGHTING); }
  if (camMode != 2) drawCarModel(lastModel, pc, true, c.braking, c.nitroOn, c.roll, c.steerAngle, c.wheelRot); drawWeather();
  glDisable(GL_FOG); glDisable(GL_LIGHTING); begin2D(); drawHUD(); end2D(); }
static void drawShowroom() {
  static const float top[3] = {0.05f, 0.07f, 0.14f}, hor[3] = {0.25f, 0.3f, 0.45f}; lightMul = 1.f; drawSky(top, hor); Mat4 P = Mat4::perspective(rad(42), VW / VH, 0.5f, 100), V = Mat4::lookAt({4.2f, 2.4f, 7.5f}, {-2.6f, 0.8f, 0}, {0, 1, 0});
  setup3D(P, V); setupLight(false); glDisable(GL_LIGHTING); glColor3f(0.08f, 0.1f, 0.16f); glBegin(GL_TRIANGLE_FAN); glVertex3f(0, 0, 0); for (int i = 0; i <= 48; i++) { float a = 2 * PI * i / 48; glVertex3f(9 * std::cos(a), 0, 9 * std::sin(a)); } glEnd();
  glColor3f(0.3f, 0.5f, 1); glLineWidth(2); glBegin(GL_LINE_LOOP); for (int i = 0; i < 48; i++) { float a = 2 * PI * i / 48; glVertex3f(3.6f * std::cos(a), 0.01f, 3.6f * std::sin(a)); } glEnd(); glEnable(GL_LIGHTING);
  static const float pc[3] = {0.9f, 0.12f, 0.08f}; lastModel = Mat4::rotateY(animT * 0.6f); drawCarModel(lastModel, pc, true, false, false, 0, 0, animT * 6.f); glDisable(GL_LIGHTING); }
static void panel(float x, float y, float w, float h, float a = 0.6f) { col(0.02f, 0.03f, 0.08f, a); fillRect(x, y, w, h); col(0.35f, 0.5f, 0.8f, 0.8f); strokeRect(x, y, w, h, 2); }
static void footer() { col(0.65f, 0.75f, 0.95f, 0.9f); textC(640, 708, "Sami Khan   |   linkedin.com/in/samikhan07   |   github.com/samikhan7507", GLUT_BITMAP_HELVETICA_12); }

static void drawMenu() { drawShowroom(); begin2D(); col(1, 0.85f, 0.2f); bigC(400, 110, "3D TURBO RACER", 0.55f, 4.f); col(0.4f, 0.8f, 1); bigC(400, 168, "CG EDITION", 0.3f, 3.f);
  for (int i = 0; i < 5; i++) { bool s = i == menuSel; if (s) { col(1, 0.8f, 0.2f, 0.9f); fillRect(120, 230 + i * 62, 560, 46); col(0, 0, 0); } else { col(0.1f, 0.14f, 0.25f, 0.85f); fillRect(120, 230 + i * 62, 560, 46); col(0.85f, 0.9f, 1); } textC(400, 260 + i * 62, MENU_ITEMS[i]); }
  col(1, 1, 1); text(120, 580, "Money: $" + std::to_string(UP.money), GLUT_BITMAP_HELVETICA_18); col(0.7f, 0.8f, 1); text(120, 610, "[UP/DOWN] choose   [ENTER] select   [ESC] quit", GLUT_BITMAP_HELVETICA_12); footer(); end2D(); }
static void drawSelect() { drawShowroom(); begin2D(); col(1, 0.85f, 0.2f); bigC(640, 80, "CHOOSE DIFFICULTY", 0.4f, 3.f);
  static const char* desc[4] = {"Gentle curves, light traffic, generous time", "Hills, mountain pass, rain and fog", "Night storm, rough terrain, hairpins", "Blizzard, ice grip, dense traffic, tight clock"};
  for (int i = 0; i < 4; i++) { const auto& d = tr::difficulty(i); float x = 40 + i * 305, y = 130; bool s = i == diffSel; col(d.skyTop[0], d.skyTop[1], d.skyTop[2], 0.9f); fillRect(x, y, 285, 80); col(d.ground[0], d.ground[1], d.ground[2], 0.95f); fillRect(x, y + 80, 285, 18);
    col(0.05f, 0.07f, 0.14f, 0.9f); fillRect(x, y + 98, 285, 330); if (s) col(1, 0.8f, 0.2f); else col(0.4f, 0.5f, 0.7f); strokeRect(x, y, 285, 428, s ? 5.f : 2.f);
    col(1, 1, 1); bigC(x + 142, y + 62, d.name, 0.26f, 2.5f); float ty = y + 124; col(1, 0.9f, 0.5f); textC(x + 142, ty, d.env, GLUT_BITMAP_HELVETICA_18); ty += 30; col(0.85f, 0.9f, 1);
    ty = block(x + 14, ty, desc[i], 40, 18, GLUT_BITMAP_HELVETICA_12) + 8; text(x + 14, ty, std::string("Weather: ") + d.weatherName, GLUT_BITMAP_HELVETICA_12); ty += 20; char b[96]; std::snprintf(b, sizeof b, "Length: %.1f km    Time: %d s", d.length / 1000, (int)d.timeLimit); text(x + 14, ty, b, GLUT_BITMAP_HELVETICA_12); ty += 20;
    std::snprintf(b, sizeof b, "Traffic: every ~%d m   Grip: %d%%", (int)d.trafficGap, (int)(d.grip * 100)); text(x + 14, ty, b, GLUT_BITMAP_HELVETICA_12); ty += 20; text(x + 14, ty, "Reward x" + std::to_string(d.reward).substr(0, 3), GLUT_BITMAP_HELVETICA_12); ty += 30;
    col(1, 0.8f, 0.2f); for (int k = 0; k < 4; k++) { if (k <= i) circle(x + 24 + k * 22, ty, 7, true); else circle(x + 24 + k * 22, ty, 7, false); } ty += 30; col(0.6f, 1, 0.7f); text(x + 14, ty, "Best score: " + std::to_string(UP.best[i]), GLUT_BITMAP_HELVETICA_18); }
  col(0.7f, 0.8f, 1); textC(640, 590, "[LEFT/RIGHT] choose   [ENTER] race   [ESC] back", GLUT_BITMAP_HELVETICA_18); footer(); end2D(); }
static void drawGarage() { drawShowroom(); begin2D(); col(1, 0.85f, 0.2f); bigC(420, 80, "GARAGE", 0.45f, 3.5f); col(0.5f, 1, 0.6f); text(60, 120, "Money: $" + std::to_string(UP.money), GLUT_BITMAP_HELVETICA_18);
  for (int i = 0; i < 4; i++) { bool s = i == garSel; float y = 150 + i * 100; col(s ? 0.2f : 0.08f, s ? 0.25f : 0.1f, s ? 0.45f : 0.18f, 0.9f); fillRect(60, y, 600, 86); if (s) col(1, 0.8f, 0.2f); else col(0.4f, 0.5f, 0.7f); strokeRect(60, y, 600, 86, s ? 4.f : 2.f);
    col(1, 1, 1); text(80, y + 28, UPG_NAMES[i], GLUT_BITMAP_HELVETICA_18); col(0.75f, 0.85f, 1); text(80, y + 50, UPG_DESC[i], GLUT_BITMAP_HELVETICA_12); int lv = UP.level(i);
    for (int k = 0; k < tr::Upgrades::MAXLVL; k++) { if (k < lv) col(1, 0.8f, 0.2f); else col(0.2f, 0.25f, 0.35f); fillRect(80 + k * 34, y + 60, 28, 14); }
    if (lv >= tr::Upgrades::MAXLVL) { col(0.5f, 1, 0.6f); text(500, y + 40, "MAX", GLUT_BITMAP_HELVETICA_18); } else { col(UP.money >= UP.cost(lv) ? 0.5f : 1.f, UP.money >= UP.cost(lv) ? 1.f : 0.4f, 0.4f); text(480, y + 40, "UPGRADE $" + std::to_string(UP.cost(lv)), GLUT_BITMAP_HELVETICA_18); } }
  col(0.7f, 0.8f, 1); text(60, 580, "[UP/DOWN] choose   [ENTER] buy upgrade   [ESC] back   (progress is saved automatically)", GLUT_BITMAP_HELVETICA_12); footer(); end2D(); }
static void drawPauseOverlay() { begin2D(); col(0, 0, 0, 0.6f); fillRect(0, 0, VW, VH); col(1, 0.85f, 0.2f); bigC(640, 200, "PAUSED", 0.5f, 4.f); static const char* it[4] = {"RESUME", "CG LEARNING MODE", "RESTART RACE", "QUIT TO MENU"};
  for (int i = 0; i < 4; i++) { bool s = i == pauseSel; if (s) { col(1, 0.8f, 0.2f, 0.9f); fillRect(440, 260 + i * 60, 400, 44); col(0, 0, 0); } else { col(0.1f, 0.14f, 0.25f, 0.9f); fillRect(440, 260 + i * 60, 400, 44); col(0.85f, 0.9f, 1); } textC(640, 289 + i * 60, it[i]); } end2D(); }
static void drawResult() { drawShowroom(); begin2D(); panel(300, 70, 680, 560, 0.8f); const auto& d = W.d(); const char* r = W.finished ? "RACE COMPLETE!" : W.wrecked ? "CAR WRECKED" : "TIME UP"; col(W.finished ? 0.4f : 1.f, W.finished ? 1.f : 0.4f, 0.4f); bigC(640, 140, r, 0.38f, 3.f);
  col(1, 1, 1); textC(640, 175, std::string(d.name) + " - " + d.env); float y = 230; char b[96]; auto row = [&](const char* k, const std::string& v) { col(0.7f, 0.8f, 1); text(380, y, k); col(1, 1, 1); text(700, y, v); y += 30; };
  row("Final score", std::to_string(W.score)); row("Checkpoints", std::to_string(W.checkpoints) + " / " + std::to_string(W.totalCP)); row("Distance", std::to_string((int)W.car.s) + " m");
  row("Near misses", std::to_string(W.nearMisses)); std::snprintf(b, sizeof b, "%d  (traffic %d, objects %d, walls %d)", W.collisions, W.trafficHits, W.obsHits, W.wallHits); row("Collisions", b); row("Damage", std::to_string((int)W.car.damage) + "%");
  row("Money earned", "$" + std::to_string(W.earned)); row("Best score", std::to_string(UP.best[W.diffIdx])); if (W.newBest && W.score >= UP.best[W.diffIdx]) { col(1, 0.85f, 0.2f); textC(640, y + 10, "NEW BEST!", GLUT_BITMAP_HELVETICA_18); }
  int stars = W.finished ? (W.car.damage < 30 ? 3 : W.car.damage < 70 ? 2 : 1) : 0; col(1, 0.8f, 0.2f); for (int k = 0; k < 3; k++) { if (k < stars) circle(570 + k * 70, y + 70, 24, true); else circle(570 + k * 70, y + 70, 24, false); }
  col(0.7f, 0.8f, 1); textC(640, 590, "[ENTER] menu    [R] retry    [G] garage", GLUT_BITMAP_HELVETICA_18); end2D(); }
static void drawHelp() { drawShowroom(); begin2D(); panel(250, 60, 780, 600, 0.82f); col(1, 0.85f, 0.2f); bigC(640, 120, "CONTROLS", 0.4f, 3.f); static const char* L[] = {"W / UP ........ accelerate", "S / DOWN ...... brake / reverse", "A D / LEFT RIGHT ... steer", "SPACE ......... nitro boost (collect blue cubes to refill)", "B ............. handbrake (drift)", "C ............. change camera (chase / far / hood)", "G ............. show live CG maths overlay", "L ............. open CG Learning Mode", "P / ESC ....... pause", "R ............. restart race", "F ............. toggle fullscreen", "", "Goal: reach every checkpoint (+time), avoid traffic and obstacles,", "pass close to cars for near-miss combos, and finish before time runs out."};
  float y = 170; col(0.9f, 0.95f, 1); for (auto l : L) { text(300, y, l, GLUT_BITMAP_9_BY_15); y += 28; } col(0.7f, 0.8f, 1); textC(640, 630, "[ESC] back", GLUT_BITMAP_HELVETICA_18); end2D(); }

// ------------------------------------------------------------------ flow
static void startRace(int di) { W.start(di, UP); cam.yaw = W.car.yaw; cam.fov = 60; st = St::Race; showCG = false; }
static void toggleFullscreen() { static bool fs = false; fs = !fs; if (fs) glutFullScreen(); else glutReshapeWindow(1280, 720); }
static void savePPM(const std::string& name) {
  std::vector<unsigned char> px((size_t)vpw * vph * 3), fl((size_t)vpw * vph * 3); glPixelStorei(GL_PACK_ALIGNMENT, 1); glReadPixels(vpx, vpy, vpw, vph, GL_RGB, GL_UNSIGNED_BYTE, px.data());
  for (int y = 0; y < vph; y++) std::memcpy(&fl[(size_t)y * vpw * 3], &px[(size_t)(vph - 1 - y) * vpw * 3], (size_t)vpw * 3);
  std::string path = shotDir + "/" + name + ".ppm"; if (FILE* f = std::fopen(path.c_str(), "wb")) { std::fprintf(f, "P6\n%d %d\n255\n", vpw, vph); std::fwrite(fl.data(), 1, fl.size(), f); std::fclose(f); std::printf("saved %s\n", path.c_str()); } }

struct Step { std::function<void()> setup; int frames; std::string name; }; static std::vector<Step> script; static size_t stepIdx = 0; static int stepCnt = -1;
static void buildScript() {
  static const char* dn[4] = {"easy", "medium", "hard", "extreme"};
  script.push_back({[] { st = St::Menu; }, 4, "01_main_menu"}); script.push_back({[] { st = St::Select; diffSel = 1; }, 4, "02_difficulty_select"});
  for (int d = 0; d < 4; d++) { script.push_back({[d] { startRace(d); botOn = true; }, 620, std::string("0") + std::to_string(3 + d) + "_race_" + dn[d]});
    if (d == 0) script.push_back({[] { showCG = true; }, 2, "07_cg_overlay"}); if (d == 0) script.push_back({[] { showCG = false; camMode = 1; }, 2, "08_race_far_camera"}); if (d == 0) script.push_back({[] { camMode = 0; }, 0, ""}); }
  script.push_back({[] { st = St::Garage; UP.money = 900; UP.engine = 2; UP.tires = 1; }, 4, "09_garage"});
  for (int p = 0; p < learn::NPAGES; p++) script.push_back({[p] { st = St::Learn; learn::enter(p, animT); learn::S.t0 = animT - 10; if (p == 5) learn::S.flip = false; }, 3, std::string("learn_") + (p < 9 ? "0" : "") + std::to_string(p + 1) + "_" + learn::INFO[p].tab});
  script.push_back({[] { botOn = true; tr::Upgrades u; W.start(0, u); while (!W.over && W.time < 400) { W.bot(); W.update(1.f / 30.f); } while (W.endT < 3) { W.update(1.f / 30.f); } UP = tr::Upgrades(); W.commitResult(); UP = W.up; st = St::Result; }, 4, "20_result"});
}
static void demoTick() {
  if (!shotName.empty() || stepIdx >= script.size()) return; Step& s = script[stepIdx]; if (stepCnt < 0) { s.setup(); stepCnt = 0; } else stepCnt++;
  if (stepCnt >= s.frames) { if (s.name.empty()) { stepIdx++; stepCnt = -1; } else shotName = s.name; } }

static void update(float dt) {
  if (demo) demoTick();
  if (st == St::Race) {
    if (botOn) W.bot(); else { W.in.throttle = (keys['w'] || sk[GLUT_KEY_UP]) ? 1.f : 0.f; W.in.brake = (keys['s'] || sk[GLUT_KEY_DOWN]) ? 1.f : 0.f;
      W.in.steer = ((keys['a'] || sk[GLUT_KEY_LEFT]) ? 1.f : 0.f) - ((keys['d'] || sk[GLUT_KEY_RIGHT]) ? 1.f : 0.f); W.in.nitro = keys[' ']; W.in.handbrake = keys['b']; }
    W.update(dt); updateCamera(dt); if (W.over && W.endT > 2.5f && !W.newBest) { W.commitResult(); UP = W.up; UP.save(); st = St::Result; botOn = false; }
  } else if (st == St::Learn) learn::update(dt, sk[GLUT_KEY_UP], sk[GLUT_KEY_DOWN]);
}
static void display() {
  glDisable(GL_SCISSOR_TEST); glViewport(0, 0, winW, winH); glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glViewport(vpx, vpy, vpw, vph); glEnable(GL_SCISSOR_TEST); glScissor(vpx, vpy, vpw, vph); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glEnable(GL_LINE_SMOOTH); glEnable(GL_POINT_SMOOTH); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  switch (st) { case St::Menu: drawMenu(); break; case St::Select: drawSelect(); break; case St::Garage: drawGarage(); break; case St::Race: drawRace(); break;
    case St::Pause: drawRace(); drawPauseOverlay(); break; case St::Result: drawResult(); break; case St::Help: drawHelp(); break;
    case St::Learn: begin2D(); learn::draw(animT); end2D(); break; }
  if (demo && !shotName.empty()) { savePPM(shotName); shotName.clear(); stepIdx++; stepCnt = -1; if (stepIdx >= script.size()) { std::printf("demo finished\n"); std::exit(0); } }
  glutSwapBuffers(); }
static void reshape(int w, int h) { winW = w; winH = std::max(1, h); float a = (float)winW / winH, t = VW / VH; if (a > t) { vph = winH; vpw = (int)(winH * t); } else { vpw = winW; vph = (int)(winW / t); } vpx = (winW - vpw) / 2; vpy = (winH - vph) / 2; }
static void tick(int) { double now = glutGet(GLUT_ELAPSED_TIME) / 1000.0; float dt = demo ? 1.f / 60.f : (float)std::min(0.05, now - lastT); lastT = now; animT += dt; update(dt); glutPostRedisplay(); glutTimerFunc(demo ? 1 : 14, tick, 0); }

static void menuEnter() {
  if (menuSel == 0) st = St::Select; else if (menuSel == 1) st = St::Garage; else if (menuSel == 2) { learnReturn = St::Menu; learn::enter(0, animT); st = St::Learn; } else if (menuSel == 3) st = St::Help; else { UP.save(); std::exit(0); } }
static void keyDown(unsigned char k, int, int) {
  unsigned char lk = (unsigned char)std::tolower(k); keys[lk] = true; if (demo) return; if (k == 'f' || k == 'F') { toggleFullscreen(); return; }
  switch (st) {
    case St::Menu: if (k == 13 || k == ' ') menuEnter(); else if (k == 27) { UP.save(); std::exit(0); } else if (lk == 'w') menuSel = (menuSel + 4) % 5; else if (lk == 's') menuSel = (menuSel + 1) % 5; break;
    case St::Select: if (k == 13) startRace(diffSel); else if (k == 27) st = St::Menu; else if (lk == 'a') diffSel = (diffSel + 3) % 4; else if (lk == 'd') diffSel = (diffSel + 1) % 4; break;
    case St::Garage: if (k == 13) { int lv = UP.level(garSel); if (lv < tr::Upgrades::MAXLVL && UP.money >= UP.cost(lv)) { UP.money -= UP.cost(lv); UP.level(garSel)++; UP.save(); } } else if (k == 27) st = St::Menu; else if (lk == 'w') garSel = (garSel + 3) % 4; else if (lk == 's') garSel = (garSel + 1) % 4; break;
    case St::Race: if (k == 27 || lk == 'p') { st = St::Pause; pauseSel = 0; } else if (lk == 'g') showCG = !showCG; else if (lk == 'c') camMode = (camMode + 1) % 3; else if (lk == 'r') startRace(W.diffIdx); else if (lk == 'l') { learnReturn = St::Pause; learn::enter(0, animT); st = St::Learn; } break;
    case St::Pause: if (k == 27 || lk == 'p') st = St::Race; else if (k == 13) { if (pauseSel == 0) st = St::Race; else if (pauseSel == 1) { learnReturn = St::Pause; learn::enter(0, animT); st = St::Learn; } else if (pauseSel == 2) startRace(W.diffIdx); else st = St::Menu; } else if (lk == 'w') pauseSel = (pauseSel + 3) % 4; else if (lk == 's') pauseSel = (pauseSel + 1) % 4; break;
    case St::Result: if (k == 13) st = St::Menu; else if (lk == 'r') startRace(W.diffIdx); else if (lk == 'g') st = St::Garage; break;
    case St::Help: if (k == 27 || k == 13) st = St::Menu; break;
    case St::Learn: if (k == 27) st = learnReturn; else learn::keyboard(k, animT); break; } }
static void keyUp(unsigned char k, int, int) { keys[(unsigned char)std::tolower(k)] = false; }
static void specDown(int k, int, int) {
  if (k < 256) sk[k] = true; if (demo) return;
  if (st == St::Menu) { if (k == GLUT_KEY_UP) menuSel = (menuSel + 4) % 5; if (k == GLUT_KEY_DOWN) menuSel = (menuSel + 1) % 5; }
  else if (st == St::Select) { if (k == GLUT_KEY_LEFT) diffSel = (diffSel + 3) % 4; if (k == GLUT_KEY_RIGHT) diffSel = (diffSel + 1) % 4; }
  else if (st == St::Garage) { if (k == GLUT_KEY_UP) garSel = (garSel + 3) % 4; if (k == GLUT_KEY_DOWN) garSel = (garSel + 1) % 4; }
  else if (st == St::Pause) { if (k == GLUT_KEY_UP) pauseSel = (pauseSel + 3) % 4; if (k == GLUT_KEY_DOWN) pauseSel = (pauseSel + 1) % 4; }
  else if (st == St::Learn) { int m = k == GLUT_KEY_LEFT ? 0 : k == GLUT_KEY_RIGHT ? 1 : k == GLUT_KEY_UP ? 2 : k == GLUT_KEY_DOWN ? 3 : -1; if (m >= 0) learn::special(m, animT);
    if (k == GLUT_KEY_PAGE_UP) learn::enter(learn::S.page - 1, animT); if (k == GLUT_KEY_PAGE_DOWN) learn::enter(learn::S.page + 1, animT); } }
static void specUp(int k, int, int) { if (k < 256) sk[k] = false; }

int main(int argc, char** argv) {
  glutInit(&argc, argv); for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--screenshots")) { demo = true; if (i + 1 < argc) shotDir = argv[++i]; } else if (!std::strcmp(argv[i], "--help")) { std::puts("3D Turbo Racer - CG Edition\n  --screenshots DIR   scripted autopilot run that saves PPM screenshots (used for testing)"); return 0; } }
  glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH); glutInitWindowSize(1280, 720); glutCreateWindow("3D Turbo Racer - CG Edition | Sami Khan"); glutIgnoreKeyRepeat(1);
  glutDisplayFunc(display); glutReshapeFunc(reshape); glutKeyboardFunc(keyDown); glutKeyboardUpFunc(keyUp); glutSpecialFunc(specDown); glutSpecialUpFunc(specUp);
  if (!demo) UP.load(); else { learn::S.page = 0; buildScript(); } reshape(1280, 720); lastT = glutGet(GLUT_ELAPSED_TIME) / 1000.0; glutTimerFunc(16, tick, 0); glutMainLoop(); return 0; }

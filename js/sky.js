/* =====================================================================
 * sky.js - sky dome (gradient shader + sun/moon + stars), clouds,
 * mountain rings and the DAY -> SUNSET -> NIGHT cycle.
 * Authors : Sami (B25, R54139) & Zaid (B29, R54231)
 * ===================================================================== */
var Sky = (function () {
  var T = THREE;
  var THEMES = [
    { name: 'DAY',    top: 0x2f6fb5, hor: 0xbad3ea, fog: 0xbad3ea, grass: 0x4f8a3c, hemiSky: 0xdcefff, hemiGnd: 0x4a5d34, sunCol: 0xfff1d6, mtn1: 0x7f9bb8, mtn2: 0x5f8a62, cloud: 0xffffff, hemiI: 0.95, sunI: 0.85, star: 0, sun: [0.35, 0.5, -0.8] },
    { name: 'SUNSET', top: 0x3b3f8c, hor: 0xf3925a, fog: 0xe3a07a, grass: 0x4a6a34, hemiSky: 0xffc9a0, hemiGnd: 0x4a4030, sunCol: 0xff9a50, mtn1: 0x8a6a8a, mtn2: 0x56604a, cloud: 0xffb890, hemiI: 0.75, sunI: 0.7, star: 0.15, sun: [0.25, 0.1, -0.95] },
    { name: 'NIGHT',  top: 0x040918, hor: 0x1b2a55, fog: 0x101a38, grass: 0x1d3520, hemiSky: 0x5a6aa8, hemiGnd: 0x14181f, sunCol: 0x9aaaff, mtn1: 0x1c2a4a, mtn2: 0x14221f, cloud: 0x3a4670, hemiI: 0.5, sunI: 0.22, star: 1, sun: [-0.3, 0.55, -0.8] }
  ];
  THEMES.forEach(function (t) { ['top', 'hor', 'fog', 'grass', 'hemiSky', 'hemiGnd', 'sunCol', 'mtn1', 'mtn2', 'cloud'].forEach(function (k) { t[k] = new T.Color(t[k]); }); t.sunV = new T.Vector3(t.sun[0], t.sun[1], t.sun[2]).normalize(); });
  var KEYS = ['top', 'hor', 'fog', 'grass', 'hemiSky', 'hemiGnd', 'sunCol', 'mtn1', 'mtn2', 'cloud'];

  function cloudTexture() {
    var c = document.createElement('canvas'); c.width = 256; c.height = 96; var x = c.getContext('2d');
    for (var i = 0; i < 14; i++) {
      var cx = 40 + Math.random() * 176, cy = 40 + Math.random() * 28, r = 22 + Math.random() * 26, g = x.createRadialGradient(cx, cy, 2, cx, cy, r);
      g.addColorStop(0, 'rgba(255,255,255,0.9)'); g.addColorStop(1, 'rgba(255,255,255,0)'); x.fillStyle = g; x.fillRect(0, 0, 256, 96);
    }
    return new T.CanvasTexture(c);
  }
  function ridge(R, hMax, seed, mat) {          // ring of mountains built vertex by vertex
    var N = 120, pos = [], idx = [];
    for (var i = 0; i <= N; i++) {
      var a = i / N * Math.PI * 2, n = Math.abs(Math.sin(a * 3 + seed) * 0.5 + Math.sin(a * 7 + seed * 2) * 0.3 + Math.sin(a * 19 + seed * 3) * 0.2);
      pos.push(Math.cos(a) * R, -10, Math.sin(a) * R, Math.cos(a) * R, hMax * (0.25 + 0.75 * n), Math.sin(a) * R);
      if (i < N) { var k = i * 2; idx.push(k, k + 1, k + 2, k + 1, k + 3, k + 2); }
    }
    var g = new T.BufferGeometry(); g.setAttribute('position', new T.Float32BufferAttribute(pos, 3)); g.setIndex(idx);
    var m = new T.Mesh(g, mat); m.frustumCulled = false; m.renderOrder = -8; return m;
  }

  function create(scene, w) {
    var s = { clock: 0, mode: 'AUTO', cur: {}, num: { hemiI: 0.95, sunI: 0.85, star: 0 }, night: 0 };
    KEYS.forEach(function (k) { s.cur[k] = THEMES[0][k].clone(); });
    s.group = new T.Group(); scene.add(s.group);
    s.skyMat = new T.ShaderMaterial({ side: T.BackSide, depthWrite: false, fog: false,
      uniforms: { top: { value: s.cur.top }, hor: { value: s.cur.hor }, sunDir: { value: THEMES[0].sunV.clone() }, sunCol: { value: s.cur.sunCol }, star: { value: 0 } },
      vertexShader: 'varying vec3 vD; void main(){ vD = position; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }',
      fragmentShader: 'uniform vec3 top; uniform vec3 hor; uniform vec3 sunDir; uniform vec3 sunCol; uniform float star; varying vec3 vD;\n' +
        'float hash(vec3 p){ return fract(sin(dot(p, vec3(12.9898,78.233,37.719))) * 43758.5453); }\n' +
        'void main(){ vec3 d = normalize(vD); float h = d.y;\n' +
        ' vec3 c = mix(hor, top, pow(clamp(h,0.0,1.0), 0.55)); if (h < 0.0) c = hor * 0.8;\n' +
        ' float sd = max(dot(d, normalize(sunDir)), 0.0); c += sunCol * (pow(sd, 700.0) * 2.5 + pow(sd, 14.0) * 0.4);\n' +
        ' float st = step(0.9975, hash(floor(d * 220.0))) * step(0.03, h); c += vec3(st) * star;\n' +
        ' gl_FragColor = vec4(c, 1.0); }' });
    var dome = new T.Mesh(new T.SphereGeometry(470, 32, 16), s.skyMat); dome.renderOrder = -10; dome.frustumCulled = false; s.group.add(dome);
    var ct = cloudTexture(); s.clouds = [];
    for (var i = 0; i < 16; i++) {
      var sp = new T.Sprite(new T.SpriteMaterial({ map: ct, color: 0xffffff, transparent: true, depthWrite: false, fog: false, opacity: 0.85 }));
      sp.userData = { a: Math.random() * Math.PI * 2, r: 260 + Math.random() * 120, y: 55 + Math.random() * 90, sp: 0.002 + Math.random() * 0.004 };
      var sc = 90 + Math.random() * 110; sp.scale.set(sc, sc * 0.38, 1); sp.renderOrder = -9; sp.frustumCulled = false; s.group.add(sp); s.clouds.push(sp);
    }
    s.m1 = new T.MeshBasicMaterial({ color: 0x7f9bb8, side: T.DoubleSide, fog: false }); s.m2 = new T.MeshBasicMaterial({ color: 0x5f8a62, side: T.DoubleSide, fog: false });
    s.group.add(ridge(430, 85, 1.3, s.m1)); s.group.add(ridge(350, 48, 4.1, s.m2));
    s.skyMat.userData.nobend = true; s.m1.userData.nobend = true; s.m2.userData.nobend = true;
    s.w = w; return s;
  }

  function ease(x) { return x * x * (3 - 2 * x); }
  /* mode: 'AUTO' or 0/1/2.  Returns the current night amount 0..1 */
  function update(s, dt, camPos) {
    var a, b, f;
    if (s.mode === 'AUTO') {
      s.clock += dt; var ph = 55, i = Math.floor(s.clock / ph) % 3, loc = s.clock % ph;
      a = THEMES[i]; b = THEMES[(i + 1) % 3]; f = loc < 40 ? 0 : ease((loc - 40) / 15);
    } else { a = b = THEMES[s.mode]; f = 0; }
    var k = 1 - Math.exp(-dt * 1.6), tmp = new T.Color();
    KEYS.forEach(function (key) { tmp.copy(a[key]).lerp(b[key], f); s.cur[key].lerp(tmp, k); });
    ['hemiI', 'sunI', 'star'].forEach(function (key) { var tv = a[key] + (b[key] - a[key]) * f; s.num[key] += (tv - s.num[key]) * k; });
    var sv = a.sunV.clone().lerp(b.sunV, f).normalize(); s.skyMat.uniforms.sunDir.value.lerp(sv, k); s.skyMat.uniforms.star.value = s.num.star;
    var sc = s.w.scene; sc.fog.color.copy(s.cur.fog); s.w.grassMat.color.copy(s.cur.grass);
    s.w.hemi.color.copy(s.cur.hemiSky); s.w.hemi.groundColor.copy(s.cur.hemiGnd); s.w.hemi.intensity = s.num.hemiI;
    s.w.sun.color.copy(s.cur.sunCol); s.w.sun.intensity = s.num.sunI; s.w.sun.position.copy(s.skyMat.uniforms.sunDir.value).multiplyScalar(80);
    tmp.copy(s.cur.mtn1).lerp(s.cur.hor, 0.25); s.m1.color.copy(tmp); tmp.copy(s.cur.mtn2).lerp(s.cur.hor, 0.15); s.m2.color.copy(tmp);
    s.clouds.forEach(function (c) { c.userData.a += c.userData.sp * dt; c.position.set(Math.cos(c.userData.a) * c.userData.r, c.userData.y, Math.sin(c.userData.a) * c.userData.r); c.material.color.copy(s.cur.cloud); });
    s.group.position.set(camPos.x, 0, camPos.z);          // dome, clouds, mountains follow the camera
    s.night = Math.max(0, Math.min(1, 1 - (s.num.hemiI - 0.5) / 0.4));
    return s.night;
  }
  return { create: create, update: update, THEMES: THEMES };
})();

/* =====================================================================
 * game.js - main loop, input, physics, traffic, collision, cameras
 * Authors : Sami (B25, R54139) & Zaid (B29, R54231)
 * ===================================================================== */
(function () {
  var T = THREE, $ = function (id) { return document.getElementById(id); };
  var glCanvas = $('gl');
  var renderer = new T.WebGLRenderer({ canvas: glCanvas, antialias: true, alpha: true });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  var scene = new T.Scene();
  var persp = new T.PerspectiveCamera(60, 1, 0.3, 520);
  var ortho = new T.OrthographicCamera(-20, 20, 30, -30, 1, 300);
  var VIEWS = ['CHASE (perspective)', 'COCKPIT (perspective)', 'TOP-DOWN (parallel/orthographic)'];
  var world = World.create(scene);
  HUD.init();

  /* ---- player (with wet-road mirror reflection: M = scale(1,-1,1)) ---- */
  var playerCar = Cars.build(0xd01818, 'sport'); scene.add(playerCar);
  var mirrorCar = Cars.build(0xd01818, 'sport');
  mirrorCar.matrixAutoUpdate = false; scene.add(mirrorCar);
  [playerCar, mirrorCar].forEach(function (c) { c.matrixAutoUpdate = false; });
  var sky = Sky.create(scene, world), timeModes = ['AUTO', 0, 1, 2], timeIdx = 0, nightAmt = 0, curveV = 0, hillV = 0;
  var cones = [-0.62, 0.62].map(function (x) {                  // headlight beams (visible at night)
    var g = new T.ConeGeometry(3.4, 30, 20, 1, true); g.translate(0, -15, 0);
    var m = new T.Mesh(g, new T.MeshBasicMaterial({ color: 0xfff0b0, transparent: true, opacity: 0, blending: T.AdditiveBlending, depthWrite: false, side: T.DoubleSide, fog: false }));
    m.rotation.x = Math.PI / 2; m.position.set(x, 0.62, -2.4); playerCar.add(m); return m;
  });
  var blob = new T.Mesh(new T.PlaneGeometry(2.6, 5.4), new T.MeshBasicMaterial({ color: 0, transparent: true, opacity: 0.35 }));
  blob.rotation.x = -Math.PI / 2; blob.position.y = 0.02; scene.add(blob);
  Bend.apply(scene);                              // make road, scenery and cars follow the curve / hill bend

  var P, traffic = [], state = 'menu', view = 0, shake = 0, t0 = 0, spawnT = 0, best = 0, tilt = false, tiltVal = 0;
  var input = { left: false, right: false, gas: false, brake: false };
  var autoGas = document.body.classList.contains('touch');   // phones: car accelerates by itself (GAS = full power)
  var VMAX = 78;                                 // m/s  (= 280 km/h)
  try { best = +localStorage.getItem('hwBest') || 0; } catch (e) {}

  function reset() {
    P = { x: 0, speed: 0, vx: 0, yaw: 0, roll: 0, pitch: 0, dist: 0, score: 0, over: 0, spin: 0, overtakes: 0, near: 0, time: 0 };
    traffic.forEach(function (c) { scene.remove(c.mesh); }); traffic = []; spawnT = 0;
    $('hudScore').textContent = 0; $('go').classList.add('hide'); $('flash').style.opacity = 0;
  }

  /* ---- traffic ---- */
  var COLORS = [0x2057d6, 0xf2c200, 0x1fa34a, 0xeeeeee, 0x7a3ab8, 0xff7a1a, 0x222831, 0x00a8b5];
  function spawn(forceLane, forceZ, forceSpeed) {
    var lane = forceLane !== undefined ? forceLane : (Math.random() * World.LANES) | 0, z = forceZ !== undefined ? forceZ : -300;
    for (var i = 0; i < traffic.length; i++) if (traffic[i].lane === lane && Math.abs(traffic[i].z - z) < 40) return;
    var r = Math.random(), type = r < 0.15 ? 'truck' : r < 0.4 ? 'suv' : r < 0.7 ? 'sedan' : 'sport';
    var m = Cars.build(COLORS[(Math.random() * COLORS.length) | 0], type); m.matrixAutoUpdate = false; scene.add(m); Bend.apply(m);
    var sp = forceSpeed || (type === 'truck' ? 18 + Math.random() * 6 : 24 + Math.random() * (14 + Math.min(10, P.time / 12)));
    traffic.push({ mesh: m, lane: lane, x: World.laneX(lane), z: z, speed: sp, hx: m.userData.hx, hz: m.userData.hz, done: false, prevZ: z, wheelRot: 0 });
  }

  /* ---- update ---- */
  function update(dt) {
    var alive = state === 'play';
    if (alive) {
      P.time += dt;
      /* longitudinal: throttle, brake, drag */
      if (input.gas || (autoGas && !input.brake && P.speed < VMAX * 0.6)) P.speed += 20 * (1 - P.speed / VMAX) * dt;
      else             P.speed -= 3.5 * dt;
      if (input.brake) P.speed -= 42 * dt;
      P.speed = Math.max(0, Math.min(VMAX, P.speed));
      /* lateral steering: smooth acceleration toward target velocity */
      var steer = (input.right ? 1 : 0) - (input.left ? 1 : 0); if (tilt) steer = Math.max(-1, Math.min(1, tiltVal));
      var lim = 9 * Math.min(1, 0.35 + P.speed / 30);
      P.vx += (steer * lim - P.vx) * Math.min(1, 8 * dt);
      P.x += P.vx * dt;
      var edge = World.ROAD_HALF - 1.1;                       // guard-rail contact
      if (P.x > edge || P.x < -edge) { P.x = Math.max(-edge, Math.min(edge, P.x)); P.vx *= -0.2; P.speed *= (1 - 0.8 * dt); shake = Math.max(shake, 0.15); }
      P.yaw += (-P.vx * 1.6 - P.yaw) * Math.min(1, 8 * dt);   // degrees
      P.roll += (-P.vx * 0.9 - P.roll) * Math.min(1, 6 * dt);
      P.pitch += ((input.brake ? 3 : (input.gas ? -1.2 : 0)) - P.pitch) * Math.min(1, 5 * dt);
      P.dist += P.speed * dt;
      P.score += P.speed * dt * 0.1;

      /* traffic spawn rate grows with time (difficulty) */
      spawnT -= dt; if (spawnT <= 0) { spawn(); spawnT = Math.max(0.5, 1.5 - P.time * 0.012); }
    } else if (state === 'crash') {
      P.speed *= Math.max(0, 1 - 2.2 * dt); P.spin += 540 * dt * Math.min(1, P.speed / 10 + 0.2); P.over += dt;
      P.x += P.vx * dt * 0.5; P.vx *= 0.97;
      if (P.over > 1.2 && state === 'crash') gameOver();
    } else if (state === 'menu') { P.dist += 14 * dt; P.speed = 14; P.x = Math.sin(performance.now() / 1500) * 3; P.vx = Math.cos(performance.now() / 1500) * 2; P.yaw = -P.vx; P.roll = -P.vx * 0.5; P.pitch = 0; }

    /* road shape: smooth curves and hills depend on distance driven (gentle straight start) */
    var amp = Math.min(1, P.dist / 500), d = P.dist;
    var ct = amp * (0.95 * Math.sin(d * 0.0011) + 0.55 * Math.sin(d * 0.0029 + 1.0)), ht = amp * (0.75 * Math.sin(d * 0.0017 + 2.0) + 0.3 * Math.sin(d * 0.0043));
    curveV += (ct - curveV) * Math.min(1, 3 * dt); hillV += (ht - hillV) * Math.min(1, 3 * dt);
    Bend.set(curveV, hillV);
    if (state === 'play') P.x -= curveV * (P.speed / VMAX) * 1.3 * dt;       // curves push the car toward the outside
    Cars.setBrake(playerCar, state === 'play' && input.brake);
    var dz = P.speed * dt;
    World.update(world, dz);

    /* traffic movement + collision (AABB) + scoring */
    var me = { x: P.x, z: 0, hx: 0.9, hz: 2.05 };
    for (var i = traffic.length - 1; i >= 0; i--) {
      var c = traffic[i]; c.prevZ = c.z; c.z += (P.speed - c.speed) * dt;
      c.wheelRot += c.speed * dt;
      var box = { x: c.x, z: c.z, hx: c.hx * 0.95, hz: c.hz * 0.95 }, mid = { x: c.x, z: (c.z + c.prevZ) / 2, hx: box.hx, hz: box.hz };
      /* test current and mid-frame position (swept test avoids tunnelling at high closing speed) */
      if (state === 'play' && (CG.Collide.aabb(me, box) || CG.Collide.aabb(me, mid))) { crash(c); }
      if (state === 'play' && !c.done && c.prevZ < 0 && c.z >= 0) {         // car just passed beside us
        var gap = Math.abs(c.x - P.x) - (me.hx + c.hx);
        if (gap < 0.85) { P.near++; P.score += 100; toast('CLOSE CALL +100'); GameAudio.beep(880); }
      }
      if (state === 'play' && !c.done && c.z > c.hz + 2.3) { c.done = true; P.overtakes++; P.score += 50; toast('OVERTAKE +50'); GameAudio.beep(520); }
      if (c.z > 60 || c.z < -420) { scene.remove(c.mesh); traffic.splice(i, 1); }
    }

    /* build model matrices from OUR matrix library: M = T * R * Shear * S */
    var shearX = state === 'play' ? P.vx * 0.012 : 0;                          // lean-shear (x depends on y)
    var Rm = CG.M4.mul(CG.M4.rotY(P.yaw + P.spin), CG.M4.mul(CG.M4.rotZ(P.roll * 0.4), CG.M4.rotX(P.pitch * 0.5)));
    var SH = CG.M4.shear(shearX, 0, 0, 0, 0, 0);
    var Sc = CG.M4.scale(1, 1 - Math.max(0, P.pitch) * 0.01, 1);               // nose-dive squash
    var M = CG.M4.compose(CG.M4.translate(P.x, 0, 0), Rm, SH, Sc);
    playerCar.matrix.fromArray(M); playerCar.matrixWorldNeedsUpdate = true;
    mirrorCar.matrix.fromArray(CG.M4.mul(CG.M4.reflect('xz'), M)); mirrorCar.matrixWorldNeedsUpdate = true;   // reflection
    blob.position.x = P.x;
    spinWheels(playerCar, P.speed * dt, P.vx * 0.04); spinWheels(mirrorCar, P.speed * dt, P.vx * 0.04);
    traffic.forEach(function (c) {
      c.mesh.matrix.fromArray(CG.M4.translate(c.x, 0, c.z)); c.mesh.matrixWorldNeedsUpdate = true;
      spinWheels(c.mesh, c.speed * dt, 0);
    });
    updateCamera(dt);
    GameAudio.engine(P.speed, state === 'play' || state === 'menu');
    if (state === 'play') {
      $('hudScore').textContent = Math.floor(P.score); $('hudDist').textContent = (P.dist / 1000).toFixed(2);
    }
    HUD.drawGauge(P.speed * 3.6); HUD.drawRadar(P.x, traffic); HUD.drawSpeedLines(view === 2 ? 0 : P.speed);
  }
  function spinWheels(car, d, steer) {
    var w = car.userData.wheels;
    for (var i = 0; i < w.length; i++) { w[i].spin.rotation.x -= d / 0.38; if (w[i].front) w[i].pivot.rotation.y = -steer; }
  }

  function crash(c) {
    state = 'crash'; P.over = 0; shake = 1; GameAudio.crash(); $('flash').style.opacity = 0.7; setTimeout(function () { $('flash').style.opacity = 0; }, 150);
    P.vx = (P.x < c.x ? -1 : 1) * 5;
  }
  function gameOver() {
    state = 'over'; var s = Math.floor(P.score);
    if (s > best) { best = s; try { localStorage.setItem('hwBest', best); } catch (e) {} }
    $('goScore').textContent = s; $('goBest').textContent = best; $('goDist').textContent = (P.dist / 1000).toFixed(2) + ' km';
    $('goOver').textContent = P.overtakes; $('goNear').textContent = P.near; $('go').classList.remove('hide');
  }
  function toast(msg) {
    var e = $('toast'); e.textContent = msg; e.classList.remove('pop'); void e.offsetWidth; e.classList.add('pop');
  }

  /* ---- cameras ---- */
  function updateCamera(dt) {
    var cam = view === 2 ? ortho : persp, aspect = glCanvas.clientWidth / glCanvas.clientHeight;
    var sx = shake > 0 ? (Math.random() - 0.5) * shake * 1.2 : 0, sy = shake > 0 ? (Math.random() - 0.5) * shake * 0.8 : 0;
    shake = Math.max(0, shake - dt * 1.8);
    if (view === 0) {
      persp.fov = (58 + P.speed * 0.18) * (aspect < 1 ? 1.35 : 1);
      persp.position.set(P.x * 0.65 + sx, 3.5 + sy, 8.5 + P.speed * 0.02); persp.lookAt(P.x * 0.55, 1.4, -14);
    } else if (view === 1) {
      persp.fov = (70 + P.speed * 0.12) * (aspect < 1 ? 1.3 : 1);
      persp.position.set(P.x + sx * 0.4, 1.28 + sy * 0.4, -0.3); persp.lookAt(P.x, 1.15, -30);
    } else {
      var h = aspect < 1 ? 60 : 38; ortho.top = h; ortho.bottom = -h; ortho.left = -h * aspect; ortho.right = h * aspect;
      ortho.position.set(0, 120, -h * 0.35); ortho.up.set(0, 0, -1); ortho.lookAt(0, 0, -h * 0.35); ortho.updateProjectionMatrix();
    }
    persp.aspect = aspect; persp.updateProjectionMatrix();
    nightAmt = Sky.update(sky, dt, cam.position); cones.forEach(function (c) { c.material.opacity = 0.09 * nightAmt; });
    if (window.__cam) window.__cam(persp);          // test hook only (close-up screenshots)
    cam.updateMatrixWorld();
    playerCar.visible = !(view === 1) || state !== 'play';
    renderer.render(scene, cam);
  }

  /* ---- resize ---- */
  function resize() {
    var w = window.innerWidth, h = window.innerHeight; renderer.setSize(w, h, false);
    document.body.classList.toggle('portrait', h > w);
  }
  window.addEventListener('resize', resize); resize();

  /* ---- input: keyboard ---- */
  var KEYS = { ArrowLeft: 'left', a: 'left', A: 'left', ArrowRight: 'right', d: 'right', D: 'right', ArrowUp: 'gas', w: 'gas', W: 'gas', ArrowDown: 'brake', s: 'brake', S: 'brake', ' ': 'brake' };
  window.addEventListener('keydown', function (e) {
    if (KEYS[e.key]) { input[KEYS[e.key]] = true; e.preventDefault(); }
    if (e.key === 'c' || e.key === 'C') changeView();
    if (e.key === 'm' || e.key === 'M') $('btnMute').click();
    if (e.key === 'p' || e.key === 'P') togglePause();
    if ((e.key === 'Enter' || e.key === 'r' || e.key === 'R') && (state === 'menu' || state === 'over')) startGame();
    GameAudio.start();
  });
  window.addEventListener('keyup', function (e) { if (KEYS[e.key]) input[KEYS[e.key]] = false; });

  /* ---- input: touch / mouse buttons (pointer events, multi-touch safe) ---- */
  function bindHold(id, key) {
    var el = $(id);
    function on(e) { input[key] = true; el.classList.add('down'); e.preventDefault(); GameAudio.start(); }
    function off(e) { input[key] = false; el.classList.remove('down'); e.preventDefault(); }
    el.addEventListener('pointerdown', on); el.addEventListener('pointerup', off);
    el.addEventListener('pointercancel', off); el.addEventListener('pointerleave', off);
  }
  bindHold('bLeft', 'left'); bindHold('bRight', 'right'); bindHold('bGas', 'gas'); bindHold('bBrake', 'brake');
  function changeView() { view = (view + 1) % 3; $('btnView').textContent = 'VIEW: ' + ['CHASE', 'COCKPIT', 'TOP'][view]; $('viewName').textContent = VIEWS[view]; }
  function togglePause() {
    if (state === 'play') { state = 'pause'; $('pause').classList.remove('hide'); }
    else if (state === 'pause') { state = 'play'; $('pause').classList.add('hide'); }
  }
  $('btnFull').onclick = function () { var d = document.documentElement; try { (document.fullscreenElement ? document.exitFullscreen() : (d.requestFullscreen || d.webkitRequestFullscreen).call(d)); } catch (e) {} };
  $('btnTime').onclick = function () { timeIdx = (timeIdx + 1) % 4; sky.mode = timeModes[timeIdx]; this.textContent = 'TIME: ' + (timeIdx ? Sky.THEMES[timeModes[timeIdx]].name : 'AUTO'); };
  $('btnView').onclick = changeView; $('btnPause').onclick = togglePause; $('pause').onclick = togglePause;
  $('btnMute').onclick = function () { this.textContent = GameAudio.toggle() ? 'SOUND: OFF' : 'SOUND: ON'; };
  $('btnTilt').onclick = function () {
    function enable() { tilt = !tilt; $('btnTilt').textContent = 'TILT: ' + (tilt ? 'ON' : 'OFF'); }
    if (window.DeviceOrientationEvent && DeviceOrientationEvent.requestPermission) {
      DeviceOrientationEvent.requestPermission().then(function (r) { if (r === 'granted') enable(); }).catch(function () {});
    } else enable();
  };
  window.addEventListener('deviceorientation', function (e) {
    var g = e.gamma || 0, o = window.orientation, b = e.beta || 0;     // landscape uses beta
    tiltVal = (Math.abs(o) === 90 ? (o === 90 ? b : -b) : g) / 22;
  });
  $('btnStart').onclick = startGame; $('btnRestart').onclick = startGame;
  function startGame() { GameAudio.start(); reset(); state = 'play'; $('menu').classList.add('hide'); P.speed = 18; }

  /* ---- main loop with fixed upper bound on dt ---- */
  reset(); $('btnView').textContent = 'VIEW: CHASE'; $('viewName').textContent = VIEWS[0];
  $('hudBest').textContent = best;
  var last = performance.now();
  function frame(now) {
    var dt = window.__fixedDt || Math.max(0.001, Math.min(0.05, (now - last) / 1000)); last = now;   // __fixedDt: used only by automated tests
    if (window.__manualStep == null && state !== 'pause') update(dt);
    $('hudBest').textContent = Math.max(best, Math.floor(P.score));
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  /* debug / test hook (used by the automated browser test) */
  window.RACE = { get state() { return state; }, get P() { return P; }, traffic: function () { return traffic; }, input: input, start: startGame, step: update, spawn: spawn, setView: function (v) { view = v; }, setTime: function (i) { sky.mode = i; sky.cur.top.copy(Sky.THEMES[i].top); }, curve: function () { return [curveV, hillV]; }, skyObj: sky, lanes: World };
})();

/* =====================================================================
 * cars.js - 3D car / truck models
 * Bodies are made by EXTRUDING a side-profile polygon (with a rounded
 * bevel) instead of using plain boxes; then glass, wheels with spokes,
 * wheel arches, lights, mirrors and exhausts are added.  A small generated
 * sky "environment map" gives the paint a glossy reflection.
 * Local model space:  +x right, +y up, -z = forward (nose), +z = rear.
 * Authors : Sami (B25, R54139) & Zaid (B29, R54231)
 * ===================================================================== */
var Cars = (function () {
  var T = THREE;

  /* ---- procedural environment map (sky above, grass below) ---- */
  function makeEnv() {
    function face(top, mid, bot) {
      var c = document.createElement('canvas'); c.width = c.height = 64; var x = c.getContext('2d');
      var g = x.createLinearGradient(0, 0, 0, 64); g.addColorStop(0, top); g.addColorStop(0.5, mid); g.addColorStop(0.52, bot); g.addColorStop(1, bot);
      x.fillStyle = g; x.fillRect(0, 0, 64, 64); return c;
    }
    function flat(col) { var c = document.createElement('canvas'); c.width = c.height = 64; var x = c.getContext('2d'); x.fillStyle = col; x.fillRect(0, 0, 64, 64); return c; }
    var side = face('#5d9be0', '#e8f1fb', '#4a6a3a');
    var t = new T.CubeTexture([side, side, flat('#7fb4ee'), flat('#3d5a2e'), side, side]); t.needsUpdate = true; return t;
  }
  var ENV = makeEnv();

  var glass = new T.MeshStandardMaterial({ color: 0x0d1822, metalness: 0.9, roughness: 0.08, envMap: ENV, envMapIntensity: 1.3, polygonOffset: true, polygonOffsetFactor: -2, side: T.DoubleSide });
  var tyre = new T.MeshLambertMaterial({ color: 0x141414 });
  var rimMat = new T.MeshStandardMaterial({ color: 0xd9dde3, metalness: 0.9, roughness: 0.25, envMap: ENV });
  var dark = new T.MeshLambertMaterial({ color: 0x0b0b0b, side: T.DoubleSide });
  var chrome = new T.MeshStandardMaterial({ color: 0xcccccc, metalness: 1, roughness: 0.2, envMap: ENV });
  var headMat = new T.MeshBasicMaterial({ color: 0xfff4c2 });
  var plateMat = new T.MeshLambertMaterial({ color: 0xf2f2f2 });
  var geoBox = new T.BoxGeometry(1, 1, 1);
  var geoTyre = new T.CylinderGeometry(0.38, 0.38, 0.27, 22);
  var geoRim = new T.CylinderGeometry(0.25, 0.25, 0.285, 16);
  var geoSpoke = new T.BoxGeometry(0.02, 0.46, 0.045);
  var geoHub = new T.CylinderGeometry(0.07, 0.07, 0.3, 10);
  var geoArch = new T.RingGeometry(0.385, 0.52, 28);

  function box(parent, mat, w, h, d, x, y, z) {
    var m = new T.Mesh(geoBox, mat); m.scale.set(w, h, d); m.position.set(x, y, z); parent.add(m); return m;
  }
  /* extrude a side profile [[len,height],...] (len>0 = toward the nose) to a given width */
  function extrude(pts, width, mats, bevel) {
    var sh = new T.Shape(); pts.forEach(function (p, i) { i ? sh.lineTo(p[0], p[1]) : sh.moveTo(p[0], p[1]); }); sh.closePath();
    var b = bevel === undefined ? 0.1 : bevel;
    var g = new T.ExtrudeGeometry(sh, { depth: Math.max(0.01, width - 2 * b), bevelEnabled: b > 0, bevelThickness: b, bevelSize: b * 0.9, bevelSegments: 3, curveSegments: 4 });
    g.computeBoundingBox(); g.translate(0, 0, -(g.boundingBox.max.z + g.boundingBox.min.z) / 2);
    g.rotateY(Math.PI / 2);                       // shape x -> -z (forward), extrusion axis -> +x (width)
    return new T.Mesh(g, mats);
  }
  /* slanted glass quad (windscreen / rear window) between two profile points */
  function slant(car, a, b, width, zShift) {
    var dy = b[1] - a[1], dz = -(b[0] - a[0]), len = Math.sqrt(dy * dy + dz * dz);
    var m = new T.Mesh(new T.PlaneGeometry(width, len), glass);
    m.position.set(0, (a[1] + b[1]) / 2, -(a[0] + b[0]) / 2 + (zShift || 0)); m.rotation.x = Math.atan2(dz, dy); car.add(m); return m;
  }
  /* zs = z positions of axles; the first axle in 'steerCount' are the steering ones */
  function wheels(car, hw, zs, r, steerCount, bodyHalf) {
    car.userData.wheels = [];
    zs.forEach(function (z, k) {
      [-1, 1].forEach(function (sd) {
        var pivot = new T.Group(); pivot.position.set(sd * hw, r, z);
        var spin = new T.Group(); var s = r / 0.38; spin.scale.set(s, s, s);
        var w = new T.Mesh(geoTyre, tyre); w.rotation.z = Math.PI / 2;
        var rim = new T.Mesh(geoRim, rimMat); rim.rotation.z = Math.PI / 2; spin.add(w); spin.add(rim);
        for (var i = 0; i < 5; i++) {                                  // five spokes, visible on the outer face
          var sp = new T.Mesh(geoSpoke, rimMat); sp.rotation.x = i * Math.PI / 5; sp.position.x = sd * 0.138; spin.add(sp);
        }
        var hub = new T.Mesh(geoHub, chrome); hub.rotation.z = Math.PI / 2; spin.add(hub);
        pivot.add(spin); car.add(pivot);
        var arch = new T.Mesh(geoArch, dark); arch.rotation.y = sd * Math.PI / 2; arch.scale.set(r / 0.38, r / 0.38, 1);
        arch.position.set(sd * ((bodyHalf || hw) + 0.012), r, z); car.add(arch);        // dark wheel-arch disc behind the tyre
        car.userData.wheels.push({ pivot: pivot, spin: spin, front: k < steerCount });
      });
    });
  }
  function lights(car, hw, zf, zr, y, big) {
    var tailMat = new T.MeshBasicMaterial({ color: 0x9a0d0d }); car.userData.tail = tailMat;
    [-1, 1].forEach(function (s) {
      box(car, dark, big ? 0.56 : 0.5, 0.2, 0.05, s * hw * 0.68, y, zf + 0.0);
      box(car, headMat, big ? 0.5 : 0.42, 0.13, 0.08, s * hw * 0.68, y, zf - 0.01);
      box(car, tailMat, 0.5, 0.13, 0.08, s * hw * 0.7, y, zr);
    });
    box(car, tailMat, hw * 0.8, 0.05, 0.06, 0, y, zr + 0.01);          // light bar between tail lamps
    box(car, plateMat, 0.5, 0.14, 0.03, 0, y - 0.22, zr + 0.03);       // number plate
    box(car, dark, hw * 0.9, 0.13, 0.05, 0, y - 0.2, zf - 0.02);       // grille
  }
  function mirrors(car, x, y, z, mat) { [-1, 1].forEach(function (s) { box(car, mat, 0.18, 0.1, 0.12, s * x, y, z); }); }

  /* profiles: lower body and cabin (greenhouse) as [length, height] points */
  var P = {
    sport: { L: 4.4, W: 1.92,
      body: [[-2.2,0.32],[2.15,0.32],[2.22,0.5],[1.75,0.66],[1.0,0.74],[-1.0,0.8],[-1.85,0.8],[-2.2,0.7]],
      cab: [[1.05,0.74],[0.4,1.13],[-0.7,1.15],[-1.45,0.82],[-1.0,0.78]], cw: 1.56 },
    sedan: { L: 4.6, W: 1.88,
      body: [[-2.3,0.34],[2.25,0.34],[2.3,0.52],[1.85,0.7],[1.2,0.8],[-1.4,0.84],[-2.15,0.84],[-2.3,0.72]],
      cab: [[0.95,0.8],[0.35,1.28],[-0.9,1.3],[-1.55,0.86],[-1.4,0.84]], cw: 1.58 },
    suv: { L: 4.5, W: 1.95,
      body: [[-2.25,0.36],[2.2,0.36],[2.27,0.62],[1.8,0.85],[1.2,0.92],[-2.15,0.95],[-2.27,0.8]],
      cab: [[1.0,0.92],[0.6,1.5],[-2.0,1.52],[-2.15,0.95]], cw: 1.68 }
  };

  /* type: 'sport' | 'sedan' | 'suv' | 'truck' */
  function build(color, type) {
    var car = new T.Group(); car.userData.type = type;
    var body = new T.MeshStandardMaterial({ color: color, metalness: 0.55, roughness: 0.28, envMap: ENV, envMapIntensity: 1.0 });
    if (type === 'truck') {
      var ribMat = new T.MeshLambertMaterial({ color: 0xcfd4da });
      box(car, new T.MeshLambertMaterial({ color: 0x2b2f35 }), 2.1, 0.35, 9.0, 0, 0.62, 0.9);                 // chassis
      box(car, new T.MeshLambertMaterial({ color: 0xe9ecef }), 2.4, 2.9, 7.0, 0, 2.2, 1.7);                   // container
      for (var i = 0; i < 6; i++) box(car, ribMat, 2.44, 2.9, 0.07, 0, 2.2, -1.5 + i * 1.3);                  // ribs
      box(car, body, 2.46, 0.45, 7.02, 0, 1.2, 1.7);                                                          // coloured stripe
      var cab = extrude([[-1.2,0.6],[1.2,0.6],[1.25,1.5],[0.9,2.4],[-1.2,2.5]], 2.3, [glass, body], 0.12);
      cab.position.set(0, 0, -3.2); car.add(cab);
      slant(car, [1.25, 1.5], [0.9, 2.4], 2.0, -3.2);                                                         // windscreen
      box(car, dark, 2.3, 0.35, 0.12, 0, 0.75, -4.55);                                                        // front bumper
      box(car, chrome, 1.6, 0.4, 0.06, 0, 1.2, -4.47);                                                        // grille
      mirrors(car, 1.38, 2.0, -3.9, dark);
      [-1, 1].forEach(function (s) { box(car, dark, 0.5, 0.5, 0.05, s * 1.0, 0.5, 5.3); });                   // mud flaps
      wheels(car, 1.0, [-3.1, 3.4, 4.6], 0.5, 1, 1.2);
      lights(car, 1.1, -4.5, 5.45, 0.95, true);
      car.userData.hx = 1.2; car.userData.hz = 5.0;
      return car;
    }
    var p = P[type]; car.add(extrude(p.body, p.W, body, 0.12));
    car.add(extrude(p.cab, p.cw, [glass, body], 0.06));
    var A = p.cab[0], B = p.cab[1], C = p.cab[2], D = p.cab[3];
    slant(car, A, B, p.cw - 0.1);                                           // windscreen
    slant(car, C, D, p.cw - 0.1);                                           // rear window
    box(car, dark, p.W - 0.1, 0.1, p.L - 0.2, 0, 0.34, 0);                  // under-body shadow strip
    box(car, chrome, p.W - 0.05, 0.025, 0.05, 0, 0.56, -p.L / 2 - 0.15);    // front trim
    mirrors(car, p.W / 2 + 0.06, A[1] + 0.1, -(A[0] - 0.15), body);
    /* door seams, handles, B-pillar, splitter, diffuser, side skirts */
    var zA = -(A[0] - 0.12), zD = -(D[0] + 0.2), zM = -((A[0] + D[0]) / 2);
    [-1, 1].forEach(function (sd) {
      var xs = sd * (p.W / 2 + 0.006);
      box(car, dark, 0.012, 0.36, 0.014, xs, 0.6, zA); box(car, dark, 0.012, 0.36, 0.014, xs, 0.6, zD); box(car, dark, 0.012, 0.012, Math.abs(zD - zA), xs, 0.78, (zA + zD) / 2);
      box(car, chrome, 0.02, 0.025, 0.16, xs, 0.72, zM + 0.3); box(car, chrome, 0.02, 0.025, 0.16, xs, 0.72, zM - 0.55);
      box(car, dark, 0.03, 0.09, p.L * 0.55, sd * (p.W / 2 - 0.02), 0.38, 0);
    });
    box(car, body, p.cw + 0.01, (B[1] - A[1]) * 0.9, 0.07, 0, (A[1] + B[1]) / 2 + 0.02, zM);
    box(car, dark, p.W + 0.04, 0.05, 0.42, 0, 0.33, -p.L / 2 - 0.02);
    box(car, dark, p.W - 0.35, 0.14, 0.3, 0, 0.38, p.L / 2 + 0.06);
    if (type === 'sport') {
      box(car, body, 1.8, 0.06, 0.5, 0, 1.2, p.L / 2 - 0.35);               // rear wing
      [-0.62, 0.62].forEach(function (x) { box(car, dark, 0.07, 0.3, 0.1, x, 1.02, p.L / 2 - 0.35); });
      var stripe = new T.MeshBasicMaterial({ color: 0xf5f5f5 });
      box(car, stripe, 0.22, 0.012, 1.55, 0, 0.745, -1.2);                  // racing stripe on the bonnet
      [-0.45, 0.45].forEach(function (x) { var e = new T.Mesh(new T.CylinderGeometry(0.06, 0.06, 0.2, 10), chrome); e.rotation.x = Math.PI / 2; e.position.set(x, 0.42, p.L / 2 + 0.12); car.add(e); });
    }
    wheels(car, p.W / 2 - 0.1, [-p.L / 2 + 0.95, p.L / 2 - 0.95], type === 'suv' ? 0.42 : 0.38, 1, p.W / 2 + 0.01);
    lights(car, p.W / 2 - 0.02, -p.L / 2 - 0.16, p.L / 2 + 0.16, 0.62);
    car.userData.hx = p.W / 2; car.userData.hz = p.L / 2;
    return car;
  }
  function setBrake(car, on) { if (car.userData.tail) car.userData.tail.color.setHex(on ? 0xff5a3a : 0x9a0d0d); }
  return { build: build, setBrake: setBrake };
})();

/* =====================================================================
 * hud.js - heads-up display drawn with our own rasterizer
 *   Speedometer : midpoint circle (dial), Bresenham (ticks), DDA (needle,
 *                 clipped with Cohen-Sutherland against the dial window)
 *   Radar       : world->screen transform with a 3x3 matrix, Bresenham lanes
 *   Speed lines : DDA lines radiating from screen centre, clipped to screen
 * Authors : Sami (B25, R54139) & Zaid (B29, R54231)
 * ===================================================================== */
var HUD = (function () {
  var gaugeC, gaugeX, gauge, radarC, radarX, radar, fxC, fxX, fx, MAXKMH = 300;
  function init() {
    gaugeC = document.getElementById('gauge'); gaugeX = gaugeC.getContext('2d'); gauge = new CG.Raster(gaugeC.width, gaugeC.height);
    radarC = document.getElementById('radar'); radarX = radarC.getContext('2d'); radar = new CG.Raster(radarC.width, radarC.height);
    fxC = document.getElementById('fx');       fxX = fxC.getContext('2d');       fx = new CG.Raster(fxC.width, fxC.height);
  }
  function ang(kmh) { return (225 - 270 * Math.min(kmh, MAXKMH) / MAXKMH) * Math.PI / 180; }

  function drawGauge(kmh) {
    var r = gauge, cx = 90, cy = 90, R = 84, i;
    r.clear(0, 0, 0, 0);
    r.fillCircle(cx, cy, R, [10, 14, 22, 190]);                       // dial face
    var ring = [230, 235, 245, 255], red = [255, 60, 50, 255];
    r.circleMidpoint(cx, cy, R, ring); r.circleMidpoint(cx, cy, R - 1, ring);
    r.circleMidpoint(cx, cy, R - 12, [120, 130, 150, 255], function (x, y, c) {   // inner arc, red zone > 240
      var a = Math.atan2(cy - y, x - cx), deg = (a * 180 / Math.PI + 360) % 360;
      var kmhHere = (225 - deg + 360) % 360 * MAXKMH / 270;
      if (kmhHere <= MAXKMH + 1 && deg > 0) r.putPixel(x, y, kmhHere > 240 ? red : c);
    });
    for (i = 0; i <= MAXKMH; i += 20) {                                // ticks (Bresenham)
      var a = ang(i), big = i % 60 === 0, r1 = R - (big ? 18 : 10), r2 = R - 4;
      r.lineBresenham(cx + r1 * Math.cos(a), cy - r1 * Math.sin(a), cx + r2 * Math.cos(a), cy - r2 * Math.sin(a), i > 240 ? red : ring);
    }
    var na = ang(kmh), len = R - 16;                                   // needle (DDA + clipping)
    var nx = cx + len * Math.cos(na), ny = cy - len * Math.sin(na);
    var win = [3, 3, gaugeC.width - 4, gaugeC.height - 4];
    for (var t = -1; t <= 1; t++) r.clippedLine(cx + t, cy, nx + t, ny, win, [255, 190, 40, 255], true);
    r.fillCircle(cx, cy, 6, [255, 190, 40, 255]); r.fillCircle(cx, cy, 2, [20, 20, 20, 255]);
    gaugeX.putImageData(r.img, 0, 0);
    gaugeX.fillStyle = '#fff'; gaugeX.textAlign = 'center'; gaugeX.font = 'bold 9px Arial';
    for (i = 0; i <= MAXKMH; i += 60) { var q = ang(i), rr = R - 28; gaugeX.fillText(i, cx + rr * Math.cos(q), cy - rr * Math.sin(q) + 3); }
    gaugeX.font = 'bold 26px Arial'; gaugeX.fillText(Math.round(kmh), cx, cy + 40);
    gaugeX.font = 'bold 9px Arial'; gaugeX.fillStyle = '#9fb0c8'; gaugeX.fillText('km/h', cx, cy + 52);
  }

  function drawRadar(px, traffic) {
    var r = radar, W = r.w, H = r.h;
    var M = CG.M3.mul(CG.M3.translate(W / 2, H * 0.75), CG.M3.scale(W / World.ROAD_W, 1.15)); // world -> radar pixels
    r.clear(8, 12, 20, 170);
    var l = 0, i, p;
    for (i = 1; i < World.LANES; i++) {                                // lane lines
      p = CG.M3.apply(M, -World.ROAD_HALF + i * World.LANE_W, 0);
      for (var y = 0; y < H; y += 8) r.lineBresenham(p.x, y, p.x, y + 3, [90, 100, 120, 255]);
    }
    for (i = 0; i < traffic.length; i++) {
      var c = traffic[i]; p = CG.M3.apply(M, c.x, c.z);
      var hw = c.hx * W / World.ROAD_W, hh = c.hz * 1.15;
      if (p.y > -10 && p.y < H + 10) r.fillRect(Math.round(p.x - hw), Math.round(p.y - hh), Math.round(p.x + hw), Math.round(p.y + hh), [255, 90, 70, 255]);
    }
    p = CG.M3.apply(M, px, 0);
    r.fillRect(Math.round(p.x - 4), Math.round(p.y - 5), Math.round(p.x + 4), Math.round(p.y + 5), [80, 220, 255, 255]);
    r.rect(0, 0, W - 1, H - 1, [200, 210, 230, 255]);
    radarX.putImageData(r.img, 0, 0);
  }

  function drawSpeedLines(speed) {
    fx.clear(0, 0, 0, 0);
    var s = Math.max(0, (speed - 38) / 40);
    if (s > 0) {
      var W = fx.w, H = fx.h, cx = W / 2, cy = H * 0.45, n = Math.floor(6 + s * 14);
      var win = [0, 0, W - 1, H - 1], col = [255, 255, 255, Math.min(80, 20 + s * 55)];
      for (var i = 0; i < n; i++) {
        var a = Math.random() * Math.PI * 2, r1 = 28 + Math.random() * 45, r2 = r1 + 20 + Math.random() * 50 * s + 20;
        fx.clippedLine(cx + r1 * Math.cos(a) * 1.7, cy + r1 * Math.sin(a), cx + r2 * Math.cos(a) * 1.7, cy + r2 * Math.sin(a), win, col, false);
      }
    }
    fxX.putImageData(fx.img, 0, 0);
  }
  return { init: init, drawGauge: drawGauge, drawRadar: drawRadar, drawSpeedLines: drawSpeedLines };
})();

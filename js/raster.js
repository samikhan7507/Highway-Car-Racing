/* =====================================================================
 * raster.js  -  Software rasterizer (pixel buffer) written from scratch
 *   putPixel, DDA line, Bresenham line, midpoint circle,
 *   Cohen-Sutherland line clipping, filled triangle (edge functions).
 * Everything draws straight into a Uint8ClampedArray (ImageData), the
 * same way a frame-buffer works in graphics hardware.
 * Authors : Sami (B25, R54139) & Zaid (B29, R54231)
 * ===================================================================== */
var CG = CG || {};

CG.Raster = function (w, h) {
  this.w = w; this.h = h;
  this.img = new ImageData(w, h);
  this.px = this.img.data;
};

CG.Raster.prototype = {
  clear: function (r, g, b, a) {
    var p = this.px, n = p.length;
    for (var i = 0; i < n; i += 4) { p[i]=r; p[i+1]=g; p[i+2]=b; p[i+3]=a; }
  },
  /* col = [r,g,b,a] ; pixels outside the buffer are rejected */
  putPixel: function (x, y, col) {
    x = x | 0; y = y | 0;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    var i = (y * this.w + x) * 4, p = this.px;
    p[i] = col[0]; p[i+1] = col[1]; p[i+2] = col[2]; p[i+3] = col[3];
  },

  /* ---- DDA (Digital Differential Analyzer) ----------------------------
     steps = max(|dx|,|dy|); x += dx/steps ; y += dy/steps  (floating point) */
  lineDDA: function (x1, y1, x2, y2, col, plot) {
    var dx = x2 - x1, dy = y2 - y1;
    var steps = Math.max(Math.abs(dx), Math.abs(dy));
    var xi = steps ? dx / steps : 0, yi = steps ? dy / steps : 0;
    var x = x1, y = y1, put = plot || this.putPixel.bind(this);
    for (var i = 0; i <= steps; i++) {
      put(Math.round(x), Math.round(y), col);
      x += xi; y += yi;
    }
    return steps + 1;                        // number of pixels plotted
  },

  /* ---- Bresenham line (integer only, all 8 octants) ------------------- */
  lineBresenham: function (x1, y1, x2, y2, col, plot) {
    x1 = Math.round(x1); y1 = Math.round(y1); x2 = Math.round(x2); y2 = Math.round(y2);
    var dx = Math.abs(x2 - x1), dy = Math.abs(y2 - y1);
    var sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    var err = dx - dy, n = 0, put = plot || this.putPixel.bind(this);
    for (;;) {
      put(x1, y1, col); n++;
      if (x1 === x2 && y1 === y2) break;
      var e2 = 2 * err;                      // decision parameter
      if (e2 > -dy) { err -= dy; x1 += sx; }
      if (e2 <  dx) { err += dx; y1 += sy; }
    }
    return n;
  },

  /* ---- Midpoint circle: compute 1 octant, mirror by 8-way symmetry ---- */
  circleMidpoint: function (cx, cy, r, col, plot) {
    var x = 0, y = r, p = 1 - r, n = 0, put = plot || this.putPixel.bind(this);
    while (x <= y) {
      put(cx+x,cy+y,col); put(cx-x,cy+y,col); put(cx+x,cy-y,col); put(cx-x,cy-y,col);
      put(cx+y,cy+x,col); put(cx-y,cy+x,col); put(cx+y,cy-x,col); put(cx-y,cy-x,col);
      n += 8; x++;
      if (p < 0) p += 2 * x + 1;             // midpoint inside  -> choose E
      else { y--; p += 2 * (x - y) + 1; }    // midpoint outside -> choose SE
    }
    return n;
  },
  fillCircle: function (cx, cy, r, col) {    // scan-lines from the same midpoint loop
    var x = 0, y = r, p = 1 - r, self = this;
    function span(x1, x2, yy) { for (var i = x1; i <= x2; i++) self.putPixel(i, yy, col); }
    while (x <= y) {
      span(cx-x,cx+x,cy+y); span(cx-x,cx+x,cy-y); span(cx-y,cx+y,cy+x); span(cx-y,cx+y,cy-x);
      x++;
      if (p < 0) p += 2*x + 1; else { y--; p += 2*(x-y) + 1; }
    }
  },
  rect: function (x1, y1, x2, y2, col) {
    this.lineBresenham(x1,y1,x2,y1,col); this.lineBresenham(x2,y1,x2,y2,col);
    this.lineBresenham(x2,y2,x1,y2,col); this.lineBresenham(x1,y2,x1,y1,col);
  },
  fillRect: function (x1, y1, x2, y2, col) {
    for (var y = y1; y <= y2; y++) for (var x = x1; x <= x2; x++) this.putPixel(x, y, col);
  },

  /* ---- Cohen-Sutherland line clipping --------------------------------- */
  /* region code bits: 1=LEFT 2=RIGHT 4=BOTTOM 8=TOP. Returns [x1,y1,x2,y2] or null */
  outcode: function (x, y, xmin, ymin, xmax, ymax) {
    var c = 0;
    if (x < xmin) c |= 1; else if (x > xmax) c |= 2;
    if (y < ymin) c |= 4; else if (y > ymax) c |= 8;
    return c;
  },
  clipLine: function (x1, y1, x2, y2, xmin, ymin, xmax, ymax) {
    var c1 = this.outcode(x1,y1,xmin,ymin,xmax,ymax), c2 = this.outcode(x2,y2,xmin,ymin,xmax,ymax);
    for (var it = 0; it < 12; it++) {
      if (!(c1 | c2)) return [x1, y1, x2, y2];     // trivially accepted
      if (c1 & c2) return null;                    // trivially rejected
      var c = c1 ? c1 : c2, x, y;
      if (c & 8)      { x = x1 + (x2-x1)*(ymax-y1)/(y2-y1); y = ymax; }
      else if (c & 4) { x = x1 + (x2-x1)*(ymin-y1)/(y2-y1); y = ymin; }
      else if (c & 2) { y = y1 + (y2-y1)*(xmax-x1)/(x2-x1); x = xmax; }
      else            { y = y1 + (y2-y1)*(xmin-x1)/(x2-x1); x = xmin; }
      if (c === c1) { x1 = x; y1 = y; c1 = this.outcode(x,y,xmin,ymin,xmax,ymax); }
      else          { x2 = x; y2 = y; c2 = this.outcode(x,y,xmin,ymin,xmax,ymax); }
    }
    return null;
  },
  /* convenience: clip, then draw with Bresenham (or DDA) */
  clippedLine: function (x1, y1, x2, y2, win, col, useDDA) {
    var c = this.clipLine(x1, y1, x2, y2, win[0], win[1], win[2], win[3]);
    if (!c) return false;
    if (useDDA) this.lineDDA(c[0],c[1],c[2],c[3],col); else this.lineBresenham(c[0],c[1],c[2],c[3],col);
    return true;
  },

  /* ---- Triangle rasterization with edge functions (barycentric) ------- */
  fillTriangle: function (a, b, c, col) {
    var minx = Math.max(0, Math.floor(Math.min(a.x,b.x,c.x))), maxx = Math.min(this.w-1, Math.ceil(Math.max(a.x,b.x,c.x)));
    var miny = Math.max(0, Math.floor(Math.min(a.y,b.y,c.y))), maxy = Math.min(this.h-1, Math.ceil(Math.max(a.y,b.y,c.y)));
    function edge(p, q, x, y) { return (q.x-p.x)*(y-p.y) - (q.y-p.y)*(x-p.x); }
    var area = edge(a, b, c.x, c.y); if (!area) return;
    for (var y = miny; y <= maxy; y++)
      for (var x = minx; x <= maxx; x++) {
        var w0 = edge(b,c,x+.5,y+.5)/area, w1 = edge(c,a,x+.5,y+.5)/area, w2 = edge(a,b,x+.5,y+.5)/area;
        if (w0 >= 0 && w1 >= 0 && w2 >= 0) this.putPixel(x, y, col);
      }
  }
};

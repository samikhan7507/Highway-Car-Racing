/* =====================================================================
 * cgmath.js  -  Computer Graphics math written from scratch
 * Project : 3D Highway Car Racing  (Computer Graphics Mini Project)
 * Authors : Sami (B25, R54139)  &  Zaid (B29, R54231)
 *
 * Contents
 *   CG.M3  : 3x3 homogeneous matrices for 2D transformations
 *   CG.M4  : 4x4 homogeneous matrices for 3D transformations
 *            (column-major, so they can be handed to WebGL / three.js)
 *   CG.Collide : AABB and circle collision tests
 *
 * Convention: a point is a column vector, p' = M * p.
 * Composite transform  M = M2 * M1  means  "apply M1 first, then M2".
 * ===================================================================== */
var CG = CG || {};

/* ---------------- 2D : 3x3 homogeneous (row-major arrays of 9) ------- */
CG.M3 = {
  identity: function () { return [1,0,0, 0,1,0, 0,0,1]; },
  mul: function (a, b) {                       // returns a * b
    var r = [0,0,0, 0,0,0, 0,0,0];
    for (var i = 0; i < 3; i++)
      for (var j = 0; j < 3; j++)
        for (var k = 0; k < 3; k++) r[i*3+j] += a[i*3+k] * b[k*3+j];
    return r;
  },
  translate: function (tx, ty) { return [1,0,tx, 0,1,ty, 0,0,1]; },
  scale: function (sx, sy)     { return [sx,0,0, 0,sy,0, 0,0,1]; },
  rotate: function (deg) {                     // counter-clockwise, about origin
    var t = deg * Math.PI / 180, c = Math.cos(t), s = Math.sin(t);
    return [c,-s,0, s,c,0, 0,0,1];
  },
  reflectX: function () { return [1,0,0, 0,-1,0, 0,0,1]; },   // about x-axis
  reflectY: function () { return [-1,0,0, 0,1,0, 0,0,1]; },   // about y-axis
  reflectOrigin: function () { return [-1,0,0, 0,-1,0, 0,0,1]; },
  reflectLineYX: function () { return [0,1,0, 1,0,0, 0,0,1]; }, // about y = x
  shear: function (shx, shy) { return [1,shx,0, shy,1,0, 0,0,1]; },
  /* rotate/scale about an arbitrary pivot: T(p) * M * T(-p) */
  about: function (px, py, m) {
    return CG.M3.mul(CG.M3.translate(px,py), CG.M3.mul(m, CG.M3.translate(-px,-py)));
  },
  apply: function (m, x, y) {
    var w = m[6]*x + m[7]*y + m[8];
    return { x: (m[0]*x + m[1]*y + m[2]) / w, y: (m[3]*x + m[4]*y + m[5]) / w };
  }
};

/* ---------------- 3D : 4x4 homogeneous (column-major, 16) ------------ */
CG.M4 = {
  identity: function () { return [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]; },
  /* element (row r, col c) lives at index c*4 + r */
  mul: function (a, b) {                       // returns a * b
    var o = new Array(16);
    for (var c = 0; c < 4; c++)
      for (var r = 0; r < 4; r++) {
        var s = 0;
        for (var k = 0; k < 4; k++) s += a[k*4+r] * b[c*4+k];
        o[c*4+r] = s;
      }
    return o;
  },
  translate: function (x, y, z) { return [1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1]; },
  scale: function (x, y, z)     { return [x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1]; },
  rotX: function (deg) { var t=deg*Math.PI/180,c=Math.cos(t),s=Math.sin(t);
    return [1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1]; },
  rotY: function (deg) { var t=deg*Math.PI/180,c=Math.cos(t),s=Math.sin(t);
    return [c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1]; },
  rotZ: function (deg) { var t=deg*Math.PI/180,c=Math.cos(t),s=Math.sin(t);
    return [c,s,0,0, -s,c,0,0, 0,0,1,0, 0,0,0,1]; },
  /* reflection through a coordinate plane: 'xy' flips z, 'yz' flips x, 'xz' flips y */
  reflect: function (plane) {
    return plane === 'xy' ? CG.M4.scale(1,1,-1) :
           plane === 'yz' ? CG.M4.scale(-1,1,1) : CG.M4.scale(1,-1,1);
  },
  /* general shear:  x' = x + a*y + b*z ;  y' = y + c*x + d*z ; z' = z + e*x + f*y */
  shear: function (a, b, c, d, e, f) {
    return [1,c,e,0,  a,1,f,0,  b,d,1,0,  0,0,0,1];
  },
  apply: function (m, p) {                     // p = [x,y,z] ; returns [x,y,z,w]
    var x=p[0], y=p[1], z=p[2];
    return [m[0]*x+m[4]*y+m[8]*z+m[12], m[1]*x+m[5]*y+m[9]*z+m[13],
            m[2]*x+m[6]*y+m[10]*z+m[14], m[3]*x+m[7]*y+m[11]*z+m[15]];
  },
  /* Perspective projection (OpenGL style).  fov = vertical field of view (deg)
     x_ndc = (f/aspect) * x / -z ,  y_ndc = f * y / -z ,  f = 1/tan(fov/2)      */
  perspective: function (fovDeg, aspect, n, f) {
    var t = 1 / Math.tan(fovDeg * Math.PI / 360), nf = 1 / (n - f);
    return [t/aspect,0,0,0, 0,t,0,0, 0,0,(f+n)*nf,-1, 0,0,2*f*n*nf,0];
  },
  /* Parallel (orthographic) projection: z is simply dropped / scaled */
  ortho: function (l, r, b, t, n, f) {
    return [2/(r-l),0,0,0, 0,2/(t-b),0,0, 0,0,-2/(f-n),0,
            -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1];
  },
  /* Project a 3D point (already in eye space) to pixel coordinates */
  toScreen: function (proj, p, w, h) {
    var q = CG.M4.apply(proj, p);
    var iw = q[3] !== 0 ? 1 / q[3] : 1;
    return { x: (q[0]*iw*0.5 + 0.5) * w, y: (1 - (q[1]*iw*0.5 + 0.5)) * h, z: q[2]*iw };
  },
  /* Compose: world = T * Ry * Rz * Shear * S   (scale first, translate last) */
  compose: function (T, R, SH, S) {
    return CG.M4.mul(T, CG.M4.mul(R, CG.M4.mul(SH, S)));
  }
};

/* ---------------- Collision detection ------------------------------- */
CG.Collide = {
  /* Axis-aligned bounding boxes given centre (cx,cz) and half sizes (hx,hz) */
  aabb: function (a, b) {
    return Math.abs(a.x - b.x) < a.hx + b.hx && Math.abs(a.z - b.z) < a.hz + b.hz;
  },
  circle: function (a, b) {
    var dx = a.x - b.x, dy = a.y - b.y, r = a.r + b.r;
    return dx*dx + dy*dy < r*r;
  }
};

/* =====================================================================
 * bend.js - curved and hilly road
 * After the model-view transform every vertex is moved sideways (curve)
 * and up/down (hill) by an amount that grows with the SQUARE of its
 * distance ahead of the camera:
 *       x' = x + curve * d^2 * k1        y' = y + hill * d^2 * k2
 * (a quadratic, i.e. parabolic, bend).  All road, scenery and cars use the
 * same formula, so everything bends together while the game logic keeps
 * working in simple straight "road coordinates".
 * Authors : Sami (B25, R54139) & Zaid (B29, R54231)
 * ===================================================================== */
var Bend = (function () {
  var U = { uCurve: { value: 0 }, uHill: { value: 0 } };
  function patch(m) {
    if (!m || m._bent || (m.userData && m.userData.nobend)) return;
    m._bent = true;
    m.onBeforeCompile = function (shader) {
      shader.uniforms.uCurve = U.uCurve; shader.uniforms.uHill = U.uHill;
      shader.vertexShader = 'uniform float uCurve;\nuniform float uHill;\n' + shader.vertexShader.replace('#include <project_vertex>',
        'vec4 mvPosition = modelViewMatrix * vec4( transformed, 1.0 );\n' +
        'float bd = max( -mvPosition.z - 6.0, 0.0 );\n' +
        'mvPosition.x += uCurve * bd * bd * 0.0011;\n' +
        'mvPosition.y += uHill * bd * bd * 0.0006;\n' +
        'gl_Position = projectionMatrix * mvPosition;');
    };
    m.needsUpdate = true;
  }
  /* patch every material under an object; objects are never frustum-culled
     (their un-bent position can be off screen while the bent one is visible) */
  function apply(root) {
    root.traverse(function (o) {
      if (o.isMesh) { o.frustumCulled = false; (Array.isArray(o.material) ? o.material : [o.material]).forEach(patch); }
    });
  }
  return { U: U, apply: apply, set: function (c, h) { U.uCurve.value = c; U.uHill.value = h; } };
})();

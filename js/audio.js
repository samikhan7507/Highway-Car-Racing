/* audio.js - tiny WebAudio engine tone + crash noise (no sound files needed) */
var GameAudio = (function () {
  var ctx = null, osc = null, gain = null, muted = false;
  function start() {
    if (ctx) { if (ctx.state === 'suspended') ctx.resume(); return; }
    var AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    ctx = new AC(); osc = ctx.createOscillator(); gain = ctx.createGain();
    osc.type = 'sawtooth'; osc.frequency.value = 50; gain.gain.value = 0;
    var lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 600;
    osc.connect(lp); lp.connect(gain); gain.connect(ctx.destination); osc.start();
  }
  function engine(speed, on) {                  // speed in m/s
    if (!ctx) return;
    osc.frequency.value = 45 + speed * 2.4;
    gain.gain.value = (on && !muted) ? 0.05 : 0;
  }
  function crash() {
    if (!ctx || muted) return;
    var n = ctx.sampleRate * 0.6, b = ctx.createBuffer(1, n, ctx.sampleRate), d = b.getChannelData(0);
    for (var i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / n, 2);
    var s = ctx.createBufferSource(), g = ctx.createGain(); g.gain.value = 0.5;
    s.buffer = b; s.connect(g); g.connect(ctx.destination); s.start();
  }
  function beep(f) {
    if (!ctx || muted) return;
    var o = ctx.createOscillator(), g = ctx.createGain(); o.frequency.value = f; g.gain.value = 0.06;
    o.connect(g); g.connect(ctx.destination); o.start(); o.stop(ctx.currentTime + 0.08);
  }
  return { start: start, engine: engine, crash: crash, beep: beep,
           toggle: function () { muted = !muted; if (gain && muted) gain.gain.value = 0; return muted; } };
})();

/* Service worker: caches the game so it starts instantly and works offline after the first visit. */
var CACHE = 'highway-racer-v1';
var FILES = ['./', 'index.html', 'manifest.webmanifest', 'icons/icon-192.png', 'icons/icon-512.png', 'lib/three.min.js',
  'js/cgmath.js', 'js/raster.js', 'js/bend.js', 'js/sky.js', 'js/audio.js', 'js/cars.js', 'js/world.js', 'js/hud.js', 'js/game.js'];
self.addEventListener('install', function (e) { e.waitUntil(caches.open(CACHE).then(function (c) { return c.addAll(FILES); }).then(function () { return self.skipWaiting(); })); });
self.addEventListener('activate', function (e) { e.waitUntil(caches.keys().then(function (ks) { return Promise.all(ks.filter(function (k) { return k !== CACHE; }).map(function (k) { return caches.delete(k); })); }).then(function () { return self.clients.claim(); })); });
self.addEventListener('fetch', function (e) {
  if (e.request.method !== 'GET') return;
  e.respondWith(caches.match(e.request).then(function (r) { return r || fetch(e.request); }));
});

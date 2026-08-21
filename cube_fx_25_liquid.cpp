#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 25. ACE 3-D LIQUID
// ===========================================================================
// The first effect here that treats the cube as a VOLUME rather than a
// surface. A tilting plane cuts through the solid and everything below it is
// wet, which needs one dot product per pixel and immediately makes the object
// read as a container with something in it.
//
// The plane is a real damped oscillator, not an animation curve: kicks give it
// an impulse and it sloshes and settles on its own, so the tilt overshoots,
// rocks back and dies away the way liquid actually does.
//
// The top face earns its keep here - when the level sits below it you are
// looking DOWN at the surface, so it becomes a rippling pool while the walls
// show the same water in cross section. Raise the level past it and the whole
// cube submerges.
// ---------------------------------------------------------------------------
static FX_RET mode_liquid() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(16 + 3 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int16_t *ph = (int16_t *)SEGENV.data;      // level, levelVel, tiltX/Y + vels
  int8_t  *cx = (int8_t *)(ph + 8);
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *st = (uint8_t *)(cz + n);         // [0] built [1] splash [2..3] clock

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 0; k < 8; k++) ph[k] = 0;
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  int dt = (int)fx_dt8(st + 2);
  if (dt > 60) dt = 60;                      // keep the integrator sane

  // --- the surface as a damped spring --------------------------------------
  int target = -110 + ((int)SEGMENT.custom1 * 230) / 255;
  if (SEGMENT.check1) target += (bass * 70) / 255;         // bass fills it
  if (target > 150) target = 150;

  const int slosh = 20 + (int)SEGMENT.custom2;
  if (peak && SEGMENT.check2) {                            // a kick hits the tank
    ph[1] = (int16_t)(ph[1] + (slosh * (int)peak) / 200);
    ph[3] = (int16_t)(ph[3] + (int)hw_random16((uint16_t)slosh) - slosh / 2);
    ph[5] = (int16_t)(ph[5] + (int)hw_random16((uint16_t)slosh) - slosh / 2);
    st[1] = peak;
  }
  ph[1] = (int16_t)(ph[1] + ((target - (int)ph[0]) * dt) / 150 - ((int)ph[1] * dt) / 400);
  ph[0] = (int16_t)(ph[0] + ((int)ph[1] * dt) / 150);
  for (int k = 2; k <= 4; k += 2) {                        // tilt returns to level
    ph[k + 1] = (int16_t)(ph[k + 1] - ((int)ph[k] * dt) / 130 - ((int)ph[k + 1] * dt) / 350);
    ph[k]     = (int16_t)(ph[k] + ((int)ph[k + 1] * dt) / 130);
  }
  { const int f = (int)st[1] - (int)fx_step(9, (uint16_t)dt);
    st[1] = (uint8_t)((f < 0) ? 0 : f); }

  const int lev = ph[0], tX = ph[2], tY = ph[4];
  const int rf   = 1 + (SEGMENT.custom3 >> 1);             // ripple pitch
  const int rAmp = 3 + (SEGMENT.custom3 >> 2) + ((int)st[1] >> 4);
  const uint8_t t1 = (uint8_t)((strip.now * (2 + (SEGMENT.speed >> 5))) >> 6);
  const uint8_t t2 = (uint8_t)((strip.now * (3 + (SEGMENT.speed >> 5))) >> 7);
  const int surfW = 7 + (SEGMENT.intensity >> 5);
  const uint8_t drive = cfx_drive(vol, 1.0f, 150 + (SEGMENT.intensity >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = cx[i], py = cy[i], pz = cz[i];
      const bool isTop = cube && ((x / B) == 1 && (y / B) == 1);

      // surface height above this horizontal position: level, plus the tilt of
      // the plane, plus two crossing wave trains
      const int rip = (((int)sin8_t((uint8_t)(px * rf + t1)) - 128)
                     + ((int)sin8_t((uint8_t)(py * rf - t2)) - 128)) * rAmp / 128;
      const int hs = lev + (tX * px) / 128 + (tY * py) / 128 + rip;
      const int vert = cube ? pz : py;
      const int sgn  = vert - hs;

      uint32_t c; int lum;
      if (isTop) {
        if (hs >= 118) {                                   // submerged to the brim
          lum = 220 + rip * 2;
          c = SEGMENT.color_from_palette((uint8_t)(70 + rip), false, false, 0);
        } else {                                           // looking down at it
          int att = 230 - (118 - hs);
          if (att < 25) att = 25;
          lum = (att * (150 + rip * 5)) >> 8;
          c = SEGMENT.color_from_palette((uint8_t)(96 + rip * 3), false, false, 0);
        }
      } else if (sgn < -surfW) {                           // body of the water
        int depth = -sgn - surfW; if (depth > 255) depth = 255;
        lum = 210 - depth / 2 + rip;
        c = SEGMENT.color_from_palette((uint8_t)(110 + depth / 3), false, false, 0);
      } else if (sgn <= surfW) {                           // the meniscus
        const int e = (sgn < 0) ? -sgn : sgn;
        lum = 255 - (e * 90) / (surfW + 1);
        c = RGBW32(210, 245, 255, 0);
      } else {
        continue;                                          // dry
      }
      if (lum < 0) lum = 0; else if (lum > 255) lum = 255;
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_LIQUID[] PROGMEM =
  "Ace 3-D Liquid@Ripple speed,Surface,Fill,Slosh,Ripple size,Bass fills,Splash on beat,Flat mode;;!;2f;sx=130,ix=140,c1=130,c2=120,c3=14,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_LiquidUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_liquid, _data_FX_MODE_LIQUID);
  }
  void loop() override {}
};

static CubeFx_LiquidUsermod cube_fx_liquid;
REGISTER_USERMOD(cube_fx_liquid);

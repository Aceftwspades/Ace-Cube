#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 13. CUBE WIRE
// ===========================================================================
// Cube Edges stripped to the wireframe. No edge<->centre travel at all: the
// beat pulses run ALONG the edges instead of across them.
//
// The trick is which coordinate the pulse lives in. Cube Edges pulses through
// ed[] - distance to the nearest edge. This one pulses through al[] - position
// along that edge. Because all twelve edges share one al parametrisation, a
// single pulse expands along every edge of the solid simultaneously, two
// fronts running apart from where it struck, which is Cube Ripples' behaviour
// confined to the wire.
//
// The wave still displaces the lit line, so the pulses ride a snaking wire
// rather than a straight one.
// ---------------------------------------------------------------------------
#define CW_P 4

static FX_RET mode_cube_wire() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + CW_P * 5 + 8)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *ed = SEGENV.data;
  uint8_t *al = SEGENV.data + n;
  uint8_t *ps = SEGENV.data + 2 * n;   // per pulse: origin, born lo/hi, strength, alive
  uint8_t *st = ps + CW_P * 5;         // [0] built [1..2] clock [3] round robin

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        float X, Y, Z, edge, along;
        cfx_pos(x, y, cols, rows, B, cube, X, Y, Z);
        if (cube) {
          const float mx = fabsf(X), my = fabsf(Y), mz = fabsf(Z);
          if      (mx <= my && mx <= mz) { along = X; edge = 1.0f - ((my < mz) ? my : mz); }
          else if (my <= mx && my <= mz) { along = Y; edge = 1.0f - ((mx < mz) ? mx : mz); }
          else                           { along = Z; edge = 1.0f - ((mx < my) ? mx : my); }
        } else {
          const float mx = fabsf(X), my = fabsf(Y);
          edge  = 1.0f - ((mx > my) ? mx : my);
          along = (mx > my) ? Y : X;
        }
        if (edge < 0.0f) edge = 0.0f; else if (edge > 1.0f) edge = 1.0f;
        ed[i] = (uint8_t)(edge * 255.0f);
        al[i] = (uint8_t)(int)(along * 127.0f + 128.5f);
      }
    }
    for (int k = 0; k < CW_P * 5 + 8; k++) ps[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];

  const uint16_t nowT = (uint16_t)strip.now;
  const uint16_t dtW  = fx_dt8(st + 1);

  const int thick  = 12 + (SEGMENT.intensity >> 2);   // wire, 0.4..2.3 px
  const int growth = 2 + (SEGMENT.speed >> 4);        // al units per 23 ms
  const int pw     = 8 + (SEGMENT.intensity >> 4);    // pulse front width

  if (peak && SEGMENT.check1) {
    int slot = -1;
    for (int k = 0; k < CW_P; k++) if (!ps[k * 5 + 4]) { slot = k; break; }
    if (slot < 0) { slot = st[3] % CW_P; st[3] = (uint8_t)(st[3] + 1); }
    uint8_t *e = ps + slot * 5;
    e[0] = hw_random8();                              // struck somewhere on the wire
    e[1] = (uint8_t)(nowT & 0xFF); e[2] = (uint8_t)(nowT >> 8);
    e[3] = peak; e[4] = 1;
  }

  int pOrig[CW_P], pRad[CW_P], pStr[CW_P]; int live = 0;
  for (int k = 0; k < CW_P; k++) {
    uint8_t *e = ps + k * 5;
    if (!e[4]) continue;
    const uint16_t born = (uint16_t)e[1] | ((uint16_t)e[2] << 8);
    const int r = (int)(((uint32_t)(uint16_t)(nowT - born) * growth) / 23);
    if (r > 128 + pw) { e[4] = 0; continue; }
    pOrig[live] = e[0];
    pRad[live]  = r;
    pStr[live]  = ((int)e[3] * (128 + pw - r)) / (128 + pw);   // fade as it spreads
    live++;
  }

  uint8_t shape = (uint8_t)(SEGMENT.custom3 / 5);
  if (shape > 5) shape = 5;
  const uint8_t freq = 1 + (SEGMENT.custom1 >> 5);
  const uint8_t amp  = SEGMENT.custom2 >> 1;
  const uint8_t scrl = (uint8_t)((strip.now * (1 + (SEGMENT.speed >> 4))) >> 5);

  const uint8_t base = cfx_drive(vol, 1.4f, 45);
  const uint8_t hue  = (uint8_t)(strip.now >> 7);

  SEGMENT.fadeToBlackBy(SEGMENT.check2 ? fx_fade(26, dtW) : 255);

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int d = (int)ed[i];

      int target = 0;
      if (shape) {
        const uint8_t w = cfx_wave(shape, (uint8_t)((uint16_t)al[i] * freq - scrl));
        target = (int)amp + ((int)amp * ((int)w - 128)) / 128;
      }
      int off = d - target; if (off < 0) off = -off;
      if (off >= thick) continue;                     // not on the wire
      const uint8_t prof = (uint8_t)(255 - (off * 255) / thick);

      uint8_t add = 0;
      for (int k = 0; k < live; k++) {
        const uint8_t dd = (uint8_t)(al[i] - (uint8_t)pOrig[k]);
        const int dist = (dd < 128) ? (int)dd : (256 - (int)dd);   // shortest way round
        int o = dist - pRad[k]; if (o < 0) o = -o;
        if (o >= pw) continue;
        add = qadd8(add, (uint8_t)((((pw - o) * 255) / pw * pStr[k]) >> 8));
      }

      const uint8_t lum = qadd8(scale8(prof, base), scale8(prof, add));
      SEGMENT.addPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(al[i] + hue + (add >> 2)),
                                   false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_WIRE[] PROGMEM =
  "Ace 3-D Cube Wire@Speed,Thickness,Wave cycles,Wave height,Wave shape,Pulse on beat,Trails,Flat mode;;!;2f;sx=120,ix=90,c1=16,c2=150,c3=6,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeWireUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_wire, _data_FX_MODE_CUBE_WIRE);
  }
  void loop() override {}
};

static CubeFx_CubeWireUsermod cube_fx_cube_wire;
REGISTER_USERMOD(cube_fx_cube_wire);

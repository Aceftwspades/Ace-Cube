# Ace LED FX — custom audio-reactive effects for WLED

Custom C++ effects for [WLED](https://github.com/wled/WLED), written as self-registering
[usermods](https://kno.wled.ge/advanced/custom-features/) rather than patches to WLED core.
Everything here compiles into a normal WLED binary alongside the built-in effect list — no
core files are modified.

Two hardware targets, both running **WLED v16.0.1**:

| Target | Description |
|---|---|
| **5-face LED cube** | 48×48 gap-mapped matrix on an ESP32, 5 data lines, ~43 fps |
| **Gledopto Ethernet panel** | `PCB-ESP32-RJ45-4CH-V1.0` controller with an onboard PDM microphone (IO32 / IO15) |

> **Firmware note:** target v16.0.1 (Arduino core 2.0.18 / IDF 4.4), not 17.0.0-dev — that
> branch currently only initializes 2 of the cube's 5 faces and has broken PDM audio on
> Arduino core 3.x / IDF 5.x.

## WLED links

- [WLED](https://github.com/wled/WLED) — the official firmware this project builds on
- [kno.wled.ge](https://kno.wled.ge/) — official documentation
- [Custom Features / Usermods](https://kno.wled.ge/advanced/custom-features/) — how the
  `custom_usermods` build option and `REGISTER_USERMOD()` self-registration work
- [Community Usermods](https://kno.wled.ge/advanced/community-usermods/) — index of
  community-written usermods (this project isn't listed there, but follows the same pattern)
- [Compiling WLED](https://kno.wled.ge/advanced/compiling-wled/) — PlatformIO build setup

## Layout

```
cube_fx_common.h            shared helpers — #include this, don't duplicate from it
cube_fx_00_cube_axes.cpp    one effect per file, fully self-contained
cube_fx_01_cube_noise.cpp
cube_fx_02_cube_ripples.cpp
...
cube_fx_31_tempo_scope.cpp
user_fx_custom.cpp          generic 2-D effects (flat panel, no cube geometry)
```

This used to be a single 4,974-line `user_fx_cube.cpp`. It's split into one small `.cpp` per
effect plus one shared header so a single effect can be worked on without scrolling past all
the others, and adding a new effect never means editing a file with other work already in it.

`user_fx_custom.cpp` is a separate, independent file of generic 2-D effects — WLED's usermod
build compiles every `.cpp` in the folder into one binary, so the split changes nothing about
how the project builds or how the two files relate to each other.

Each `cube_fx_NN_name.cpp` is fully self-contained:
- its `mode_x()` function
- its `_data_FX_MODE_X` metadata string
- any sprites/tables/helpers used *only* by that effect
- its own tiny `Usermod` subclass that calls `strip.addEffect(...)`, and its own
  `REGISTER_USERMOD(...)` call

That last part is what makes new effects drop in cleanly: WLED already supports any number of
`Usermod`s registering themselves independently, so each effect file registers *itself*.
There's no central "add your effect here" list to touch.

Effects are named "Ace 3-D …" (cube) or "Ace 2-D …" (flat panel) so they group together
alphabetically at the top of WLED's effect list.

## Cube effects (`cube_fx_*.cpp`)

The number prefix matches the numbering from the original monolithic file. There's no #10,
#15–18, #20–23, or #30 — those slots were retired before the split and the gaps were
preserved rather than renumbering everything.

| # | File | Effect |
|---|---|---|
| 00 | `cube_fx_00_cube_axes.cpp` | Cube Axes — calibration pattern, not a real effect |
| 01 | `cube_fx_01_cube_noise.cpp` | Cube Noise |
| 02 | `cube_fx_02_cube_ripples.cpp` | Cube Ripples |
| 03 | `cube_fx_03_spectral_globe.cpp` | Spectral Globe |
| 04 | `cube_fx_04_cube_slice.cpp` | Cube Slice |
| 05 | `cube_fx_05_cube_edges.cpp` | Cube Edges |
| 06 | `cube_fx_06_cube_frame.cpp` | Cube Frame |
| 07 | `cube_fx_07_cube_chladni.cpp` | Cube Chladni |
| 08 | `cube_fx_08_cube_bloom.cpp` | Cube Bloom |
| 09 | `cube_fx_09_rubiks_cube.cpp` | Rubik's Cube |
| 11 | `cube_fx_11_cube_plate.cpp` | Cube Plate — two-mode Chladni plate |
| 12 | `cube_fx_12_cube_cell.cpp` | Cube Cell — Waving Cell, made cube-native |
| 13 | `cube_fx_13_cube_wire.cpp` | Cube Wire |
| 14 | `cube_fx_14_mario_block.cpp` | Question Block |
| 19 | `cube_fx_19_tron.cpp` | Tron |
| 24 | `cube_fx_24_cube_lattice.cpp` | Lattice |
| 25 | `cube_fx_25_liquid.cpp` | Liquid |
| 26 | `cube_fx_26_sunlight.cpp` | Sunlight |
| 27 | `cube_fx_27_gyroid.cpp` | Gyroid |
| 28 | `cube_fx_28_split_geq.cpp` | Split GEQ |
| 29 | `cube_fx_29_quadrant_labyrinth.cpp` | Quadrant Labyrinth — BFS-solved maze per face, beat-driven corridor pulses |
| 31 | `cube_fx_31_speaker_cone.cpp` | Cube Speaker — each face pulses as a mock speaker cone, beat-triggered ring trails |
| 31 | `cube_fx_31_tempo_scope.cpp` | Tempo Scope — diagnostic, not a "real" effect |

> **Known housekeeping item:** `cube_fx_31_speaker_cone.cpp` and `cube_fx_31_tempo_scope.cpp`
> both currently claim slot 31 (and `speaker_cone`'s internal header comment still says
> `29`, left over from before Quadrant Labyrinth took that slot). One of them needs
> renumbering before both are registered in the same build.

Both Quadrant Labyrinth and Cube Speaker fall back to flat-panel behavior when no cube net is
detected, and each exposes five sliders and three checkboxes in the WLED UI.

## Flat panel effects (`user_fx_custom.cpp`)

Ten generic 2-D audio-reactive effects that don't depend on cube geometry: Beat Mandala,
Chladni Plate, Feedback Echo, Glass Kaleidoscope, Harmonic Lissajous, Hex Quilt,
Kaleidoscope, Moire Rosette, Spectral RD, and Spectral Wormhole.

## `cube_fx_common.h`

Holds only things genuinely shared across effects:
- `FX_RET` / `FX_DONE` — 0.15 vs. 16.x/17-dev effect-signature compatibility
- cube net geometry: `cfx_pos`, `cfx_buildCube`, `cfx_isCube`, and the
  `CFX_NET_PREP` / `CFX_NET_ROW` / `CFX_NET_SKIP` macros, which skip the four unlit corner
  blocks of the cube net (~44% fewer pixels computed)
- audio helpers: `cfx_getAudioData`, `cfx_bands`, `cfx_drive`, `cfx_smoothSpec`, `fx_lowBeat`
- frame-timing helpers: `fx_dt`, `fx_dt8`, `fx_step`, `fx_fade` — all motion is calibrated at
  23 ms so behavior holds at any frame rate above 43 fps
- `mq_scale` (pixel scale/dim) — used by every effect from Question Block onward
- the wall/face surface toolkit (`cfx_buildBand`, `lf_fwd`, `lf_inv`, `cfx_buildDirLut`,
  `cfx_buildCells`) — used by Question Block, Tron, Split GEQ, and several others

If you ever find yourself copy-pasting a helper into a second effect file, that's the sign it
belongs in `cube_fx_common.h` instead — move it there and delete the copies. Anything used by
exactly one effect (game boards, sprite tables, per-effect `#define`s) stays local to that
effect's own file.

## Design principles

- **Beat response is subtle and organic** — bass-gated, one-shot rather than multi-frame,
  proportional to kick strength, with a refractory window (`fx_lowBeat()`).
- **RAM discipline on ESP32** — static array sizes are bounded; geometry caches use
  `SEGENV.allocateData` in a build-once pattern rather than reallocating per frame.
- **Cube net + flat panel fallback** — effects that use cube geometry detect whether a full
  net is present and degrade gracefully to flat-panel behavior when it isn't.

## Adding a new effect

1. Copy `cube_fx_00_cube_axes.cpp` (the smallest one) to `cube_fx_NN_your_effect.cpp`, picking
   any unused number.
2. Write `mode_your_effect()` and `_data_FX_MODE_YOUR_EFFECT`.
3. Update the `Usermod` / `REGISTER_USERMOD` block at the bottom to match your names.
4. That's it — no other file needs to change, and nothing else needs to be recompiled except
   your new file and whatever links the binary.

If your effect needs a helper that already exists in another effect's file (not
`cube_fx_common.h`), don't `#include` that other `.cpp` — either duplicate the small helper
into your file, or, if it's clearly general-purpose, promote it into `cube_fx_common.h` so
both files reference one copy.

For the WLED-side mechanics of registering a usermod (build flags, `platformio_override.ini`,
the `Usermod` base class lifecycle), see the official
[Custom Features](https://kno.wled.ge/advanced/custom-features/) docs.

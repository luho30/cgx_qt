# cgx → Qt6 port — replay guide

**License:** GNU General Public License version 2, identical to cgx itself —
see `COPYING`. Every file created by this port carries the upstream
CALCULIX/GPL header; the only third-party component in the tree is `libSNL`
(Scott A.E. Lanham, GPLv2, see `libSNL/license.txt`). Since Step 14 no GLUT
code, header or notice is shipped either — the GLUT_* values cgx uses live
in the port's own `src/glut_constants.h`.

Goal: full swap of the vendored GLUT-3.5 windowing/event/menu layer for Qt6,
in small behavior-checked steps. This file records **every modification** so
the port can be re-applied onto a future upstream `cgx_X.XX` tarball.

Port status: Steps 0–15 done — port complete (CMake build, Qt app shell,
Qt viewport, input, menus, fonts/X11 removal, axes/cmdline widgets,
header-only menubar, full-window 3D viewport with 100% transparent HUD legend overlay,
semi-transparent in-window console panel, Qt-native hardcopy without ImageMagick,
command line as an opaque overlay band over the whole view, last external
`glut-3.5` include dependency replaced by an in-tree constants header,
test hooks compiled out of the default build (security audit), sign-off with harness + matrices green).

Standing rule: this file is updated at the end of **every** step — status
line above, a `### Step N` entry under §3, and any newly touched upstream
file added to §1/§4. A step is not done until the replay recipe (§5) and
verification matrix (§7) still hold for it.

## 1. File ownership: ours vs upstream

The port is designed so re-application is mechanical:

| Path (relative to `cgx_X.XX/`) | Ownership | Notes |
|---|---|---|
| `src/qt/` (15 files) | **OURS, new** | `main.cpp`, `glue.h/.cpp`, `qt_shim.h`, `CgxMainWindow.h/.cpp`, `CgxViews.h/.cpp`, `CgxFont.cpp`, `selftest.cpp`, `ConsoleCapture.h/.cpp`, `ConsoleView.h/.cpp`, `Hardcopy.cpp`. Never exists upstream — copy verbatim. |
| `tools/console_screencheck.py` | **OURS, new** | Real-screen check that the console panel is above the legend (§Step 11). Copy verbatim. |
| `src/CMakeLists.txt` | **OURS, new** | Replaces `Makefile`+`Makefile.inc`. Source lists must be re-synced per §5. Option `CGX_QT_TESTHOOKS` (default OFF) compiles the test hooks in (Step 15). |
| `src/glut_constants.h` | **OURS, new** | The 36 GLUT_* values cgx still uses (Step 14) — replaces `<GL/glut_cgx.h>`. Never exists upstream — copy verbatim. |
| `src/cgx.h` (tail, font tokens) | **Upstream touch 1** | Guarded `#ifdef CGX_QT` include of `qt/qt_shim.h` (see §4) + `GLUT_FONT` redefined to `(void*)0..5` tokens (Step 6; layout macros untouched). |
| `src/extUtil.h` (include block) | **Upstream touch 2** | Deleted `#include <GL/glx.h>` (zero `glX*` calls; it had smuggled in `<X11/Xlib.h>`, now included directly for vestigial types), added `<GL/glu.h>` (was pulled in via `glut_cgx.h`), swapped `<GL/glut_cgx.h>` → `"glut_constants.h"` (Step 14) — see §4. |
| `src/Makefile`, `src/Makefile.inc` | Upstream, retired | Kept as `Makefile.legacy.bak`, `Makefile.inc.legacy.bak` for diffing. |
| `src/cgx.c` and everything else | **Upstream, UNTOUCHED** | `main()` renamed via compile flag, not by editing (§3). |
| `../libSNL/` | Upstream sibling, untouched | Compiled and linked (`file(GLOB …)`) — a real build dependency. |
| `../glut-3.5/` | **not needed (Step 14)** | Was header-only for `<GL/glut_cgx.h>`; now in-tree. The sibling may stay on disk for old cgx versions (2.5/2.22) but cgx_2.23 never reads it. |

## 2. System prerequisites (Ubuntu 26.04, verified)

```bash
pkexec apt-get install -y \
  libgl-dev libglu1-mesa-dev \
  libx11-dev libxi-dev libxmu-dev libxext-dev libxt-dev libsm-dev libice-dev \
  g++ cmake qt6-base-dev
```

Notes:

- `gl.h`/`glx.h` come from `libgl-dev`, `glu.h` from `libglu1-mesa-dev`.
- `g++` is mandatory (link step + `uselibSNL.cpp`, `generateTet.cpp`, `libSNL/*.cpp`).
- Do **not** install proprietary NVIDIA drivers for Kepler cards (e.g. GT 740M):
  supported only up to the `470` legacy branch, which is absent/broken on new
  kernels. Mesa `nouveau` + `DRI_PRIME=1 ./cgx` already gives acceleration.
- `sudo` needs a terminal here; agent shells use `pkexec`.

## 3. What was done per step (and why)

### Step 0 — baseline

Upstream `cgx_2.23` unpacked under `/usr/local/CalculiX/` next to sibling
versions (`cgx_2.22`, `cgx_2.5`, …) and shared `../glut-3.5`, `../libSNL`.
No git repo. Build system: plain `src/Makefile` + `src/Makefile.inc`
(`SLIB` ≈ 124 C files; `SUTIL` = 35 vendored GLUT C files + 2 local
`.cpp` + `libSNL/*.cpp`, linked in one `g++` step). No tests/lint/CI.

### Step 1 — CMake-only build (still 100% GLUT)

- Wrote `src/CMakeLists.txt` mirroring `Makefile` exactly: same sources
  (win32_*.c excluded, as upstream), same `-DSEMINIT`, same include dirs
  (`./`, `/usr/include`, `/usr/include/GL`, `../../libSNL/src`,
  `../../glut-3.5/src`, `/usr/X11/include` if present), same link libs,
  **plus** `Qt6::Core/Gui/Widgets/OpenGL/OpenGLWidgets` (unused yet —
  proves the toolchain; linker drops them until referenced).
- **Trap — GCC 15 defaults to `-std=gnu23`**, which rejects the K&R
  definitions in `improveMesh.c`. Fix: C sources compile as `gnu17`
  (`CMAKE_C_STANDARD 17` + extensions; per-language `-std=` so the C flag
  never leaks to `g++`), C++ as `gnu++17`.
- `libSNL/*.cpp` uses `file(GLOB …)` exactly like the Makefile wildcard;
  `CGX_SLIB`/`GLUT_SRCS` are explicit lists (must be re-synced, §5).
- Deleted `Makefile*` (backups kept). Build: `cmake -B build -S . &&
  cmake --build build -j$(nproc)` → `src/build/cgx`.

### Step 2 — Qt app shell, headless split

- New `src/qt/main.cpp` owns the only real `main()`. It scans `argv` for
  `-bg`: found → `QCoreApplication` (no display connection) and runs the
  legacy headless path (`readfbd()` + `exit()` in `cgx.c`, which exits
  **before** any `glutInit`); otherwise → `QApplication`, then legacy
  interactive path (still GLUT-driven in this step).
- `cgx.c` untouched: `set_source_files_properties(cgx.c PROPERTIES
  COMPILE_DEFINITIONS "main=cgx_main")` renames its `main()` at compile time.
- **Trap — C/C++ linkage.** `cgx.h` has no global `extern "C"` guards (only
  `uselibSNL.h`/`generateTet.cpp` wrap their own include). Every Qt (C++)
  file that calls a legacy C function must declare it `extern "C"`
  explicitly — including at namespace scope. Block-scope `extern`
  declarations inside a function body get C++ linkage and fail to link.
  Centralize them in `qt/glue.h` (`cgx_main`, `cgxReshape`, …).

### Step 3 — Qt viewport replaces GLUT windows (current state)

- New widgets: `CgxMainWindow` (`QMainWindow`, `MenuView` fixed to
  `width_menu` + expanding `GraphicsView`); both views are `QOpenGLWidget`s
  whose `paintGL()` calls the currently registered legacy display callback.
- `qt/glue.h/.cpp`: `extern "C"` shims for the GLUT window/display/state/
  loop entry points (17 in Step 3, extended with input + forwarders in
  Step 4 — 34 `cgx*` functions total, only `Reshape/Entry/VisibilityFunc`
  still parked): window create/destroy/set/get, `PostRedisplay`, `DisplayFunc`
  (stored in a per-window-id map, so `w3`-while-active registrations can't
  clobber `w1`), `Get` (geometry/screen/elapsed via Qt; **menu queries
  forwarded to real `glutGet`**), `ReshapeWindow` (top-level only), `IdleFunc`
  (16 ms `QTimer` pump; legacy `idleFunction()` deactivates itself via
  `glutIdleFunc(NULL)` as before), `MainLoop` (no-op → `main.cpp` runs
  `app.exec()`), `Attach/DetachMenu` (no-ops). Input-callback registrations
  were parked as `((void)0)` in Step 3, made real in Step 4 (below).
- `qt/qt_shim.h` (`#define glutX cgxX`) is pulled in at the **end of
  `cgx.h` under `#ifdef CGX_QT`** — one `#define CGX_QT` in CMake retargets
  all ~551 call sites with zero edits to `cgx.c` et al. Vendored GLUT
  sources don't include `cgx.h`, so the real symbols still link for menus,
  bitmap fonts, colors, `glutInit*` (kept real until Steps 4–6).
- `main.cpp` sets a **compatibility-profile** `QSurfaceFormat` (2.1, depth
  24) — legacy GL needs display lists + `GL_SELECT` picking, which a core
  profile would break. `app.exec()` runs only if windows were created.
- `CMAKE_ENABLE_EXPORTS` (`-rdynamic`) enables `dladdr()` paint diagnostics,
  gated by env `CGX_QT_DEBUG_PAINT=1` (prints `drawMode`, `GL_LIGHTING`,
  resolved display-callback name for the first paints).
- Key verified facts: `-bg` exits before `glutInit`; `idleFunction` needs
  ≥3 invocations (2 × `PostRedisplay` + full load); `reshape()` needs a
  current context (main window calls it under `makeCurrent`); `QOpenGLWidget`
  is **unsupported on `offscreen` platform** — interactive-path checks need
  a real display (`:0` or Xvfb).

### Step 4 — input (mouse/keyboard/picking via Qt events)

- `qt_shim.h`: `glutMouseFunc/MotionFunc/PassiveMotionFunc/KeyboardFunc/
  SpecialFunc` now map to real shims (`glue.h/.cpp`); `Reshape/Entry/
  VisibilityFunc` stay parked. No `cgx.c`/`pickFunktions.c` edits.
- Shims store handlers **per legacy window id** (`s_*Funcs[activWindow]`),
  matching GLUT semantics. This preserves the `pickFunktions.c` pattern of
  swapping `Keyboard()` against `pick()/defineDiv()/defineValue()` with
  zero call-site changes — verified by round-trip (`qenq` swallows `help`,
  `q` restores it).
- `CgxViews.cpp` translates: buttons L/M/R → 0/1/2, wheel → 3/4 (cgx.c-local
  `GLUT_WEEL_UP/DOWN`, not in GLUT headers), F1–F12 → 1–12, arrows → 100–108,
  ASCII from `QKeyEvent::text()` with explicit Esc/Return/Tab/Backspace/
  Delete/Space fallbacks; key coords from cursor pos (GLUT top-left origin
  == Qt widget coords, no flip). Views use `StrongFocus` + mouse tracking;
  graphics view is focused on show.
- Window policy (verified against `createNewMainMenu`, which attaches the
  main menu with `glutAttachMenu(LEFT)` while `w0` is current): graphics
  view forwards L/M/R (w1 never has a menu); menu strip swallows LEFT
  (GLUT popup pending Step 5) and forwards M/R/wheel/motion/keys.
- Findings: `glutGetModifiers`/`glutWarpPointer` are **unused** in cgx
  sources (no port needed); `glutDetachMenu` never called upstream.
- Test automation: KDE/Wayland gates XTEST synthetic input behind a
  "Remote Control" approval dialog (one click on OK approves); Qt defaults
  to Wayland here, so drive tests with `QT_QPA_PLATFORM=xcb` for xdotool.
  xdotool `mousemove` warps (single MotionNotify) — use small-step loops for
  streak drags; drags ending outside the widget deliver nothing. Verified:
  left-drag rotate, middle-drag zoom (ruler `6e-01`→`8e-01`), wheel steps,
  `help<Return>`, pick-swap round-trip, Up-arrow history recall.

### Step 5 — popup menus + menubar via a GLUT API mirror (no call-site edits)

- All menu construction flows through the GLUT API, so instead of porting
  each of the 21 `glutCreateMenu` trees, `glue.h/.cpp` implements a **mirror
  model** (`menu id → callback + items`, submenu links) behind shims for
  `Create/Destroy/Get/SetMenu`, `AddMenuEntry/AddSubMenu`,
  `RemoveMenuItem`, `ChangeToMenuEntry/ChangeToSubMenu` (`qt_shim.h`).
  Dynamic upstream rebuilds (`createDatasetEntries`,
  `recompileEntitiesInMenu`, user menus) work unmodified.
- Rendering: left-click (no drag) in the menu strip pops a recursively built
  `QMenu` for the menu attached to `w0`; a `QMenuBar` mirrors the same
  attached structure, rebuilt lazily when the dirty flag (set by any
  mutating shim) is seen in `cgxPostRedisplay`/`cgxPostWindowRedisplay`.
  `QAction::triggered` calls the stored legacy callback with its value.
- Findings that matter for replay:
  - `glutCreateMenu` makes the new menu **current** (shim must too) —
    otherwise entries land in the wrong menu (found via menubar showing
    dataset entries instead of mainmenu structure).
  - `glutDestroyMenu` on the current menu resets current to 0.
  - `GLUT_MENU_NUM_ITEMS` is served from the mirror (real GLUT knows
    nothing once creation is intercepted); indices are 1-based upstream.
  - `glutAttachMenu(LEFT)` runs while `w0` is current, so menubar
    follows `w0`'s attachment; graphics view (`w1`) never had a menu.
  - Initial menu attachment syncs immediately to `QMenuBar`, displaying
    all menus (`Viewing`, `Animate`, `Frame`, `Zoom`, ...) across both
    geometry and result modes.
  - QMenu keyboard nav exists but XTEST focus delivery to modal popups is
    flaky here; verification used mouse clicks (submenu opens, `+z View`
    reorients 33.7% pixels, menubar `Animate` dropdown correct).
- Verified: popup content (full mainmenu), nested submenus, entry callbacks
  with values, menubar render + dropdowns, dataset-menu rebuild at load,
  `-bg`/banner regressions.

## 4. Upstream edits (re-apply verbatim; everything else byte-identical)

Edit 1 — append to the end of `src/cgx.h`:

```c
#ifdef CGX_QT
/* Step 3+: redirect GLUT window/display/loop calls to Qt shims */
#include "qt/qt_shim.h"
/* Step 6: vendored GLUT sources are no longer compiled, so their font
   globals (GLUT_BITMAP_*) don't exist. Tokens (void*)0..5 select the six
   fonts in GLUT_FONT order; CgxFont.cpp maps them to QFonts. Layout macros
   (WIDTH/HEIGHT/DEF/SUM) are untouched, so all text geometry is preserved. */
#undef GLUT_FONT
#define GLUT_FONT {(void*)0, (void*)1, (void*)2, (void*)3, (void*)4, (void*)5}
#endif
```

Edit 2 — in `src/extUtil.h`, three include changes (one block):

1. delete `#include <GL/glx.h>` (keep the `#include <X11/Xlib.h>` line that
   replaced it: Xlib headers remain a build dependency for the vestigial
   `Display`/`Colormap`/`XColor` types in dead signatures; zero `glX*`
   calls exist anywhere).
2. add `#include <GL/glu.h>` (Step 14: `<GL/glut_cgx.h>` used to pull it
   in; `extUtil.h` needs `GLUnurbsObj` and the NURBS `glu*` calls).
3. replace `#include <GL/glut_cgx.h>` with `#include "glut_constants.h"`
   (Step 14 — the in-tree constants, see §3 Step 14).

Edit 3 — in `src/cgx.c`, compile out the three legacy hardcopy bodies
(`SaveTGAScreenShot`, `getTGAScreenShot`, `createHardcopy`) under
`#ifndef CGX_QT` (one guard each, marked `Step 12`). Their callers are
renamed by Edit 1, so with CGX_QT set nothing references the legacy bodies;
without it the file builds exactly as upstream.

## 5. Re-apply recipe for a new upstream `cgx_X.XX`

Assume a fresh unpack at `/usr/local/CalculiX/cgx_X.XX/` (keep old tree for
diffing). Total: copy `qt/` (15 files) + `tools/` +
`CMakeLists.txt`, apply the §4 edits (now three), sync lists.

1. `cp -r <old>/src/qt <new>/src/qt && cp <old>/src/CMakeLists.txt
   <new>/src/ && cp <old>/src/glut_constants.h <new>/src/` (then bump
   `project(... VERSION …)`).
2. Apply the §4 edits (`cgx.h` append incl. Step 12 defines, `extUtil.h`
   include block — glx/glu/glut —, `cgx.c` `#ifndef CGX_QT` guards around
   the three hardcopy bodies).
3. Sync `CGX_SLIB` with `<new>/src/Makefile.inc` (`SLIB`) plus
   `<new>/src/Makefile` (`ULIB`, currently just `userFunction.c`):
   `diff <(grep -oE '[A-Za-z0-9_]+\.c' old/Makefile.inc | sort)
   <(grep -oE '[A-Za-z0-9_]+\.c' new/Makefile.inc | sort)` — add new files,
   drop removed ones. Then re-drop `XFunktions.c` + `readStdCmap.c` (dead
   X11/GLUT colormap path — re-verify zero callers first).
4. No `GLUT_SRCS` anymore (deleted in Step 6) and no
   `../../glut-3.5/src` include dir (deleted in Step 14): `glut_constants.h`
   travels with `src/`, so the new tree needs no glut sibling at all.
5. Conflict checks (all must be empty before building):
   - `grep -n "^int main" <new>/src/cgx.c` — still the single entry renamed
     via `-Dmain=cgx_main`? If upstream renamed/restructured it, adjust
     `glue.h`/`main.cpp` declarations.
   - `grep -rhoE "glut[A-Za-z]+" <new>/src/*.c | sort -u` vs old list (§6) —
     any **new** `glut*` symbol fails to link by design (only mapped symbols
     exist); add a real shim, never a silent no-op.
   - `grep -rn "glX" <new>/src/*.c <new>/src/*.h` — must stay empty
     (`glx.h` is gone).
   - `grep -n "width_menu\|activWindow\|int w0" <new>/src/cgx.c` — `glue.cpp`
     `extern`s (`w0,w1,w2,w3,activWindow,width_w0,…,width_menu,height_menu`)
     must still match type/name.
   - `grep -n "DrawPickedItems\|DrawMenuSet\|DrawAxes\|updCommandLine\|\
     reshape\|idleFunction" <new>/src/cgx.h` — prototypes the Qt side calls.
   - New `XOpenDisplay/XStoreColors/XQueryColor/glutSetColor` calls anywhere
     in compiled sources → re-breaks the dropped X/GLUT linkage; port or
     stub before linking.
   - `grep -n "SaveTGAScreenShot\|getTGAScreenShot\|^void createHardcopy\|^int WriteTGA" <new>/src/cgx.c`
     vs old — the Step 12 `#ifndef CGX_QT` guards + `cgx.h` renames must cover
     all three bodies; any new hardcopy helper calling `system(convert/…)` or
     `while(access())` needs the same treatment (see Step 12 traps).
6. Prereqs (§2; `libx11-dev` headers still needed for vestigial X11 types),
   then build + verify (§7).
7. If upstream changed `extUtil.h` includes (`GL/gl.h`,
   `GL/glut_cgx.h`) or the `w0..w3`/`activWindow` window model, the shim
   layer needs a matching update — see `qt/glue.cpp` header comments.

## 6. GLUT usage inventory (2.23 baseline, for step 5+ porting)

551 call sites; concentrated in `cgx.c` (388), `pickFunktions.c` (104),
`plotFunktions.c` (22), `setFunktions.c` (12), `extGL.c` (11).
Distinct symbols: `SetWindow` 155, `AddMenuEntry` 84, `KeyboardFunc` 39,
`PostRedisplay` 31, `DisplayFunc` 23, `Get` 21, `CreateMenu` 21,
`AddSubMenu` 20, `SwapBuffers` 19, `SetMenu` 8, `BitmapCharacter` 7,
`InitDisplayMode` 6, `DestroyMenu` 6, `ReshapeWindow`/`BitmapWidth` 5,
`SetColor`/`PositionWindow`/`CreateSubWindow` 4, `Special/PassiveMotion/
Mouse/Idle/DestroyWindowFunc` 2, `Visibility/Reshape/Motion/Entry/
CreateWindow/AttachMenu/Init/MainLoop` 1. Window topology: `w0` main →
`w1` graphics subwindow → `w2` axes gizmo + `w3` command line; `activWindow`
+ `glutDisplayFunc` swapping drives modes; `pickFunktions.c` swaps
`Keyboard↔pick/defineDiv/defineValue`.

## 7. Verification matrix (run after every step and every re-apply)

From `src/` (`build/cgx` is the CMake binary):

```bash
cmake -B build -S . && cmake --build build -j$(nproc)  # must succeed;
# only legacy -Wformat-overflow/-Wmisleading-indentation warnings
./build/cgx                                            # banner + usage, exit 0
./build/cgx -bg ../examples/basic/disc.fbd | tail -2   # ready / done
./build/cgx -bg ../examples/basic/cylinder.fbd | tail -1
QT_QPA_PLATFORM=offscreen ./build/cgx -bg ../examples/basic/disc.fbd
env -u DISPLAY ./build/cgx -bg ../examples/basic/cylinder.fbd  # no display OK
ldd build/cgx | grep -i qt6                            # Qt6 linked (Step 2+)
# Interactive (needs :0; stdout is block-buffered → unbuffer + log to file):
timeout 25 stdbuf -o0 -e0 ./build/cgx -b ../examples/basic/disc.fbd > /tmp/cgx.log 2>&1
# expect exit 124 (killed while event loop runs = healthy) and:
grep -E "GL_MAX_EVAL_ORDER|ready|done|gtol calculated" /tmp/cgx.log
# Visual (Step 3+): run in background, screenshot, compare with legacy ./cgx
python3 -c "from PIL import ImageGrab; ImageGrab.grab().save('/tmp/cgx.png')"
# Linkage (Step 6+): no vendored GLUT objects, no direct X libs
nm build/cgx | grep -E " U glut"                        # expect empty
nm build/cgx | grep -i glut | grep -v " U "             # expect only glut_font
readelf -d build/cgx | grep NEEDED                      # Qt6 + GL/GLU only
# Text (Step 6+): ruler/caption/menubar legible in screenshots; A/B vs legacy
# Test build (Step 15) — the CGX_QT_* hooks below exist ONLY in it:
cmake -B build-test -S . -DCGX_QT_TESTHOOKS=ON && cmake --build build-test -j$(nproc)
strings -a build/cgx | grep -c CGX_QT_                  # default build: expect 0
# Full input suite, portal-proof on Wayland and xcb (Step 7+):
CGX_QT_SELFTEST=1 QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd
# expect exit 0, SELFTEST-DONE, exactly 3 help executions (since Step 11: the
# 3rd is typed into the focused console panel and must be forwarded to cgx),
# dock show/hide 1/0
# Hardcopy (Step 12+, needs a display + ffmpeg for GIF/movie; X11 + PIL for stacking check):
CGX_QT_KEYS="ds 1 e 1;hcpy png;hcpy tga;hcpy gif;hcpy ps" QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd
# expect hcpy_1.png/.tga/.gif/.ps + `create …`/`ready`, no `not found`, no stall; open them
CGX_QT_SELFTEST_CMDS="movi frames 2" CGX_QT_SELFTEST=1 QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd
# expect `movie.gif` (2 frames), frames cleaned up, SELFTEST-DONE
# Step 11 (console panel): the same selftest covers it (capture, 5-line panel,
# scrollbar, echo, selection/copy/focus, 1.7 MB flood = deadlock regression). Stacking vs the legend
# can ONLY be verified on the real screen:
python3 ../tools/console_screencheck.py   # exit 0 = PASS (X11 + PIL + build-test needed)
# Step 10: also with a result + legend active and geometry under the legend:
CGX_QT_SELFTEST_WHEEL=-8 CGX_QT_SELFTEST_CMDS="ds 1 e 1" CGX_QT_SELFTEST=1 \
  QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd   # screen-check lines, no FAIL
# Step 13 (command-line overlay): covered by the same selftest runs — expect
#   `cmdline-visible=1 w3=… flag=1`, `cmdline focus=1 band-hit=QLineEdit`,
#   NO "squeezed 3D view" FAIL, `cmdline-visible=0 … flag=0` after toggle-off;
# geometry proof from a KEYS run (stderr):
CGX_QT_KEYS="ds 1 e 1;view cl" QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd 2>&1 \
  | grep KEYS-DONE    # view=…,201,…,450 console=…,541,…,78 → view bottom −
                      # console bottom = 32 px line band, and view height =
                      # window height − menubar (480 − 30 = 450, no squeeze)
```

`QOpenGLWidget` does not work on the `offscreen` platform — interactive
checks cannot run headless; use `:0` or Xvfb.

**External-input caveat on this Wayland session (measured, not guessed):**
XTEST *keyboard* events reach clients normally, but XTEST *pointer motion
does not* — a plain `xev` moved to 683,371 still reports
`root:(739,429)` while `xdotool getmouselocation` reports the warped
position, i.e. every press is delivered at one frozen point (root cause is
environmental: Xwayland 24.1.10 + KWin 6.6.6, `[xwayland ei] Unhandled
event EI_EVENT_SYNC (91)`; a pre-authorization grant in the `kde-authorized`
permission store only suppresses the consent dialog, not the frozen motion —
tried during the investigation and revoked again, so don't repeat it).
Consequences for the matrix above: `xdotool type` only proves "text reaches
the *focused* widget", and clicks cannot be aimed at a widget (they land
wherever the frozen point sits — window moves are clamped by KWin, so the
"reposition the window under the point" trick fails too). Positional
coverage therefore comes from the in-app selftest (`widgetAt()` = Qt's real
routing incl. stacked overlays, `hasFocus()`, no-squeeze geometry) plus
numeric screenshot diffs; from outside we confirmed keyboard injection into
the focused command line and that a press in the 3D view does take focus
away from it.

## 8. Resolution of Model Distortion & Aspect Ratio Issues

### Root cause 1: Numeric Locale Corrupting 3D Coordinates
- **Symptom:** In Qt builds, `disc.fbd` rendered as a distorted, pinched 4-pointed star instead of a smooth circular disc; `result.frd` rendered as a twisted jagged shape.
- **Cause:** `QCoreApplication` and `QApplication` constructors implicitly call `setlocale(LC_ALL, "")`. On European/German systems (`de_DE.UTF-8`), this sets `LC_NUMERIC` with comma (`,`) as the decimal separator. `cgx` relies on standard C `atof`, `sscanf`, and `strtod` which expect dot (`.`). As a result, fractional numbers like `0.70711` stopped parsing at `.` and evaluated to `0.0`, collapsing arc control points to the origin `(0,0,0)`.
- **Fix:** In `src/qt/main.cpp`, immediately call `std::setlocale(LC_NUMERIC, "C");` after constructing `QCoreApplication` and `QApplication`.

### Root cause 2: Window Size & Viewport Aspect Ratio Mismatch
- **Symptom:** The Qt window initially shrank to 384×200, squashing geometry horizontally.
- **Cause:** `CgxMainWindow` lacked size hints, and `reshape()` in `cgx.c` subtracted `width_menu` and `height_menu` from the total `QMainWindow` size, causing `aspectRatio_w1` to diverge from `m_graphicsView`'s actual widget aspect ratio.
- **Fix:** In `src/qt/CgxMainWindow.cpp`:
  - Set `m_graphicsView->setMinimumSize(400, 400);` and initial main window size to 784×672 (`width_w0` × `height_w0`).
  - In `resizeEvent`, explicitly update `width_w1`, `height_w1`, and `aspectRatio_w1 = (double)width_w1 / (double)height_w1;` from `m_graphicsView`'s exact dimensions so `glOrtho` always matches the widget's viewport 1:1.

### Verified Results:
- `disc.fbd` renders as a smooth, solid, shaded disc matching legacy GLUT pixel-for-pixel.
- `result.frd` renders as a clean, crisp 3D solid beam.

### Step 6 — bitmap fonts via QFont, X11/GLUT deleted from the build

- `qt/CgxFont.cpp`: `(void*)0..5` tokens → QFonts matching legacy pixel
  heights ({10,12,13,15,18,24}, serif/sans/mono). Glyphs rasterized once to
  cached 1-bit bitmaps, drawn with `glBitmap` advancing the raster pos by
  the real advance — the exact `glutBitmapCharacter/Width` contract, so
  `text()/scala_*/button()`, `DrawCommandLine` cursor math and pick-text
  centering work unchanged (measure == draw by construction).
- `XFunktions.c` + `readStdCmap.c` **dropped from `CGX_SLIB`** (not edited):
  `calcOffset/getColormap/storeColors/readStdCmap` have zero callers and
  held 100% of Xlib calls + all `glutSetColor` calls. `basCol={0,1,2}`
  defaults stand. Prototypes stay as harmless declarations.
- Vendored `glut-3.5` sources removed from CMake (dir kept on disk;
  `<GL/glut_cgx.h>` still included header-only for `GLUT_*` constants).
  `glutInit/InitDisplayMode/InitWindowSize` → no-ops (Qt owns contexts).
- Link libs now Qt6 + `GL GLU m pthread rt` only — verified via
  `readelf -d` (no X11/Xt/Xmu/Xext/Xi/SM/ICE) and `nm` (no `U glut*`;
  only cgx's own `glut_font` data symbol remains).
- Traps worth replaying: `glBitmap` needs `GL_UNPACK_ALIGNMENT,1` (rows are
  `(w+7)/8` bytes; default 4 shears every glyph — first screenshots showed
  static noise); pack MSB-first, GL-bottom-row-first, `yorig` so the
  baseline lands on the raster pos; `<X11/Xlib.h>` must be included directly
  once `glx.h` goes (it provided the types transitively); the pre-existing
  `cgxGet`→real-`glutGet` forward had to become local `return 0` + stderr
  diagnostic (link-as-tripwire caught it). Shim surface rule: only map
  symbols with real implementations, so new upstream `glut*` calls fail to
  link loudly (§5 recipe).
- Verified: caption/ruler/menubar crisp and legacy-identical (A/B incl.
  blank menu panel), popup + pick-swap round-trip intact post-removal.

### Step 7 — axes gizmo, command line, portal-proof test harness

- `AxesView` (`QOpenGLWidget`, 110×110) floats bottom-left over the 3D view
  inside a `GraphicsContainer` (plain holder; the 3D view stays
  layout-managed — a layout-less container collapses to zero, found the hard
  way). `paintGL` calls w2's stored callback (`DrawAxes`). Repaint routing
  split: `viewForPaint()` maps w2→overlay (so `Mouse()`'s explicit w2
  `PostRedisplay` tracks rotation), while context fallback still uses the
  graphics view. Verified: tripod renders, tracks drags (framebuffer A/B).
- Command line: `QLineEdit` strip under the main row, visible only while w3
  exists (shown on `CreateSubWindow` with `parent==w0` once w1 is assigned —
  covers both the menu-item-5 and `cl` creation sites; hidden on
  `DestroyWindow(w3)`). `w3`'s GLUT display func is never painted (no
  widget); direct `DrawCommandLine` calls draw transiently into the graphics
  FBO and self-wipe on next repaint (legacy early-returns when the flag is
  off, unchanged).
- Submit feeds the widget's final string char-by-char into w1's handler
  (identical to clean typing). Critical trap: the legacy `keystroke` buffer
  must be reset first — after a history recall it still holds the recalled
  line, so the parser appended (`"help"+"view cl off"` executed HELP; caught
  via harness log). Up/Down in the dock forwards special 101/103 and syncs
  the widget from the legacy buffer (the single source of truth).
- `w2`/`w3`/`activWindow` globals KEPT (not deleted): they are the routing
  keys for display/input/menu shims; deleting them is churn without benefit.
- `qt/selftest.cpp` (`CGX_QT_SELFTEST=1`): in-app harness delivering real
  `QMouseEvent`/`QWheelEvent`/`QKeyEvent` via `sendEvent` + framebuffer
  captures + `SELFTEST` stderr markers, exiting 0 on `SELFTEST-DONE`.
  Built because Wayland compositors gate synthetic X11 input (xdotool)
  behind an unclickable approval dialog — the harness is portal-proof and is
  now the prescribed regression test (§7). 12 steps: drag, wheel, keys,
  `view cl` on/off with dock visibility asserts, dock exec + history.
- Initial-size note: the top-level still opens at layout minimum
  (~584px, not requested 784×672) on KWin — pre/post-show `resize()` and a
  deferred re-resize all had no effect; manual resize works and aspect math
  is correct at any size. Cosmetic only; user-resizable, `wsize` intact.

### Step 8 — sign-off (all green)

- Example matrix `-bg` (exit 0 + `ready/done`): disc, cylinder, sphere,
  turbine `latim.fbd`, airfoil `rae2822.fbd`, `result.frd` — 6/6.
- Full harness on xcb, native Wayland, and `DRI_PRIME=1` (NVIDIA offload):
  exit 0, `SELFTEST-DONE`, exactly 2 help executions until Step 10, 3 since Step 11 (catches phantom
  command execution like the keystroke-append bug). One Wayland run dropped
  a single key event (1 in ~5 full runs); rerun green — noted flake, the
  harness exit code + help count catch regressions deterministically.
- Paint diagnostics removed per plan (`CGX_QT_DEBUG_PAINT` block,
  `cgxGraphicsFunc`, `-rdynamic`); technique documented here for re-adding
  in minutes if ever needed.

### Step 9 — header-only menubar, obsolete black frame line removed

- Obsolete black line loop: in legacy GLUT, w1 was an embedded subwindow
  of w0, and `iniDrawMenu()` drew a bounding `GL_LINE_LOOP` in black around
  the drawing area controlled by `frameFlag` (initialized to 1). In Qt,
  `GraphicsView` is a layout-managed sibling widget, making the drawn border
  obsolete. `main.cpp` initializes `frameFlag = 0` in the GUI path before
  `cgx_main()`. The line loop is gone; the dataset legend, text, and color
  scale bar are untouched and remain exactly where they were in the strip.
- Menubar always in header:
  - Added `menuBar()->setNativeMenuBar(false)` to `CgxMainWindow` so Qt
    never exports the menu bar to external desktop panels (e.g. KDE Plasma,
    Ubuntu AppMenu) — the menu is guaranteed to stay inside the window header.
  - In `cgxAttachMenu()` (`glue.cpp`), immediately trigger
    `markMenusDirty(); cgxSyncMenuBar();` so the header menu bar is
    constructed synchronously upon application startup across all modes
    (including `-b` geometry models as well as `.frd` result models).
  - In `cgxRebuildMenuBar()` action connections, trigger `cgxSyncMenuBar()`
    after each menu callback so dataset transitions and toggles update the
    header actions immediately.
- Removed click-popup on legend strip:
  - `MenuView::mousePressEvent`/`mouseReleaseEvent` in `CgxViews.cpp` now
    swallows left-clicks without popping a `QMenu`. All menus are accessed
    exclusively via the header `QMenuBar`.
  - Middle-click, right-click, wheel, motion, and key events on the strip
    remain intact and continue to forward to legacy handlers.
- Verified:
  - `disc.fbd`: `menubar=[Viewing, Animate, Frame, Zoom, Center, Enquire, Cut, Graph, Orientation, Hardcopy, Help, Toggle CommandLine, -QUIT-]`.
  - `result.frd`: `menubar=[Datasets, Viewing, Animate, Frame, Zoom, Center, Enquire, Cut, Graph, Orientation, Hardcopy, Help, Toggle CommandLine, -QUIT-]`.
  - Framebuffer inspection confirms zero perimeter black lines; caption and legend render cleanly; headless `-bg` matrix 5/5 exit 0; selftest exit 0 `SELFTEST-DONE`.

### Step 10 — full-window opaque 3D view, 100% transparent legend + axes overlays

Layout: `GraphicsView` fills the whole window; `MenuView` (legend, legacy w0,
`width_menu` ≈ 184 px wide, full height, left) and `AxesView` (tripod, legacy
w2, bottom-right) are sibling `QOpenGLWidget`s inside `GraphicsContainer`,
raised on top with `Qt::WA_AlwaysStackOnTop`. `cgxReshape()` sets `width_w1`,
`height_w1`, `aspectRatio_w1` from the real `GraphicsView` size (full window).
Mouse/wheel/key events on the legend are forwarded to the 3D view (w1), so
dragging/zooming works anywhere in the window.

Transparency design (the part that went wrong once, see "Trap" below):

- `glClearColor` is redirected by `qt_shim.h` to `cgxClearColor` (ours). It
  looks at a **paint target** — which Qt widget's framebuffer is current — not
  at `activWindow`:
  - legend / axes target → clear to `(0,0,0,0)` (premultiplied transparent
    black; Qt composites QOpenGLWidgets with premultiplied alpha, so
    transparent pixels must also have RGB=0, otherwise a white clear washes the
    scene out).
  - 3D target → legacy colour, but alpha forced to 1.0 (the legacy
    "black background" toggle sets `backgrndcol_rgb[3]=0` too and would make
    the 3D view transparent).
- The target is set where the context is actually switched:
  `makeViewCurrent(win)` (legacy `glutSetWindow` → direct draws such as
  `DrawMenuLoad()` after a dataset switch, `DrawAxes()`), and by `runPaint()`
  in `cgxCallMenuDisplay/AxesDisplay/GraphicsDisplay` (Qt `paintGL`).
- `viewFor(w2)` resolves to the **axes** widget (it used to resolve to the 3D
  view). Direct legacy draws into w2 now land in the axes framebuffer.
- `cgxSwapBuffers()` schedules one `update()` of the target's widget, only when
  not inside a paint (`s_paintDepth == 0`) — direct C draws then get
  composited, and nothing can re-trigger itself (idle CPU is 0).
- No tint/alpha hack: with or without results the legend background is fully
  transparent; only text, numbers and the colour bar are drawn.

**Trap (cost a broken build; keep in mind on every replay):** the first
version of this step called `cgxSetWindow(w2)` inside `cgxCallAxesDisplay()`.
Because `viewFor(w2)` mapped to the 3D view, the 3D view's FBO became current
and `DrawAxes()` cleared it to transparent black: the whole window was black.
Never call `cgxSetWindow/makeCurrent` inside the `cgxCall*Display` wrappers —
Qt has already made the right widget current. Also: the earlier "verification"
only looked at widget framebuffers (`grabFramebuffer`) and a pixel sample, and
passed while the real window was broken.

Verification (selftest, `CGX_QT_SELFTEST=1`, exit 0 on `SELFTEST-DONE`,
exit 1 on `SELFTEST-FAILED`):

- `screenCheck()` at base / after rotate / after zoom / after window resize
  compares the Qt-composited top-level (`QWidget::grab()`) with the 3D view's
  own framebuffer: outside the overlays they must be identical
  (`outsideOverlayDiff≈0`, not black), the legend background must show the 3D
  scene (`legendDiff` small = only text/colour bar), and the four inset corners
  of the axes widget must show the 3D scene (no box). The axes check counts
  alpha>0 pixels (tripod) instead of "dark" pixels.
- The check was proven able to fail: with the clear shim forced transparent for
  all targets it reports failures and exits 1; in the good build it passes.
- **`QScreen::grabWindow()` is useless here** — it returns solid black for the
  GL-composited window even when the app is fine. Use `QWidget::grab()` in-app,
  or an external compositor screenshot (e.g. `spectacle`/PIL `ImageGrab`) for
  eyes-on checks.
- Extra selftest env: `CGX_QT_SELFTEST_CMDS="ds 1 e 1"` runs commands after
  load (shows a result so the legend is active); `CGX_QT_SELFTEST_WHEEL=-8`
  zooms in (negative = other direction) so geometry passes under the legend.
- Results: geometry (`-b disc.fbd`) and result (`result.frd`, with and without
  `ds 1 e 1`) all pass; `-bg` matrix 5/5 exit 0; no `U glut*`; only Qt6/GL/GLU
  linked; idle CPU 0 over 6 s; real-desktop screenshot confirmed.
- Readability note: with a fully transparent legend, black text sits directly
  on the model; on dark meshes or in dark-background mode it can be hard to
  read (accepted trade-off of 100% transparency).

### Step 11 — console panel (last 5 lines of stdout, scrollbar, 70% opaque, selectable)

What it is: while the command line is on (menu "Toggle CommandLine",
`view cl`, or Alt+C anywhere in the window), a full-window-width panel pops up
directly above the command line
showing the last lines of cgx's console output (exactly 5 lines high, own
vertical scrollbar, ~70% opaque = 30% transparent background
(`ConsoleView::kBackgroundAlpha = 178`; was 153/60% first), black text on white or
white text on black following cgx's background colour). `view cl off` hides both. Typed
dock commands are echoed as `: cmd`. The panel is above the legend and axes.

**Capturing the output without touching the legacy code**
(`qt/ConsoleCapture.{h,cpp}`, installed in `main.cpp` before `cgx_main()`, GUI
mode only — `-bg` never touches fd 1):
- All ~10,500 output calls in cgx are plain `printf` to stdout; stderr is never
  used. So: `dup(1)` saves the original stdout, a pipe replaces fd 1, stdout is
  made line-buffered (`setvbuf _IOLBF`, else a pipe makes it fully buffered and
  the panel would update every 4 KB).
- A dedicated reader thread drains the pipe, **writes every byte to the saved
  original fd (tee: terminal or `> log` output is unchanged and verified
  byte-identical)** and appends to a mutex-protected buffer; the GUI polls it
  every 50 ms (`ConsoleView::poll`, which also `fflush(stdout)`s so prompts
  without a newline show up).
- **Why a thread (deadlock):** cgx prints from the GUI thread. If that thread
  had to drain the pipe too, the 64 KB pipe would fill during a big read and
  cgx would block forever. Regression test: the selftest prints 30000 lines
  (1.7 MB) from the GUI thread in a tight loop and must still finish.
- The reader thread blocks SIGPIPE, so `cgx | head` does not kill cgx from that
  thread (verified: exit 124 from the timeout, not 141).
- Pending buffer capped at 8 MB (oldest dropped, `bytesDropped()`); the panel
  keeps the last 5000 logical lines. An `atexit` hook flushes, restores fd 1 and
  joins the thread so no tail output is lost.
- Control characters are filtered except `\n \r \b \t`; `ConsoleView` treats
  `\n` as new line, `\b` as erase (cgx's echo hack), lone `\r` as "redraw
  line" (progress output), `\r\n` as a plain newline.

**Panel widget** (`qt/ConsoleView.{h,cpp}`): a `QOpenGLWidget` painted with
`QPainter`, with its own text wrapping (monospace, by character count) and a
hand-drawn always-visible scrollbar (wheel = 3 lines, click above/below thumb =
page of 5 lines, thumb drag). Follows the bottom unless you scrolled up. No
focus (cgx hotkeys keep working); other clicks are swallowed so they do not
reach the 3D view. Anchored to the bottom of `GraphicsContainer`
(`layoutOverlays()`), shown/hidden from `CgxMainWindow::setCmdLineVisible` —
the single hook both the menu entry and `view cl` go through. The dock command
echo lives in `submitCmdLine` (`poll()` first so earlier output stays above it).

**Why a QOpenGLWidget and not a QPlainTextEdit (spike result, keep):** the
legend and axes are stacked-on-top (`Qt::WA_AlwaysStackOnTop`) GL widgets.
- An ordinary widget can never be placed above them (they are composited over
  all normal widgets).
- Removing `WA_AlwaysStackOnTop` from the legend/axes makes them render as
  **opaque black boxes** on the real screen (non-stacked GL widgets are not
  alpha-composited here) — and the console was not visible either.
- So the console is itself a stacked-on-top GL widget. Among stacked-on-top
  siblings `raise()` does **not** change the order (the console stayed on top
  even when the legend was raised after it); it is on top because it is created
  last, inside `GraphicsContainer`.

**Verification traps (keep):**
- `QWidget::grab()` is **not** a valid proxy for on-screen stacking of these GL
  widgets: it showed saturation 0 inside the panel where the real screen shows
  102 (then; 77 at 70% opacity), and a negative test (legend raised above the console)
  did not fail it.
  The in-app "above legend" check was therefore removed. Stacking is verified
  by `tools/console_screencheck.py`, which grabs the real screen (PIL
  `ImageGrab`) and compares the legend colour-bar saturation above the panel
  (255) with inside the panel (77 = 30% of 255 at the current 70% opacity; 102 at
  the original 60%), i.e. tinted by the panel. FAIL threshold is 150.
  That script was proven able to fail: with the console's stack-on-top
  attribute removed it reports `FAIL: console is NOT above the legend`
  (inside=255) and exits 1.
- `stdin` is not read by the GUI loop, so commands cannot be piped in for
  screenshots. Use `CGX_QT_KEYS="ds 1 e 1;view cl;help"` (new, independent of
  `CGX_QT_SELFTEST`): types the `;`-separated commands into the 3D view after
  the model loaded and stays open; prints `KEYS-DONE view=… console=…` with
  screen coordinates for external tooling. (Run with `QT_QPA_PLATFORM=xcb`.)
- While the console is visible it deliberately tints the axes/legend below it;
  `screenCheck()` excludes the console rectangle (axes-corner and
  outside-overlay comparisons).

**Selection and copy (amendment; all in `ConsoleView`, no other file but the
focus-return wiring in `GraphicsContainer`):**
- Drag with the left button selects (highlight = translucent blue per display
  line, painted under the text). Double click = word (run of non-blank
  characters), triple click (a press right after a double click at the same
  spot) = display line. Ctrl+A = everything. Selection is by (display line,
  column); hit-testing uses the same metrics as painting (measured
  `QFontMetrics::horizontalAdvance` of the actual substrings — never
  `'M'`-advance multiples, which left the highlight short of the glyph ink
  while the copied text was already complete — plus `lineHeight()`, margins,
  `m_scroll`) and picks the nearest column boundary. The highlight rect and
  `cellPos()` use the same measured widths, so box, caret and `drawText`
  always agree.
- **Copied text is rebuilt from the LOGICAL lines:** `selectedText()` walks
  `m_wrapCount` to know which display lines are continuations of one logical
  line and re-joins them without a newline, so a line that was only wrapped
  for display pastes as one line. Select-all == `plainText()` (tested; a
  negative test with the join broken fails it).
- Copy: Ctrl+C (selection, or the whole console if nothing is selected),
  right-click menu (Copy selection / Copy all / Select all; right click runs
  `QMenu::exec` directly in `mousePressEvent`), and finishing a selection also
  fills the X11 primary selection (middle-click paste elsewhere, like a
  terminal; `QClipboard::supportsSelection()`). The panel stays read-only.
- Any new output (`appendChunk`) or re-wrap (`rebuildVisual`) clears the
  selection, because trimming/re-wrapping shifts the display-line indices. No
  auto-scroll while dragging beyond the panel edge: the selection clamps to the
  visible lines.
- **Alt+C toggle (amendment):** a single `QShortcut(Alt+C)` with
  `WindowShortcut` context on `CgxMainWindow` (fires from any focus: 3D view,
  legend, dock, console; `autoRepeat(false)`) calls `cgxToggleCommandLine()`
  (`glue.h/.cpp`), which calls the legacy `menu(5)` — the exact entry point
  the menubar action uses. There is deliberately no shortcut on the menubar
  action itself (the bar is rebuilt on every menu mutation; two triggers would
  double-toggle) and no new toggle logic anywhere: visibility still follows the
  w3 create/destroy shims, and focus behaviour matches the menu entry (no
  focus change). Guards: Alt-modified keys are swallowed before legacy
  forwarding in `forwardKeyPress()` (3D view + legend), `ConsoleView`
  (before the focusReturn forward) and the dock `eventFilter`, because GLUT
  has no Alt concept and the character would otherwise land in the keystroke
  buffer. Caveat: Alt+C does nothing while a modal menubar dropdown is open.
  Tested: binding present with the right key/context; with the shortcut
  disabled the keypress has no side effect (no leak, no visibility change)
  from all three focuses; end-to-end off→on→off flips dock + console +
  `commandLineFlag`. (Test note: synthetic `sendEvent` keypresses DO reach
  `QShortcut`s — the first version of this test toggled three times during
  the "no side effect" phase and had to disable the shortcut for that phase.)
- **Focus (changed from "never focus"):** `ClickFocus`. A click into the text
  gives the panel keyboard focus (Ctrl+C needs it). To avoid the trap "click the
  panel, then hotkeys stop working", every key the panel does not use is
  **forwarded to the 3D view** (`setFocusReturn(graphics)`), so typing cgx
  commands still works; Esc clears the selection and moves the focus back; a
  click into the 3D view takes the focus back too. Scrollbar clicks do not take
  focus.
- Tests (selftest, xcb): range text, select-all == plainText with the 300-char
  wrapped line unbroken, `copySelection`/`copyAll`/Ctrl+C with and without
  selection, drag and reverse drag (`cellPos()` gives widget coordinates for a
  display line/column), primary selection after a drag, double click = word,
  triple click = line, new output clears the selection, click focuses the panel,
  a `help` typed into the focused panel reaches cgx (hence 3 helps), Esc returns
  focus to the 3D view. Eyes-on: `s11_selection.png` in the selftest's shot dir (written by
  the selftest; path printed as `SELFTEST shotdir=…`) shows the highlight, also across two lines.

Verification (selftest, exit 0): geometry (`-b disc.fbd`), `result.frd`, and
`result.frd` with `ds 1 e 1` + zoom all pass; checks: panel visible exactly
when the command line is, height = 5 lines, full window width, bottom edge
directly above the command line, startup output ("reading …") and `help` output
present, scrollable and following the bottom, wheel scrolls up, scrollbar
page-up/page-down/drag, dock command echoed, 30000-line flood (no deadlock, 5000
line cap, newest line present, oldest trimmed), hidden again on `view cl off`.
Also: `> log` tee identical (all 30000 flood lines in the file; file size >=
bytes captured at the last report, the difference being output printed after it), `-bg` matrix 5/5 exit 0 (capture never installed),
no `U glut*`, only Qt6/GL/GLU linked, idle CPU 1 tick over 8 s with the panel
visible, dark mode eyes-on (`view bg k`: black 70% panel, white text).
Not done on purpose: Qt's own warnings (they stay on the terminal only), moving the axes tripod out from under the panel (you asked for
the panel to be above all other widgets).

### Step 12 — Qt-native hardcopy (no ImageMagick, all formats, no stalls)

Why: the legacy pipeline screenshotted the GLUT windows with `glReadPixels`,
then shelled out to `mogrify`/`composite`/`convert` and waited unconditionally
(`while(access("3__.tga"))`). Stock Ubuntu has none of those tools, so every
hardcopy stalled forever at 100% CPU — reproduced under gdb (timeout kill, no
progress past the `composite: not found` line; the reported SIGABRT did not
reproduce in the PNG path in 100 s and stays unattributed). Even with the
tools installed the composite math (hardcoded `width_menu` offsets, GLUT
subwindow geometry) no longer matches the full-window + overlay layout.

- Interception is source-level (`cgx.h` tail maps the three names under
  `CGX_QT`; the legacy bodies in `cgx.c` are compiled out with `#ifndef
  CGX_QT`). Upstream touches 3–4 in §4; without CGX_QT the file builds
  byte-identical.
- **Trap — linker `--wrap` does NOT work here (tried, reverted):** GNU ld only
  rewrites references that stay undefined, and same-TU calls (all of
  `DrawGrafic*`'s `createHardcopy(3, NULL)`) bypass it — proven with a minimal
  test that prints "real" with and without `-O2`/inlining. On top of that GCC
  clones constant-argument call sites (`createHardcopy.part.0`), which are
  differently-named symbols the wrap can never see. The source rename is
  immune to both (inlining our function is semantically identical).
- Capture: `grabFramebuffer()` per view (the sanctioned API — never raw
  `glReadPixels` with legacy-sized mallocs); 3D opaque full-window as the
  base, legend + axes blended at their real widget geometry (devicePixelRatio
  aware). Nothing is captured from the menubar/dock (legacy didn't either).
- Deferred queue: `createHardcopy` also runs inside `DrawGrafic*` paint paths
  (movie frames), where grabbing would re-enter painting — the wrapper only
  enqueues (+`update()`s) and a queued singleshot captures after the paint.
  FIFO preserves order (a backlog quirk this caused — frames queued during a
  drag outliving the recording — is killed by a recording-generation guard
  that drops stale jobs).
- Formats, mirroring legacy counters/names/messages/`write2stack` exactly:
  PNG via `QImage`, TGA via a small writer (bottom-up, correct orientation),
  PS as raster-EPS via a small writer, single GIFs + movie frames via
  `ffmpeg` (`QProcess`, no shell) with keep-PNG fallback + message when it is
  absent. Movie assembly uses the concat demuxer with an explicit numeric file
  list (`-r 2`, `-loop 0`); `-framerate` is invalid there (input-device
  option — cost one failed assembly to learn). Per-frame cost ≈ legacy
  `convert` cost. Direct `movie`/`hcpy make` commands (`pre_movie`,
  still `convert`-based) are out of scope and stay as-is.
- **Trap — Ghostscript 10.06 renders ~nothing from procedure-fed
  `image`/`colorimage` EPS** (verified down to 16×16 test files while a vector
  `rectfill` of identical geometry is pixel-exact — mechanism unknown,
  possibly scanner buffering around `currentfile`). So the EPS writer emits
  run-length-encoded `rectfill`s only (white runs skipped): 76 KB for a
  640×450 shot, gs-verified pixel-identical to the PNG.
- Verified: `hcpy png/tga/gif/ps` → openable files with legend + colour bar +
  tripod (PNG preview matches the live view); `movi frames 2` → `movie.gif`
  (2 frames), frames cleaned; all with `create`/`ready`, zero `not found`,
  zero stalls; selftest/`-bg`/linkage/idle-CPU unchanged. Needs an unlocked
  session for eyes-on screen checks (`loginctl LockedHint`); file checks do not.

### Step 13 — command line as an overlay band (3D view no longer squeezed)

Why: Step 7 put the command line (legacy `w3`) into the central layout, so
toggling it on shrank the 3D view by its height. Legacy never does that:
`w3` is a `glutCreateSubWindow` of `w0` at `y = height_w0 -
pixPerChary[menu_font]`, one line tall (`cgx.c:1570`, again `:4530`) — a
bottom band drawn *over* the full-window view. Requirement: same opaque band
in the same place, the view keeps the whole window (minus the menubar), and
the line must not fight the stacked GL overlays.

- Ownership moved into `GraphicsContainer`: it creates the `QLineEdit` with
  a fixed height (so the band size is deterministic for layout),
  `CgxMainWindow` only wires placeholder/Return/event-filter — **no layout
  slot at all** (the single `addWidget` in the vbox was the whole bug).
- `layoutOverlays()` derives `clH` from the line's visibility and excludes
  the bottom `clH` band everywhere: legend height `height()-clH`, axes
  tripod lifted to `height()-clH-kAxesMargin-kAxesSize` (same absolute
  position as before the band existed), console at
  `height()-clH-panelHeight()`, line at `height()-clH`. Documented z-order:
  3D view → legend → axes → line → console.
- **Trap — an ordinary widget can never be painted above the
  `WA_AlwaysStackOnTop` GL overlays** (`ConsoleView.h:6-12`); z-order
  fighting is a dead end. The solution is *geometry exclusion*: nothing else
  occupies the band, so the line gets its pixels and its clicks anyway
  (`raise()` only has to clear the plain 3D view below it).
- Toggle path: `cgxToggleCommandLine` (`glue.cpp:525`) →
  `setCmdLineVisible` → `GraphicsContainer::setCommandLineVisible` — line
  first (the band height is read from its visibility), console follows, hide
  removes both. Focus mirrors legacy `activWindow = w3` (`cgx.c:1570`):
  **focus the line on show**, focus back on the 3D view on hide; `Esc` in
  the line returns focus to the view and *keeps* the text (same contract as
  the console panel); Alt-modified keys are swallowed so no `Alt+C` character
  leaks into the parser.
- Latent gap removed: with the line in the layout, toggling resized the
  `GraphicsView` child without a top-level resize, so
  `CgxMainWindow::resizeEvent` — the only caller of `cgxReshape`
  (`qt/CgxMainWindow.cpp:214`) — never ran and `width_w1`/`height_w1`/
  `aspectRatio_w1` stayed stale until the next window resize. As an overlay
  the view size is toggle-invariant (now asserted).
- Selftest (case 8): view W/H recorded before `view cl` must be unchanged
  after ("no squeeze"); the line must live in the graphics container;
  `hasFocus()` after toggle-on; and `QApplication::widgetAt()` on the band
  centre must resolve to the line — Qt's real routing including the stacked
  overlays, so this fails if anything ever covers the band again.
  `screenCheck()` now skips `clRect` in the main loop *and* in the
  axes-corner probe (the line deliberately covers the 3D view).
- Verified: both selftests (`result.frd`, `-b disc.fbd`) exit 0 / 0 FAIL /
  3 help executions / `cmdline-visible` 1→0, `cmdline focus=1
  band-hit=QLineEdit`; KEYS geometry `view=483,201,640,450
  console=483,541,640,78` → 32 px band, view height 450 = window 480 −
  menubar 30 (whole window, no squeeze); screen-check lines + 
  `console_screencheck.py` PASS with the band excluded; hardcopy, `-bg`
  matrix, linkage and idle CPU (0.3%) unchanged. External XTEST checks are
  subject to the Wayland caveat in §7 — keyboard injection into the focused
  line and click-driven focus changes were confirmed from outside;
  positional click verification is the selftest's job.
- Found in this step's regression, in Step 12's `qt/Hardcopy.cpp`: the
  ffmpeg concat list leaked — `QTemporaryFile::setAutoRemove(false)`
  contradicted its own "autoRemove on destruct" comment and left
  `cgx_mov_XXXXXX.txt` in the work dir after every `movi`. Override removed;
  `movi frames 2` re-run leaves no file, frames cleaned, `movie.gif` ok.

### Step 14 — drop the last glut-3.5 dependency (in-tree constants header)

Why: after Steps 5–6 the vendored GLUT was dead weight except for one file.
`<GL/glut_cgx.h>` was still included by `extUtil.h`, `qt/glue.cpp` and
`qt/CgxViews.cpp` purely for GLUT_* *values* — 36 of them (buttons/states,
`glutGet` query codes, special keys, display-mode/menu bits, the six bitmap
font names). That kept a third-party sibling (Kilgard's non-GPL notice) in
the repo and an external include dir in CMake for zero runtime value.

- New `src/glut_constants.h` (**OURS**): those 36 defines with values
  bit-identical to GLUT 3.7 — every consumer is cgx plus our own shims, so
  consistency holds by construction, and identical values mean no numeric
  behaviour can drift. Carries the upstream CALCULIX/GPL header, so **no
  third-party code remains in the tree** (only `libSNL`, itself GPLv2).
- Includes swapped: `extUtil.h` → `"glut_constants.h"`, `qt/glue.cpp` and
  `qt/CgxViews.cpp` → `"../glut_constants.h"`; stale comments updated
  (incl. glue.cpp's "colors stay on real vendored GLUT" line, obsolete
  since Steps 5–6).
- **Trap — `glut_cgx.h` was also including `<GL/glu.h>`**: `extUtil.h`
  needs `GLUnurbsObj` (a GLU type it declares around the NURBS helpers),
  so the first rebuild died with `unknown type name 'GLUnurbsObj'`. Fix:
  `#include <GL/glu.h>` explicitly in `extUtil.h` (recorded in §4 Edit 2 —
  an include swap always needs a rebuild *before* you conclude the
  dependency was unused).
- `CMakeLists.txt`: `../../glut-3.5/src` dropped from the include dirs
  (comment block updated). `../../libSNL/src` stays — it is compiled and
  linked, a genuine dependency.
- Fonts unchanged: under `CGX_QT`, `cgx.h` redefines `GLUT_FONT` to the
  positional tokens `(void*)0..5` (Step 6) which `CgxFont.cpp` maps to
  QFonts; the `GLUT_BITMAP_*` names are kept only for the original macro
  at the top of `cgx.h` (never expanded after the `#undef`).
- Not touched on disk: `/usr/local/CalculiX/glut-3.5` remains for the old
  cgx versions (2.5/2.22); cgx_2.23 simply never reads it any more.
- Verified: `grep glut_cgx|glut-3.5` over sources/build files → comments
  only; rebuild rc=0; `-bg` rc=0; selftest rc=0 / 0 FAIL /
  `SELFTEST-DONE`; `readelf -d` still Qt6+GL/GLU only; `nm` still 0
  undefined `glut*`. Repo side: `glut-3.5/` removed in the follow-up
  commit after the initial push (needs no force-push).

### Step 15 — security audit; test hooks compiled out of the default build

Why: the Qt layer was AI-generated, so the whole tree (upstream cgx C, libSNL,
`qt/`, build files, binary) was audited for data exfiltration and anything
remotely triggerable, then verified at runtime.

- **Static:** no socket/DNS/HTTP/DBus/`QNetwork` code anywhere; only Qt Core/
  Gui/Widgets/OpenGL(Widgets) linked; the binary imports no network symbol
  (`system()` is the only exec-type import and is upstream); the single URL
  string is `http://www.calculix.de` in exported-file headers; no `execute_process`/
  download/custom command in CMake, no RPATH, no base64/hex blobs, no
  non-ASCII tricks, no ELF/`.so` files in the repo, only three plain upstream
  example scripts. Our one child process: `ffmpeg` via `QProcess::execute`
  with an argument list (no shell), replacing legacy `system("convert …")`.
- **Runtime (executed):** `bwrap --unshare-net … -bg disc.fbd` rc=0; GUI
  under `strace -f -e trace=%network,execve`: only `AF_UNIX` sockets (X11,
  session bus, AT-SPI bus, ICE, nscd) — no `AF_INET/6/PACKET`, no exec except
  `ffmpeg` and upstream's `rm -f _*.gif` in the hardcopy/movie path; `ss`/`lsof`
  show no TCP/UDP socket and no child process; file writes only the requested
  outputs, one `/tmp/cgx_frame_*.png` and the Mesa shader cache.
- **Residual surface (upstream behaviour, documented in the howto §8):** the
  `sys` command (locked by default: `ALLOW_SYS_FLAG 0`, unlocked only via
  `ALLOW_SYS` in `~/.cgx`), unhardened C parsers (886 `strcpy`, 1263
  `sprintf`, 178 `scanf("%s")`; mitigated by PIE/full RELRO/BIND_NOW/stack
  protector/fortify), and the banner printing host name and `$HOME`.
- **Change (ours):** `CGX_QT_SELFTEST`, `_CMDS`, `_WHEEL` and `CGX_QT_KEYS` can
  type commands into the program, so they no longer exist in the default
  binary. `qt/selftest.cpp` is compiled only with `-DCGX_QT_TESTHOOKS`
  (CMake `option(CGX_QT_TESTHOOKS … OFF)`; test binary in `build-test/`);
  otherwise `cgxMaybeStartSelftest()` is an empty stub. Verified:
  `strings build/cgx | grep CGX_QT_` → 0 hits, and a default binary started
  with `CGX_QT_SELFTEST=1 CGX_QT_KEYS="hcpy png"` ignores both.
- **Change (ours):** the hard-coded absolute screenshot directory under `/tmp` is
  gone (it was a predictable, shared path): shots
  go to `CGX_QT_SHOT_DIR` or a private auto-removed `QTemporaryDir`
  (path printed as `SELFTEST shotdir=…`). `tools/console_screencheck.py` now
  runs `./build-test/cgx` (override with `CGX_BIN`).
- Verified on the test build: selftest rc=0 / 0 FAIL / `SELFTEST-DONE` /
  3 helps, hardcopy png/tga/gif/ps, `movi frames 2` → `movie.gif` without
  leftovers, `console_screencheck.py` PASS; default build `-bg` rc=0.

## 9. Roadmap — complete

All steps landed. For future upstream re-applies: follow §5, then §7
(including the selftest run on xcb). Open polish (non-blocking): initial
window size (§Step 7 note), `Reshape/Entry/VisibilityFunc` shims still
parked.

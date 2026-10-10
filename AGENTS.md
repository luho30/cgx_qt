# AGENTS.md

## Scope
- This repo is `cgx_2.23/` (Qt6 port of CalculiX GraphiX) plus `libSNL/` (real build dependency; CMake globs `libSNL/src/*.cpp`, auto-detected at `<tree>/libSNL/src` or `<parent>/libSNL/src`, override with `-DCGX_LIBSNL_DIR`).
- Other top-level dirs (`ccx_*`, `cgx_2.5`, `cgx_2.22`, `glut-3.5`, `*.tgz`) are untracked and out of scope. Don't edit or stage them.
- Work happens in `cgx_2.23/src/`. No CI, lint, formatter or unit-test suite; verification is the build check + in-app selftest.
- `origin` is `git@github.com:luho30/cgx_qt.git` (`main`); the owner also commits on GitHub, so `git pull --ff-only origin main` before starting.

## Build (from `cgx_2.23/src`)
- `./build.sh` = configure + build + check -> `build/cgx`. Extra args go to cmake. Env: `CGX_BUILD_DIR`, `CGX_JOBS`, `CGX_SKIP_CHECK=1`. It recreates `build/` if the tree was moved/renamed.
- The check (`cmake/RunCgxCheck.cmake`) runs headless `-bg disc.fbd` + `cgx --glcheck`; it also runs as POST_BUILD, `--target check`, ctest and at `cmake --install`.
- Test build with env hooks needs its OWN dir: `CGX_BUILD_DIR=$PWD/build-test ./build.sh -DCGX_QT_TESTHOOKS=ON`.
  `./build.sh -DCGX_QT_TESTHOOKS=ON` alone reuses `build/` and bakes the hooks into the default binary (cmake caches the option; fix with `-DCGX_QT_TESTHOOKS=OFF`).
  The default binary must contain no hooks: `strings build/cgx | grep -c CGX_QT_` -> 0.
- C is compiled as gnu17 on purpose (GCC 15's gnu23 breaks K&R code in `improveMesh.c`).
- Adding/removing a `.c` or `qt/*.cpp` file means editing the explicit lists (`CGX_SLIB`, `CGX_QT_SRCS`) in `CMakeLists.txt`. Only `libSNL/src/*.cpp` is globbed.
  `XFunktions.c` and `readStdCmap.c` are deliberately excluded (dead X11 code).

## Verify
- Headless: `./build/cgx -bg ../examples/basic/disc.fbd | tail -2` (exit 0). `./build/cgx --glcheck` needs a display.
- GUI selftest (real display required; `QOpenGLWidget` fails on `QT_QPA_PLATFORM=offscreen`):
  `CGX_QT_SELFTEST=1 QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd`
  Expect exit 0, `SELFTEST-DONE`, no `SELFTEST-FAIL`. Also run with `-b ../examples/basic/disc.fbd`. Add `CGX_QT_SHOT_DIR=<dir>` to keep screenshots.
  Hardcopy tests leave `hcpy_*` files in `src/`; delete them.
- Scripted commands (test build only): `CGX_QT_KEYS="ds 1 e 1;view cl" ./build-test/cgx ../examples/result.frd`
- Linkage: `nm build/cgx | grep " U glut"` empty; `readelf -d build/cgx | grep NEEDED` only Qt6, OpenGL/GLU, libc/libstdc++.
- Overlay stacking only verifiable on the real screen: `python3 ../tools/console_screencheck.py`. `QWidget::grab()` / `QScreen::grabWindow()` are not valid proxies for stacking.
- Full matrix: `cgx_2.23/QT-PORT.md` section 7.

## Architecture
- Upstream cgx C code (`src/*.c`) is deliberately left nearly untouched. The Qt port is a shim layer:
  `cgx.h` includes `qt/qt_shim.h` under `CGX_QT`, which `#define`s each `glutX` call to a `cgxX` function in `qt/glue.cpp`.
  `cgx.c`'s `main` is renamed `cgx_main` by a compile definition; `qt/main.cpp` owns the real `main`.
- Only three upstream files are edited, all under `#ifdef CGX_QT` or as include swaps: `cgx.h` (tail), `extUtil.h` (include block), `cgx.c` (`#ifndef CGX_QT` around 3 hardcopy bodies). New features go into `src/qt/`, not upstream files.
- A new `glut*` call in upstream code intentionally fails to link. Add a real shim, never a silent no-op.
- Legacy window ids are routing keys: `w0` legend, `w1` 3D view, `w2` axes, `w3` command line; handlers are stored per window id (`activWindow`).
- `cgx.h` has no `extern "C"` guards and pulls in the glut macros. C++ files must not include it; declare legacy C functions `extern "C"` at namespace scope (in `qt/glue.h`; block-scope `extern` gets C++ linkage and fails to link). For access to legacy structs (`set`, `pset`, ...) write a small C file with a plain-C header, like `qt/DisplaySetsBridge.{c,h}`.
- GL context is compatibility profile 2.1 (display lists, `GL_SELECT` picking). Don't switch to core.
- Never call `cgxSetWindow`/`makeCurrent` inside the `cgxCall*Display` wrappers: Qt already made the right widget current; doing so blacked out the whole window.
- Legend and axes are `WA_AlwaysStackOnTop` transparent GL widgets; no ordinary widget can sit above them. Solve layout by geometry exclusion (`layoutOverlays()`), not z-order.
- `qt/main.cpp` forces `LC_NUMERIC=C` after creating the Qt app; without it comma-decimal locales break `atof`/`sscanf`.
- stdout is captured by `ConsoleCapture` (pipe + reader thread, tee to the original fd); `-bg` never installs it.
- Hardcopy is source-renamed, not linker-`--wrap`ped (`--wrap` misses same-TU calls); capture is deferred until after paint.
- Menus: GLUT menu API is mirrored in `glue.cpp` and rendered into a `QMenuBar` (rebuilt when dirty). Persistent Qt-only menus ("Display Sets", `qt/DisplaySets.*`) are inserted in `cgxRebuildMenuBar()`; it must not rebuild while that popup is visible.
- Displayed sets are the C array `pset[]` (`anzGeo->psets`), changed only through the `plus`/`minus`/`plot` commands. Drive those, don't edit `pset` directly.

## Gotchas
- `-bg` needs no display; every GUI path needs a real X11 or Wayland session.
- On Wayland, XTEST pointer injection (xdotool) is unreliable (keyboard works). Use the in-app selftest (`sendEvent`, `clickCell`) for positional input.
- GIF/movie output needs `ffmpeg`; without it PNG frames are kept.
- cgx's `sys` command runs shell commands and is locked by default. If cgx prompts about it, answer `s` or `c`, never `e`: `e` appends `ALLOW_SYS` to `~/.cgx` and unlocks `sys` for every future file. Never write `ALLOW_SYS` there yourself.
- `build*/`, `hcpy_*`, `movie.gif`, `all.fbd` are gitignored; don't commit them.

## Port documentation (read before touching `src/qt/` or merging a new upstream cgx)
- `cgx_2.23/QT-PORT.md` is the authoritative port log: section 1 file ownership, 4 upstream edits, 5 re-apply recipe for a new `cgx_X.XX`, 7 verification matrix.
- Standing rule: every step updates the status line, adds a `### Step N` entry (last is Step 16, Display Sets) and the section 1/4 tables if files change.
- `/usr/local/CalculiX/README.md` is the user-level install/run guide; update it for user-visible features.

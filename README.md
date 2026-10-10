# CalculiX/CGX 2.23 — Qt6 version

How to install, build and start, plus what is new.

- **Applies to:** the `cgx_2.23/` tree in this repo (the Qt6 port of cgx) —
  it builds from any directory, nothing is hardcoded to a path
- **Written for:** Ubuntu 26.04, x86_64
- **Verified on:** Qt 6.10.2, GCC 15.2, CMake 4.2, Mesa/nouveau + Intel/NVIDIA hybrid
- **Date:** 2026-10-03, build instructions updated 2026-10-06
- **License:** GNU GPL version 2 — same as cgx (see [cgx_2.23/COPYING](cgx_2.23/COPYING))

This document is the user-level guide. The developer-level port log with
rationale, traps and replay instructions for future upstream updates lives in
[cgx_2.23/QT-PORT.md](cgx_2.23/QT-PORT.md). Commands below are copy-paste
ready and were run as shown during the final regression.

## 1. Prerequisites (once per machine)

Everything is packaged; nothing is built from source.

Minimal set (from the actual include/link analysis — only Xlib.h is used
beyond Qt/GL/GLU; the full set below is what was installed in practice):

```bash
pkexec apt-get install -y \
  g++ cmake qt6-base-dev \
  libgl-dev libglu1-mesa-dev libx11-dev
```

(use `sudo apt-get ...` if your setup allows sudo)

What each one provides:

```text
g++             C/C++ compiler (GCC 15 tested)
cmake           build driver (out-of-source build)
qt6-base-dev    Qt 6 Core/Gui/Widgets/OpenGL — the UI toolkit, and the
                *only* GUI library the binary ends up depending on
libgl-dev       OpenGL headers (GL/gl.h), libGL
libglu1-mesa-dev GLU — still used by the modelling/NURBS code (gluProject,
                gluPickMatrix, gluNewNurbsRenderer)
libx11-dev      Xlib headers only (extUtil.h needs a few types); the
                Qt6 build does *not* link libX11
```

Full set actually used during development (harmless superset):

```bash
pkexec apt-get install -y \
  libgl-dev libglu1-mesa-dev libx11-dev libxi-dev libxmu-dev \
  libxext-dev libxt-dev libsm-dev libice-dev \
  g++ cmake qt6-base-dev
```

Optional:

```text
ffmpeg    only for movie/GIF assembly (movi/movie). If it is missing,
          cgx says so and keeps the individual PNG frames — nothing else
          breaks. PNG, TGA and PS output always work without it.
```

Not required anymore (legacy GLUT build only): freeglut, ImageMagick
(`convert`), Ghostscript.

Also required: the **libSNL sources** (part of a CalculiX checkout). CMake
looks for them at `<tree>/libSNL/src` and at the upstream sibling
`<parent>/libSNL/src` — i.e. next to the cgx directory, as in
`/usr/local/CalculiX/`. If neither exists, say where they are:

```bash
cmake -B build -S . -DCGX_LIBSNL_DIR=/path/to/libSNL/src
```

Runtime libraries of the finished binary:

```bash
ldd build/cgx | grep -i qt6        # the Qt 6 libraries in use
readelf -d build/cgx | grep NEEDED # Qt6 + libGL + libGLU only
                                   # (no libX11, no GLUT)
```

## 2. Build

The tree builds from wherever it lives: no include/library path is
hardcoded, Qt6/OpenGL/GLU/Threads/X11 are located with `find_package`, and
the install step uses `GNUInstallDirs` (the installed binary carries no
RUNPATH).

Source directory (CMake project):

```bash
cd /usr/local/CalculiX/cgx_2.23/src
```

Configure, compile and verify — recommended, one command:

```bash
./build.sh
```

`build.sh` configures `src/build`, compiles it, then runs the build check
(see below). Extra configure arguments are passed through, e.g.
`./build.sh -DCGX_QT_TESTHOOKS=ON`. Environment: `CGX_BUILD_DIR` (build
directory, default `<src>/build`), `CGX_JOBS` (parallel jobs, default
`nproc`), `CGX_SKIP_CHECK=1` (skip the trailing check).

The same by hand (out-of-source, first time):

```bash
cmake -B build -S .
cmake --build build -j$(nproc)
cmake --build build --target check
```

Result: `src/build/cgx`

Rebuilding after a source change is the second command only (CMake detects
the changed files). For a clean start:

```bash
rm -rf build && cmake -B build -S . && cmake --build build -j$(nproc)
```

### Every build and every install is checked

After a build the check script `src/cmake/RunCgxCheck.cmake` runs against the
freshly built binary and must pass:

1. **headless functional smoke** — `cgx -bg <example>` exits 0 *and* reports
   `ready` and `done`. Needs no display, so it always runs.
2. **GL/window probe** — `cgx --glcheck` must create a desktop OpenGL context
   (not OpenGL ES). Skipped when neither `DISPLAY` nor `WAYLAND_DISPLAY` is
   set; `-DCGX_REQUIRE_GL=ON` makes a missing display an error instead.

Every child call has a hard timeout (120 s / 60 s), so a check can never hang
a build or an install, and any failure stops the build — or the install —
with a non-zero exit. The check runs at four points:

| Hook | Command |
|---|---|
| after every relink (POST_BUILD) | `cmake --build build` |
| explicit target | `cmake --build build --target check` |
| CTest | `ctest --test-dir build --output-on-failure` |
| during install, against the installed `bin/cgx` | `cmake --install build --prefix <prefix>` |

Knobs: `-DCGX_BUILD_CHECK=OFF` / `-DCGX_INSTALL_CHECK=OFF` switch the hooks
off, `-DCGX_CHECK_EXAMPLE=<fbd>` selects another smoke input, `CGX_SKIP_CHECK=1`
skips the check inside `build.sh`. The check only runs your own
just-built binary — no downloads, no network (§8).

### Optional test build (selftest + scripted startup commands, see §3, §5.9, §6)

```bash
cmake -B build-test -S . -DCGX_QT_TESTHOOKS=ON
cmake --build build-test -j$(nproc)
```

The default build contains none of the `CGX_QT_*` environment hooks; only
this second binary, `src/build-test/cgx`, reads them (§8).

### Install (optional)

```bash
cmake --install build --prefix /usr/local     # -> /usr/local/bin/cgx
```

The install re-runs the check against the installed binary and aborts if it
fails (`-DCGX_INSTALL_CHECK=OFF` to install anyway). Any prefix works; nothing
outside it is touched, and the program can still be run straight from
`src/build/cgx`.

Notes:

- **Renaming or moving the tree** keeps the build directory, but its CMake
  cache records the old absolute path, so configure then stops with
  `... does not match the source ... used to generate cache`. `./build.sh`
  detects exactly that case and recreates the build directory; a plain
  `rm -rf build` does the same by hand.
- The build is CMake-only. The original GLUT Makefile pair is kept as
  `src/Makefile.legacy.bak` + `src/Makefile.inc.legacy.bak` for reference; it is
  not maintained or tested any more.
- CMake only probes locally installed packages; nothing is downloaded.
- libSNL must be present (§1); if CMake reports it missing, re-run with
  `-DCGX_LIBSNL_DIR=<path-to-libSNL/src>`.

## 3. Start

Interactive with a result file (classic post-processor mode):

```bash
./build/cgx ../examples/result.frd
```

Build mode from a geometry command file:

```bash
./build/cgx -b ../examples/basic/disc.fbd
./build/cgx -b ../examples/basic/cylinder.fbd
./build/cgx -b ../examples/basic/sphere.fbd
```

Background mode — no graphics window, works without any display:

```bash
./build/cgx -bg ../examples/basic/disc.fbd
env -u DISPLAY ./build/cgx -bg ../examples/basic/disc.fbd | tail -2
```

Usage / parameter list:

```text
./build/cgx            (banner + options, exits 0)
```

GL/window probe (no window, prints the result and exits):

```bash
./build/cgx --glcheck
# GL-CHECK OK: desktop OpenGL 4.6 profile=2 renderableType=1   -> exit 0
# exit 3 = no usable GL context -> a window would NOT open (see §7)
```

The same probe also runs on every normal GUI start (silently) and is part of
every build/install check, so a binary that could not open a window never
gets through a build.

All cgx parameters are unchanged: `-b` (build), `-bg` (background), `-v`
(default, frd result), `-c` (solver input), `-stl`, `-ng`, `-vtk`, `-foam`, ...
see the output of `./build/cgx` or [cgx_2.23/doc/cgx.tex](cgx_2.23/doc/cgx.tex).

GUI variants worth knowing:

```bash
QT_QPA_PLATFORM=xcb ./build/cgx ...       # run on the X11/XWayland backend
                                          # (the path all automated checks use)
DRI_PRIME=1 ./build/cgx ...               # NVIDIA-optimus / hybrid graphics:
                                          # offload rendering to the discrete
                                          # GPU (nouveau/Mesa tested)
```

Startup commands without typing (scripted session, `;` = Return) — test
build only (`cmake -DCGX_QT_TESTHOOKS=ON`, see §2):

```bash
CGX_QT_KEYS="ds 1 e 1;view cl" ./build-test/cgx ../examples/result.frd
```

## 4. Working with the GUI

The window consists of, top to bottom:

```text
[ menubar in the window header ]
[ legend | 3D view (axes tripod in the lower right corner) ]
[ command line  (optional, toggled with Alt+C) ]
```

Mouse bindings are unchanged from classic cgx (manual,
[cgx_2.23/doc/cgx.tex](cgx_2.23/doc/cgx.tex)):

| Button | Action |
|---|---|
| left button | rotate the model |
| middle button | zoom in/out |
| right button | translate the model |

Menus are always in the window header — they are never exported to a global
menu and never pop up from the screen edge. Clicks and drags landing on the
legend or on the axes tripod still manipulate the 3D view, so the HUD never
eats a mouse gesture.

Command line / console:

```text
Alt+C        toggle command line + console panel (both together)
Enter        execute the line, prompt stays open
Esc          leave the command line, focus returns to the 3D view
             (the typed text is kept)
Up / Down    command history
```

The console panel shows the last five output lines of cgx (scrollable), is
semi-transparent over the view, and the text can be selected and copied —
e.g. the node/element numbers of a `qenq` query.

Selection commands (`qadd`, `qenq`, `qsur`, ...): as soon as one is executed
the keyboard moves to the 3D view — the selection keys (`e a n r u q ...`) act
on the model, never on the command line. When the selection ends (`q`), the
focus returns to the command line automatically. Clicking into the line
during a selection does not trap the keys either: they keep going to the
selection until it ends.

Hardcopy (all formats produced inside cgx, no external image tools):

```text
hcpy                     -> hcpy_<n>.tga   (default format)
hcpy png / hcpy tga / hcpy gif / hcpy ps
hcpy png myshot          -> myshot.png     (the extension is added by cgx)
movi frames 2            -> record, then movie.gif (uses ffmpeg if present)
```

## 5. What is new compared with the classic (GLUT) cgx

The Qt6 port is a rendering/UI backend exchange: every cgx command, file
format and parameter behaves exactly as before. What changed is how the
window, the overlays, the input and the screenshots are produced.

### 5.1 One library instead of a vendored windowing stack

- Previous: GLUT 3.5 (vendored, X11-only), plus direct Xlib usage, plus
  ImageMagick/Ghostscript for images.
- Now: Qt 6 Widgets/OpenGL. The GLUT sources are no longer compiled; the
  binary links only Qt6, libGL and libGLU (verified with `readelf`/`ldd`).
- Consequence: it runs on modern desktops (X11 and Wayland sessions),
  uses native window decorations/fonts/clipboard, and there is no
  per-machine GLUT/X11 runtime to install.

### 5.2 One full-window 3D view, correct at every window size

- Previous: the drawing area was a separate sub-window beside the menu
  strip, fenced off by a black frame line (`frameFlag`), and the viewport
  aspect had to be derived from the window size by hand — easy to get
  wrong (it was the trickiest part of the port).
- Now: one `GraphicsView` fills the whole window below the header; the
  legend sits on top of it as an overlay. The frame line is gone, and the
  projection is rebuilt from the real widget size on every resize
  (`cgxReshape`), so circles stay circles and nothing is ever squeezed,
  letterboxed or skewed on resize / fractional display scaling. Screen
  coordinates, picking and `zoom` boxes use the same geometry.

### 5.3 Transparent HUD (legend and axes)

- Previous: the dataset legend and the axes tripod had their own opaque
  window background, painting over the scene.
- Now: both are fully transparent overlays drawn on top of the 3D view —
  the model is visible behind the colour bar and the axes tripod, with no
  tint or background rectangle. Geometry can be zoomed until it passes
  under the legend. Mouse/wheel input over these overlays is forwarded to
  the view, so they are "seamless" (dragging across the legend rotates the
  model just like dragging on open space).
- Readability note: the legend text has no background any more, so its
  contrast depends on the model behind it — a deliberate trade for the
  fully transparent HUD.

### 5.4 Command line as an overlay band + real console panel

- Previous: the command line lived in a sub-window, so toggling it made
  the 3D view smaller (and reshapes were lost until the next window
  resize), and output was only visible in the terminal.
- Now: `Alt+C` shows a one-line command field in a bottom band that is laid
  *over* the view — the view keeps its full size, nothing resizes or
  flickers. Above it the console panel shows the last five lines of cgx
  output with a scrollbar; the text is selectable/copyable and echoes the
  executed commands. Focus follows the toggle (typing goes to the line
  when it is shown, back to the 3D view when it is hidden); `Alt+C` as a
  character never leaks into the parser.

### 5.5 Menubar pinned to the window header

- Previous: menus were popped up by clicking/dragging on the menu strip
  (GLUT pop-up menus), which behaved differently depending on the window
  manager and on which screen the window was.
- Now: a normal menubar in the window header, always visible and always in
  the same place, with the same entries (no click-popup on the strip).

### 5.6 Qt-native hardcopy: PNG, TGA, GIF, PS and movies

- Previous: `hcpy`/`movi` shelled out to ImageMagick `convert` and
  Ghostscript and could stall on a busy main loop; availability of the
  external tools decided whether a shot worked at all.
- Now: PNG (QImage), TGA and PS (raster EPS) writers are built in, GIFs
  and movies are assembled with ffmpeg (optional, with a clear message and
  PNG-frame fallback when it is absent). Screenshots composite the 3D
  view, the transparent legend with its colour bar and the axes tripod at
  their real positions, are devicePixelRatio aware, and never block the
  UI. Movie frames are queued and captured after the paint, so recording
  during animation is safe.
- Performance: per-frame cost is comparable to the old `convert` path.

### 5.7 Text and fonts without X core fonts

- Previous: GLUT bitmap fonts were stroked to lines, needing exact
  character metrics; missing font data produced garbled labels.
- Now: all strings (menu, legend, caption, axes, command line) use Qt
  fonts — proper antialiased text, selectable/copyable in the console, and
  locale-independent metrics.

### 5.8 Bug fixes that came with the port

- Locale: Qt's `QApplication` constructor sets the process locale from
  the environment (e.g. `de_DE.UTF-8`, comma as decimal separator), and
  cgx's `atof`/`sscanf`/`strtod` then parse `0.70711` as `0.7` + junk —
  geometry collapsed (disc.fbd came out as a pinched star). The Qt build
  forces `LC_NUMERIC=C` right after the QApplication is created, so
  numbers always use the dot regardless of the desktop language.
- Reshape: with the old in-layout command line, toggling it changed the
  view size without a window resize, so the stored width/height/aspect
  stayed stale until the next manual resize. Fixed by construction
  (overlay, view size is toggle-invariant).

### 5.9 Quality-of-life / test hooks (mainly for maintainers)

- Scripted startup commands: `CGX_QT_KEYS="cmd1;cmd2"` (see §3).
- Built-in regression harness: `CGX_QT_SELFTEST=1` runs a fixed input suite
  (help/menu/history, view drag, legend drag, wheel zoom, command-line
  toggle, focus, band hit-testing, screen checks) and exits 0 only if
  everything passes; it prints `SELFTEST-DONE` and never hangs.
- Both hooks exist only in the optional test build (`-DCGX_QT_TESTHOOKS=ON`);
  the default binary ignores these environment variables completely.
  Screenshots go to `CGX_QT_SHOT_DIR` if set, else a private temporary
  directory that is removed at exit (its path is printed on stderr).
- Build and install are gated: every relink, `--target check`, `ctest` and
  `cmake --install` run a headless smoke test plus the GL/window probe
  `cgx --glcheck`, each child with a hard timeout (§2) — a build that could
  not open a window fails instead of starting blank.
- The whole port — including per-step rationale, known traps (stacked
  OpenGL overlays, Wayland/XTEST input quirks) and the exact regression
  commands — is documented in [cgx_2.23/QT-PORT.md](cgx_2.23/QT-PORT.md) so it
  can be replayed against a future upstream cgx release.

### 5.10 "Display Sets" menu

A new menubar entry **Display Sets** (right after *Viewing*) replaces typing
`plot`/`plus`/`minus` for the common case. The popup lists every set with a
checkbox in front of the name (set on/off) and, to the right, one checkbox per
entity type (`n e f p l s b S L`; types the set does not contain are greyed
out). More sets than fit the window get a scrollbar on the right.

- Ticking a set runs `plus <type> <set> <colour>` for its selected entity types
  (default: elements, else faces, surfaces, ...); unticking runs `minus`. The
  commands are echoed in the console. Each set keeps its own colour (swatch in
  front of the name).
- Entity boxes of a visible set add/remove that type immediately; on a hidden
  set they only choose what will be shown when the set is ticked.
- The list always reflects the real state, also for sets you plotted by typing
  `plot e *eng*` or removed with `minus e door`. `plot` and `plus` can still
  be used as before. The list also
  updates live while it is open (sets created, changed or deleted by
  commands, scripts or meshing).

## 6. Verification — the checks that were run on this machine

The first commands are one-shot (they exit on their own, printing the result
behind each comment); the interactive ones keep running — press `Ctrl+C` or
use `timeout`. Written files land in the current directory (`src/`):

```bash
# build + the automatic checks -> "cgx check: all checks passed for .../build/cgx"
./build.sh

# GL/window probe on its own -> "GL-CHECK OK: desktop OpenGL …", exit 0
./build/cgx --glcheck

# the same build check, explicitly -> exit 0 (ctest: 1/1 passed)
cmake --build build --target check
ctest --test-dir build --output-on-failure

# headless batch, works without a display -> exit 0, "done"
./build/cgx -bg ../examples/basic/disc.fbd | tail -2

# test build (once)
cmake -B build-test -S . -DCGX_QT_TESTHOOKS=ON && cmake --build build-test -j$(nproc)

# full input/rendering selftest -> exit 0, SELFTEST-DONE, no FAIL
# (needs a real display; xcb backend)
CGX_QT_SELFTEST=1 QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd

# hardcopy formats -> hcpy_1.png/.tga/.gif/.ps + `create ...`/`ready`,
# no "not found", no stall
CGX_QT_KEYS="ds 1 e 1;hcpy png;hcpy tga;hcpy gif;hcpy ps" \
  QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd

# movie -> movie.gif (2 frames), frames and temp list cleaned up,
# exit 0, SELFTEST-DONE
CGX_QT_SELFTEST_CMDS="movi frames 2" CGX_QT_SELFTEST=1 \
  QT_QPA_PLATFORM=xcb ./build-test/cgx ../examples/result.frd

# install + the check against the installed binary ->
# "cgx install check: all checks passed for .../bin/cgx"
cmake --install build --prefix /tmp/cgx-prefix
/tmp/cgx-prefix/bin/cgx --glcheck
```

Idle CPU with the window open: < 1% (no polling, update-driven redraw).

## 7. Known limitations and troubleshooting

**"Qt cannot create an OpenGL context / QOpenGLWidget not supported on the
offscreen platform"**
The GUI needs a real display. Use a normal graphical session, or run
headless with `-bg` (batch, no window).

**Session is Wayland**
Plain `./build/cgx` uses the desktop session's platform and works on X11 and
Wayland (both backends verified to start and stay up). For scripted runs
and the selftest, pin the X11 backend: `QT_QPA_PLATFORM=xcb`. Note that
externally injected mouse positions (xdotool/XTEST) are unreliable under
Xwayland — the pointer freezes at one spot; this is an environment issue,
not a cgx bug, and it is why the selftest injects input through Qt itself
(details: [cgx_2.23/QT-PORT.md](cgx_2.23/QT-PORT.md) §7).

**Compiles fine, but no window opens at all (Wayland), terminal shows
`Failed to create context: 3009` / `EGL_BAD_MATCH`**
Run the probe: `./build/cgx --glcheck`. Without a desktop OpenGL context
cgx exits **3** and prints the reason. Cause: on Wayland the surface format
must ask for a desktop OpenGL context (`RenderableType: OpenGL`), otherwise
the platform picks an OpenGL-ES EGL config, `eglCreateContext` fails with
`EGL_BAD_MATCH (3009)`, the widget never paints and the surface gets no
buffer — no window at all (on X11 you get an empty window instead). This
state cannot get past a build any more: the probe runs on every GUI start
and in every build/install check (§2). Details: QT-PORT §7.

**NVIDIA Optimus / hybrid graphics, black or slow view**

```bash
DRI_PRIME=1 ./build/cgx ...
```

**Legend text hard to read**
The legend background is intentionally fully transparent (§5.3); zoom or
rotate the model, or use `hcpy` and check the captured shot.

**Movie or GIF output missing**
Install ffmpeg; without it the PNG frames are kept and cgx prints a
message instead of assembling the GIF.

**Configure stops with `... does not match the source ... used to generate
cache` after the tree was renamed or moved**
The build directory still belongs to the old path. `./build.sh` detects this
and recreates it automatically; or do it by hand:

```bash
rm -rf build && ./build.sh
```

**Build confusion after switching branches/updates**

```bash
rm -rf build && cmake -B build -S . && cmake --build build -j$(nproc)
```

## 8. Security notes

What the Qt port does NOT do (checked by source audit and at runtime):

- no network code: Qt Network/DBus are not linked, the binary imports no
  `socket`/`connect`/`getaddrinfo` symbols, and under `strace` the GUI opens only
  local AF_UNIX sockets (X server, session/accessibility bus, ICE);
- no hidden or remotely triggered behaviour: no downloads in the build, no
  obfuscated data, no helper binaries — the only thing the build runs in
  addition to the compiler is your own freshly built cgx (the check, §2);
- the only child process the port starts is ffmpeg (argument list, no
  shell) for GIF/movie assembly, when you ask for it.

Re-check it yourself at any time:

```bash
nm -D --undefined-only build/cgx | grep -Ei " (socket|connect|getaddrinfo|dlopen|popen)"
readelf -d build/cgx | grep NEEDED             # no Qt Network / DBus
strace -f -qq -e trace=%network,execve -o st.txt ./build/cgx ../examples/result.frd
bwrap --ro-bind / / --dev /dev --proc /proc --tmpfs /tmp --unshare-net \
  ./build/cgx -bg ../examples/basic/disc.fbd     # runs fine without network
```

What you should still be careful with (this is classic cgx behaviour):

- the `sys` command runs shell commands from a command file or the command
  line. It is locked by default; if cgx asks, answer `s` (stop) for any
  file you do not trust. Answering `e` writes `ALLOW_SYS` into `~/.cgx` and
  unlocks it for every future file — delete that line to lock it again.
- the C parsers (frd, fbd, inp, stl, ...) were written without hardening;
  open files from untrusted sources only in a throw-away environment.
- the start banner prints the host name, kernel version and your home
  directory, and the console panel shows it; redact these before posting
  console output or screenshots.
- the optional test build reads `CGX_QT_*` environment variables that can
  type commands into the program: do not use it for daily work.

## 9. Where to find more

- [cgx_2.23/QT-PORT.md](cgx_2.23/QT-PORT.md) — full port log: 13 steps,
  decisions, traps, regression matrix — read this before touching the Qt
  layer or merging a new upstream cgx release
- [cgx_2.23/doc/cgx.tex](cgx_2.23/doc/cgx.tex) — the cgx manual (commands,
  mouse/keyboard, formats)
- [cgx_2.23/CHANGES](cgx_2.23/CHANGES), [cgx_2.23/README](cgx_2.23/README),
  [cgx_2.23/INSTALL](cgx_2.23/INSTALL) — upstream CalculiX notes (still valid
  for the cgx command set and file formats)
- [cgx_2.23/src/build.sh](cgx_2.23/src/build.sh) — configure + build + check
  wrapper; survives renaming/moving the tree
- [cgx_2.23/src/cmake/RunCgxCheck.cmake](cgx_2.23/src/cmake/RunCgxCheck.cmake)
  — the build/install check script (headless smoke + `--glcheck`, §2)
- [cgx_2.23/tools/console_screencheck.py](cgx_2.23/tools/console_screencheck.py)
  — real-screen check of the console panel
- [cgx_2.23/examples/](cgx_2.23/examples/) — ready-to-run `.fbd` and `.frd`
  test cases used above

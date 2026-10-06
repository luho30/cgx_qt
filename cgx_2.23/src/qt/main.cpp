/* --------------------------------------------------------------------  */
/*                          CALCULIX                                     */
/*                   - GRAPHICAL INTERFACE -                             */
/*                                                                       */
/*     A 3-dimensional pre- and post-processor for finite elements       */
/*              Copyright (C) 1996 Klaus Wittig                          */
/*                                                                       */
/*     This program is free software; you can redistribute it and/or     */
/*     modify it under the terms of the GNU General Public License as    */
/*     published by the Free Software Foundation; version 2 of           */
/*     the License.                                                      */
/*                                                                       */
/*     This program is distributed in the hope that it will be useful,   */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of    */ 
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      */
/*     GNU General Public License for more details.                      */
/*                                                                       */
/*     You should have received a copy of the GNU General Public License */
/*     along with this program; if not, write to the Free Software       */
/*     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.         */
/* --------------------------------------------------------------------  */

// cgx — Qt application shell (Step 2).
//
// Dispatches before any GUI is created:
//   - `-bg` present  -> QCoreApplication (no display connection), runs the
//     legacy headless path (readfbd + exit) inside cgx_main().
//   - `--glcheck` present -> QApplication, verify that a desktop OpenGL
//     context can be created, print `GL-CHECK OK: …` and exit (no window,
//     no cgx_main()). Used by the CMake build/install check.
//   - otherwise      -> QApplication, then legacy interactive path
//     (GLUT windows + glutMainLoop) inside cgx_main().
//
// The original `main()` in cgx.c is renamed to `cgx_main()` at compile time
// via COMPILE_DEFINITIONS (see CMakeLists.txt), so this file owns the only
// real `main()`. glutMainLoop() still drives interactive mode in this step;
// the Qt event loop (app.exec()) takes over in Step 3.
#include <clocale>
#include <cstdio>
#include <cstring>

#include <QApplication>
#include <QCoreApplication>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "qt/ConsoleCapture.h"
#include "qt/glue.h"

extern "C" char frameFlag;

static bool has_bg_flag(int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "-bg") == 0)
      return true;
  return false;
}

static bool has_glcheck_flag(int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--glcheck") == 0)
      return true;
  return false;
}

// Legacy GL (display lists, GL_SELECT picking, immediate mode) needs a
// compatibility-profile context; QOpenGLWidget defaults may request core.
//
// The renderable type has to be desktop OpenGL explicitly. Left at
// DefaultRenderableType the Wayland QPA settles on an OpenGL-ES EGL config,
// so eglCreateContext for the desktop compat 2.1 format below fails with
// EGL_BAD_MATCH (3009); QOpenGLWidget then never gets a context, the Wayland
// surface never receives a buffer and NO window appears at all (X11 would
// still map a blank one). GLES is no alternative for cgx anyway: display
// lists and GL_SELECT do not exist there.
static void initGLFormat()
{
  QSurfaceFormat fmt;
  fmt.setRenderableType(QSurfaceFormat::OpenGL);
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
  fmt.setVersion(2, 1);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  fmt.setAlphaBufferSize(8);
  fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  QSurfaceFormat::setDefaultFormat(fmt);
}

// Exit code for "no usable GL context" (distinct from cgx's own codes) so a
// failed check is unambiguous for scripts.
static const int CGX_GL_FAILED = 3;

// Prove, before any window exists, that a desktop OpenGL context can be
// created from QSurfaceFormat::defaultFormat(). If it cannot, QOpenGLWidget
// never paints: on Wayland the surface receives no buffer and NO window
// shows up at all, i.e. the failure reads as "cgx runs but opens nothing".
// Called on every GUI start (silent on success) and by `cgx --glcheck`,
// which the CMake build/install check runs.
static int glCheck(bool reportOk)
{
  QOpenGLContext ctx;
  ctx.setFormat(QSurfaceFormat::defaultFormat());
  if (!ctx.create())
  {
    const QSurfaceFormat &req = QSurfaceFormat::defaultFormat();
    std::fprintf(stderr,
      "cgx: FATAL: cannot create an OpenGL context "
      "(requested renderableType=%d, %d.%d, profile=%d).\n"
      "cgx:   Without a context QOpenGLWidget never paints; on Wayland the surface\n"
      "cgx:   gets no buffer and NO window appears (Qt logs 'QEGLPlatformContext:\n"
      "cgx:   Failed to create context: 3009' = EGL_BAD_MATCH).\n"
      "cgx:   The format must request RenderableType OpenGL (initGLFormat in\n"
      "cgx:   src/qt/main.cpp); until rebuilt, run with QT_QPA_PLATFORM=xcb.\n",
      int(req.renderableType()), req.majorVersion(), req.minorVersion(),
      int(req.profile()));
    return CGX_GL_FAILED;
  }
  if (ctx.isOpenGLES())
  {
    std::fprintf(stderr,
      "cgx: FATAL: the platform handed out an OpenGL ES context; cgx needs desktop\n"
      "cgx:   OpenGL (display lists and GL_SELECT do not exist in GLES).\n");
    return CGX_GL_FAILED;
  }
  if (reportOk)
  {
    const QSurfaceFormat &got = ctx.format();
    std::printf("GL-CHECK OK: desktop OpenGL %d.%d profile=%d renderableType=%d\n",
                got.majorVersion(), got.minorVersion(), int(got.profile()),
                int(got.renderableType()));
  }
  return 0;
}

int main(int argc, char *argv[])
{
  if (has_bg_flag(argc, argv))
  {
    QCoreApplication app(argc, argv);
    // Qt initializes LC_ALL from environment, which on European locales sets
    // LC_NUMERIC to comma (','). cgx relies on standard C '.' for atof/sscanf/
    // sprintf. Restore standard numeric locale so floating-point coordinates
    // parse correctly.
    std::setlocale(LC_NUMERIC, "C");
    return cgx_main(argc, argv);
  }
  initGLFormat();
  QApplication app(argc, argv);
  // Restore numeric locale for GUI run too.
  std::setlocale(LC_NUMERIC, "C");
  // `--glcheck`: verify the context, report, exit — no windows, no cgx_main.
  if (has_glcheck_flag(argc, argv))
    return glCheck(true);
  // Same probe on a normal start: a broken format must abort with an
  // explanation instead of silently showing no window.
  if (glCheck(false) != 0)
    return CGX_GL_FAILED;
  // The black frame line loop around the drawing area in iniDrawMenu()
  // was an artifact of the legacy GLUT w0/w1 subwindow layout. In Qt,
  // GraphicsView is an independent layout-managed widget, making the
  // frame obsolete. Disable it by default while keeping the legend, text,
  // and color scala intact.
  frameFlag = 0;
  // Step 11: capture stdout for the in-window console panel (tee: the
  // terminal / log file still receives everything). GUI mode only.
  cgxconsole::install();
  int ret = cgx_main(argc, argv);
  // Step 3: cgx_main() sets up Qt windows and returns (no glutMainLoop);
  // the Qt event loop drives the application from here. Fall through to
  // exec() only if windows were created; otherwise propagate the exit code
  // (e.g. arg-parsing errors that exit before GUI setup).
  if (cgxQtWindowsAlive())
  {
    cgxMaybeStartSelftest();
    return app.exec();
  }
  return ret;
}

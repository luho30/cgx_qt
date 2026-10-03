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
//   - otherwise      -> QApplication, then legacy interactive path
//     (GLUT windows + glutMainLoop) inside cgx_main().
//
// The original `main()` in cgx.c is renamed to `cgx_main()` at compile time
// via COMPILE_DEFINITIONS (see CMakeLists.txt), so this file owns the only
// real `main()`. glutMainLoop() still drives interactive mode in this step;
// the Qt event loop (app.exec()) takes over in Step 3.
#include <clocale>
#include <cstring>

#include <QApplication>
#include <QCoreApplication>
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

// Legacy GL (display lists, GL_SELECT picking, immediate mode) needs a
// compatibility-profile context; QOpenGLWidget defaults may request core.
static void initGLFormat()
{
  QSurfaceFormat fmt;
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
  fmt.setVersion(2, 1);
  fmt.setDepthBufferSize(24);
  fmt.setStencilBufferSize(8);
  fmt.setAlphaBufferSize(8);
  fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  QSurfaceFormat::setDefaultFormat(fmt);
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

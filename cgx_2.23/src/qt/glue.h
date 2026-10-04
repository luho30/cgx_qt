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

#pragma once
// Step 3: GLUTinnov compatibility shims. Legacy C code keeps calling
// glut* names; under CGX_QT (see qt_shim.h) the window/display/state calls
// redirect here. Menus, bitmap fonts, colors and glutInit* stay on real
// vendored GLUT until Steps 4-6. Input-callback registrations are parked
// as no-ops until Step 4 wires Qt events.
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CgxDisplayFunc)(void);

/* window management */
int cgxCreateWindow(const char *title);
int cgxCreateSubWindow(int parent, int x, int y, int w, int h);
void cgxDestroyWindow(int win);
void cgxSetWindow(int win);
int cgxGetWindow(void);

/* display / buffer control */
void cgxPostRedisplay(void);
void cgxPostWindowRedisplay(int win);
void cgxSwapBuffers(void);
void cgxDisplayFunc(CgxDisplayFunc f);
void cgxClearColor(float r, float g, float b, float a);

/* state queries and window geometry */
int cgxGet(int type);
void cgxReshapeWindow(int w, int h);
void cgxPositionWindow(int x, int y);
void cgxSetWindowTitle(const char *title);
void cgxSetIconTitle(const char *title);

/* event loop */
void cgxIdleFunc(void (*f)(void));
void cgxMainLoop(void);
void cgxAttachMenu(int button);
void cgxDetachMenu(int button);

/* invoked from QOpenGLWidget::paintGL */
void cgxCallMenuDisplay(void);
void cgxCallGraphicsDisplay(void);
void cgxCallAxesDisplay(void);

/* ---- Step 4: input ---- */
typedef void (*CgxMouseFunc)(int button, int state, int x, int y);
typedef void (*CgxMotionFunc)(int x, int y);
typedef void (*CgxKeyFunc)(unsigned char key, int x, int y);
typedef void (*CgxSpecialFunc)(int key, int x, int y);

/* callback registrations (stored per legacy window id, GLUT semantics) */
void cgxMouseFunc(CgxMouseFunc f);
void cgxMotionFunc(CgxMotionFunc f);
void cgxPassiveMotionFunc(CgxMotionFunc f);
void cgxKeyboardFunc(CgxKeyFunc f);
void cgxSpecialFunc(CgxSpecialFunc f);

/* 1 while a non-Keyboard key handler (pick/defineDiv/defineValue) owns the
   keyboard of the 3D view (w1), i.e. a selection is in progress. Set/cleared
   by the cgxKeyboardFunc swap; also drives the command-line focus hand-off
   (CgxMainWindow::setKeyCapture). */
int cgxKeyFuncIsModal(void);

/* legacy window ids for Qt views (w0 menu strip, w1 graphics) */
int cgxMenuId(void);
int cgxGraphicsId(void);

/* ---- Step 5: menu mirror (replaces GLUT popup menus) ---- */
typedef void (*CgxMenuCallback)(int selection);

int cgxCreateMenu(CgxMenuCallback callback);
void cgxDestroyMenu(int menu);
int cgxGetMenu(void);
void cgxSetMenu(int menu);
void cgxAddMenuEntry(const char *label, int value);
void cgxAddSubMenu(const char *label, int submenu);
void cgxRemoveMenuItem(int item);
void cgxChangeToMenuEntry(int item, const char *label, int value);
void cgxChangeToSubMenu(int item, const char *label, int submenu);

/* popup the menu attached to win at a global screen position */
void cgxPopupMenuFor(int win, int globalX, int globalY);

/* invoke the legacy "Toggle CommandLine" menu entry (menu(5) in cgx.c): the
   single toggle path shared by the menubar entry, `view cl` and the Alt+C
   shortcut. Shows/hides the dock + console panel via the w3 create/destroy
   shims; no new toggle logic lives here. */
void cgxToggleCommandLine(void);

/* ---- Step 12: Qt-native hardcopy (see qt/Hardcopy.cpp) ----
   cgx.h maps SaveTGAScreenShot/getTGAScreenShot/createHardcopy here under
   CGX_QT: screen capture via grabFramebuffer() + QPainter composition,
   formats written directly (PNG / TGA / raster-EPS / GIF via ffmpeg) with
   no external ImageMagick tools and no unconditional file waits. */
void cgxSaveTGAScreenShot(char *filename, int w, int h);
void cgxGetTGAScreenShot(int nr);
void cgxCreateHardcopy(int selection, char *filePtr);

/* ---- Step 7: self-test entry (env CGX_QT_SELFTEST=1, see selftest.cpp) ---- */
void cgxMaybeStartSelftest(void);

/* ---- Step 6: bitmap text (replaces vendored GLUT bitmap fonts) ---- */
/* font handles are the (void*)0..5 tokens from GLUT_FONT under CGX_QT */
void cgxBitmapCharacter(void *font, int character);
int cgxBitmapWidth(void *font, int character);

/* event forwarding from Qt views; win selects the handler + activWindow */
void cgxForwardMouse(int win, int button, int state, int x, int y);
void cgxForwardMotion(int win, int x, int y, int passive);
void cgxForwardKey(int win, unsigned char key, int x, int y);
void cgxForwardSpecial(int win, int key, int x, int y);

/* 1 once the Qt main window exists (lets main() decide app.exec vs exit) */
int cgxQtWindowsAlive(void);

/* legacy entry points owned by cgx.c (C linkage) */
int cgx_main(int argc, char *argv[]);
void cgxReshape(int width, int height);

#ifdef __cplusplus
} /* extern "C" */
#endif

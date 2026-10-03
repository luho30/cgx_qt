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

// Step 3: GLUT window/display/event-loop shims (extern "C").
//
// Legacy C code calls these through the glut* names (see qt_shim.h).
// Only window/display/state/loop entry points are redirected; menus,
// bitmap fonts and colours are Qt-side (Steps 5-6, CgxFont.cpp).
#include "glue.h"
#include "CgxMainWindow.h"
#include "CgxViews.h"
#include "../glut_constants.h"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QString>
#include <QTimer>

#include <map>
#include <vector>

// Legacy globals owned by cgx.c.
extern int w0, w1, w2, w3;
extern int activWindow;
extern int width_w0, height_w0, width_w1, height_w1;
extern int width_ini, height_ini, width_menu, height_menu;
extern "C" double aspectRatio_w1;
extern "C" double backgrndcol_rgb[4];

namespace
{
CgxMainWindow *s_mainWin = nullptr;
int s_menuId = 0; // fake id of the top-level window (w0)
int s_nextId = 1; // fake subwindow ids start at 2
std::map<int, CgxDisplayFunc> s_displayFuncs;
void (*s_idleFunc)(void) = nullptr;
QTimer *s_idleTimer = nullptr;
QElapsedTimer s_elapsed;

// Which Qt widget's framebuffer the legacy GL code is currently drawing into.
// The overlays (legend = legacy w0, axes = legacy w2) must clear to fully
// transparent; the 3D view (legacy w1) must stay opaque. The target follows the
// *context that is actually current* (set in makeViewCurrent and in the
// cgxCall*Display paint wrappers), never activWindow guesswork.
enum PaintTarget
{
  TgtGraphics,
  TgtLegend,
  TgtAxes
};
PaintTarget s_target = TgtGraphics;
int s_paintDepth = 0; // >0 while inside a QOpenGLWidget::paintGL invocation

PaintTarget targetFor(int win)
{
  if (win == s_menuId)
    return TgtLegend;
  if (win != 0 && win == w2)
    return TgtAxes;
  return TgtGraphics;
}

QOpenGLWidget *viewForTarget(PaintTarget t)
{
  if (!s_mainWin)
    return nullptr;
  switch (t)
  {
  case TgtLegend:
    return s_mainWin->menuView();
  case TgtAxes:
    return s_mainWin->axesView();
  default:
    return s_mainWin->graphicsView();
  }
}

QOpenGLWidget *viewFor(int win)
{
  // w1 graphics plus the parked w3 id share the graphics context for safety
  // (DrawCommandLine needs *some* current context).
  return viewForTarget(targetFor(win));
}

// Paint routing: w2 owns the axes overlay since Step 7 (may be null before
// the w2 subwindow is created — then there is nothing to repaint).
QOpenGLWidget *viewForPaint(int win)
{
  if (!s_mainWin)
    return nullptr;
  if (win == s_menuId)
    return s_mainWin->menuView();
  if (win != 0 && win == w2)
    return s_mainWin->axesView();
  return s_mainWin->graphicsView();
}

void makeViewCurrent(int win)
{
  if (QOpenGLWidget *v = viewFor(win))
  {
    v->makeCurrent();
    s_target = targetFor(win);
  }
}

// Step-5 menu mirror model (populated by the cgxCreateMenu et al. shims;
// rendered as QMenu/QMenuBar on demand, so dynamic upstream rebuilds work).
struct CgxMenuItem
{
  QString label;
  bool isSubmenu = false;
  int value = 0; // entry: callback argument
  int submenuId = 0; // submenu: child menu id
};
struct CgxMenu
{
  CgxMenuCallback callback = nullptr;
  std::vector<CgxMenuItem> items;
};
std::map<int, CgxMenu> s_menus;
int s_currentMenu = 0;
int s_nextMenuId = 0;
bool s_menusDirty = true;
std::map<int, int> s_attachedMenu; // legacy window id -> menu id
inline void markMenusDirty()
{
  s_menusDirty = true;
}
void cgxSyncMenuBar();
} // namespace

int cgxCreateWindow(const char *title)
{
  if (!s_mainWin)
  {
    s_mainWin = new CgxMainWindow();
    if (title)
      s_mainWin->setWindowTitle(QString::fromUtf8(title));
    s_mainWin->show();
    // Size after show AND once more when the loop settles: pre-show resize
    // was observed to collapse to layout minimum on some WMs. Capture the
    // intended dims now (later reshape() calls overwrite the globals).
    const int initW = (width_w0 > 0) ? width_w0 : 800;
    const int initH = (height_w0 > 0) ? height_w0 : 600;
    s_mainWin->resize(initW, initH);
    QTimer::singleShot(0, [initW, initH]() {
      if (s_mainWin && (s_mainWin->width() < initW || s_mainWin->height() < initH))
        s_mainWin->resize(initW, initH);
    });
    // Keyboard input goes to the focused widget; legacy hotkeys live here.
    s_mainWin->graphicsView()->setFocus(Qt::ActiveWindowFocusReason);
    s_elapsed.start();
    // Legacy ids start at 1 like GLUT would: top-level is 1, first
    // subwindow (w1) is 2, matching tail assignments in cgx.c.
    s_menuId = 1;
    s_nextId = 1;
  }
  makeViewCurrent(s_menuId);
  return s_menuId;
}

int cgxCreateSubWindow(int parent, int /*x*/, int /*y*/, int /*w*/, int /*h*/)
{
  const int id = ++s_nextId;
  // Step 7: a second w0 child is the command-line window w3 (w1 is always
  // the first subwindow; w2 hangs off w1). Mirror its visibility in Qt.
  if (s_mainWin && parent == s_menuId && w1 != 0)
    s_mainWin->setCmdLineVisible(true);
  makeViewCurrent(id);
  return id;
}

void cgxDestroyWindow(int win)
{
  // Step 7: toggling the command line off destroys w3 — hide the dock.
  if (s_mainWin && win != 0 && win == w3)
    s_mainWin->setCmdLineVisible(false);
}

void cgxSetWindow(int win)
{
  activWindow = win;
  makeViewCurrent(win);
}

int cgxGetWindow(void)
{
  return activWindow;
}

void cgxPostRedisplay(void)
{
  cgxSyncMenuBar(); // rebuild menubar if menu trees changed (cheap flag check)
  if (QOpenGLWidget *v = viewForPaint(activWindow))
    v->update();
}

void cgxPostWindowRedisplay(int win)
{
  cgxSyncMenuBar();
  if (QOpenGLWidget *v = viewForPaint(win))
    v->update();
}

void cgxSwapBuffers(void)
{
  // QOpenGLWidget swaps by itself after paintGL. When legacy C code draws
  // directly outside a paint (e.g. DrawMenuLoad right after a dataset
  // switch), schedule one repaint of the widget whose buffer was drawn into.
  // Never inside a paint: that would re-trigger itself.
  if (s_paintDepth == 0)
    if (QOpenGLWidget *v = viewForTarget(s_target))
      v->update();
}

void cgxClearColor(float r, float g, float b, float a)
{
  // Overlays are cleared to premultiplied transparent black (Qt composites
  // QOpenGLWidgets with premultiplied alpha, so transparent pixels must be
  // RGB=0 as well). The 3D view keeps the legacy colour but is forced opaque:
  // the legacy "black background" toggle sets alpha to 0 as well.
  if (s_target == TgtLegend || s_target == TgtAxes)
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  else
    glClearColor(r, g, b, 1.0f);
}

void cgxDisplayFunc(CgxDisplayFunc f)
{
  s_displayFuncs[activWindow] = f;
}

int cgxGet(int type)
{
  switch (type)
  {
  case GLUT_WINDOW_X:
  case GLUT_WINDOW_Y:
  case GLUT_INIT_WINDOW_X:
  case GLUT_INIT_WINDOW_Y:
    return 0;
  case GLUT_WINDOW_WIDTH:
    return (activWindow == s_menuId) ? width_w0 : width_w1;
  case GLUT_WINDOW_HEIGHT:
    return (activWindow == s_menuId) ? height_w0 : height_w1;
  case GLUT_INIT_WINDOW_WIDTH:
    return width_w0;
  case GLUT_INIT_WINDOW_HEIGHT:
    return height_w0;
  case GLUT_SCREEN_WIDTH:
    if (QScreen *s = QGuiApplication::primaryScreen())
      return s->size().width();
    return 0;
  case GLUT_SCREEN_HEIGHT:
    if (QScreen *s = QGuiApplication::primaryScreen())
      return s->size().height();
    return 0;
  case GLUT_ELAPSED_TIME:
    return (int)s_elapsed.elapsed();
  case GLUT_MENU_NUM_ITEMS:
  {
    // Step 5: menus live in the mirror model; real GLUT knows nothing.
    auto it = s_menus.find(s_currentMenu);
    return (it == s_menus.end()) ? 0 : (int)it->second.items.size();
  }
  default:
    // Step 6: no real GLUT left to forward to. Every type used by cgx
    // sources has an explicit case above; anything new prints loudly so
    // the replay notices it instead of silently returning garbage.
    fprintf(stderr, "CGX cgxGet: unhandled GLUT query %d\n", type);
    return 0;
  }
}

void cgxReshapeWindow(int w, int h)
{
  // Only the top-level window maps to Qt geometry; subwindow geometry is
  // owned by the Qt layout until Steps 5/7 replace it.
  if (s_mainWin && activWindow == s_menuId)
    s_mainWin->resize(w, h);
}

void cgxPositionWindow(int /*x*/, int /*y*/)
{
}

void cgxSetWindowTitle(const char *title)
{
  if (s_mainWin && title)
    s_mainWin->setWindowTitle(QString::fromUtf8(title));
}

void cgxSetIconTitle(const char * /*title*/)
{
}

void cgxIdleFunc(void (*f)(void))
{
  s_idleFunc = f;
  if (!s_mainWin)
    return;
  if (f)
  {
    if (!s_idleTimer)
    {
      s_idleTimer = new QTimer(s_mainWin);
      QObject::connect(s_idleTimer, &QTimer::timeout, []() {
        if (s_idleFunc)
          s_idleFunc();
        else if (s_idleTimer)
          s_idleTimer->stop();
      });
    }
    if (!s_idleTimer->isActive())
      s_idleTimer->start(16); // ~60 Hz idle pump, replaces glutIdleFunc
  }
  else if (s_idleTimer)
    s_idleTimer->stop();
}

void cgxMainLoop(void)
{
  // No-op: control returns to qt/main.cpp, which runs app.exec().
  // The idle pump (above) keeps legacy idleFunction() alive.
}

void cgxAttachMenu(int /*button*/)
{
  // Legacy attaches the current menu to activWindow (w0, LEFT button).
  // Immediately sync the QMenuBar in the window header so it appears
  // synchronously upon creation across all startup modes.
  s_attachedMenu[activWindow] = s_currentMenu;
  markMenusDirty();
  cgxSyncMenuBar();
}

void cgxDetachMenu(int /*button*/)
{
  s_attachedMenu.erase(activWindow);
}

/* ---- Step 5: menu mirror shims (model declared in the top block) ---- */

int cgxCreateMenu(CgxMenuCallback callback)
{
  const int id = ++s_nextMenuId;
  s_menus[id].callback = callback;
  // GLUT parity: a newly created menu becomes the current menu, so the
  // following glutAddMenuEntry/AddSubMenu calls land in it.
  s_currentMenu = id;
  markMenusDirty();
  return id;
}

void cgxDestroyMenu(int menu)
{
  s_menus.erase(menu);
  if (s_currentMenu == menu)
    s_currentMenu = 0;
  markMenusDirty();
}

int cgxGetMenu(void)
{
  return s_currentMenu;
}

void cgxSetMenu(int menu)
{
  s_currentMenu = menu;
}

void cgxAddMenuEntry(const char *label, int value)
{
  auto it = s_menus.find(s_currentMenu);
  if (it == s_menus.end() || !label)
    return;
  CgxMenuItem item;
  item.label = QString::fromUtf8(label);
  item.value = value;
  it->second.items.push_back(item);
  markMenusDirty();
}

void cgxAddSubMenu(const char *label, int submenu)
{
  auto it = s_menus.find(s_currentMenu);
  if (it == s_menus.end() || !label)
    return;
  CgxMenuItem item;
  item.label = QString::fromUtf8(label);
  item.isSubmenu = true;
  item.submenuId = submenu;
  it->second.items.push_back(item);
  markMenusDirty();
}

void cgxRemoveMenuItem(int item)
{
  // Legacy indices are 1-based.
  auto it = s_menus.find(s_currentMenu);
  if (it == s_menus.end() || item < 1 || item > (int)it->second.items.size())
    return;
  it->second.items.erase(it->second.items.begin() + (item - 1));
  markMenusDirty();
}

void cgxChangeToMenuEntry(int item, const char *label, int value)
{
  auto it = s_menus.find(s_currentMenu);
  if (it == s_menus.end() || !label || item < 1 || item > (int)it->second.items.size())
    return;
  CgxMenuItem &dst = it->second.items[item - 1];
  dst.label = QString::fromUtf8(label);
  dst.isSubmenu = false;
  dst.value = value;
  markMenusDirty();
}

void cgxChangeToSubMenu(int item, const char *label, int submenu)
{
  auto it = s_menus.find(s_currentMenu);
  if (it == s_menus.end() || !label || item < 1 || item > (int)it->second.items.size())
    return;
  CgxMenuItem &dst = it->second.items[item - 1];
  dst.label = QString::fromUtf8(label);
  dst.isSubmenu = true;
  dst.submenuId = submenu;
  markMenusDirty();
}

namespace
{
QMenu *buildQtMenu(int menuId, QWidget *parent)
{
  QMenu *m = new QMenu(parent);
  auto it = s_menus.find(menuId);
  if (it == s_menus.end())
    return m;
  CgxMenuCallback cb = it->second.callback;
  for (const CgxMenuItem &item : it->second.items)
  {
    if (item.isSubmenu)
    {
      QMenu *sub = buildQtMenu(item.submenuId, m);
      sub->setTitle(item.label);
      m->addMenu(sub);
    }
    else
    {
      QAction *a = m->addAction(item.label);
      const int value = item.value;
      QObject::connect(a, &QAction::triggered, [cb, value]() {
        if (cb)
          cb(value);
        cgxSyncMenuBar();
      });
    }
  }
  return m;
}

void cgxRebuildMenuBar()
{
  if (!s_mainWin)
    return;
  QMenuBar *bar = s_mainWin->menuBar();
  bar->clear();
  auto it = s_attachedMenu.find(s_menuId);
  if (it == s_attachedMenu.end())
    return;
  auto mit = s_menus.find(it->second);
  if (mit == s_menus.end())
    return;
  CgxMenuCallback cb = mit->second.callback;
  for (const CgxMenuItem &item : mit->second.items)
  {
    if (item.isSubmenu)
    {
      QMenu *sub = buildQtMenu(item.submenuId, bar);
      sub->setTitle(item.label.trimmed());
      bar->addMenu(sub);
    }
    else
    {
      QAction *a = bar->addAction(item.label.trimmed());
      const int value = item.value;
      QObject::connect(a, &QAction::triggered, [cb, value]() {
        if (cb)
          cb(value);
        cgxSyncMenuBar();
      });
    }
  }
  s_menusDirty = false;
}

void cgxSyncMenuBar()
{
  if (s_menusDirty)
    cgxRebuildMenuBar();
}
} // namespace

void cgxPopupMenuFor(int win, int globalX, int globalY)
{
  auto it = s_attachedMenu.find(win);
  if (it == s_attachedMenu.end() || !s_mainWin)
    return;
  QMenu *m = buildQtMenu(it->second, s_mainWin);
  m->exec(QPoint(globalX, globalY));
  delete m;
  cgxRebuildMenuBar(); // callbacks may have rebuilt the menu trees
}

extern "C" {
void reshape(int width, int height);
void menu(int selection); // legacy main-menu callback in cgx.c (case 5 = command line)
} // extern "C"

void cgxToggleCommandLine(void)
{
  menu(5);
}

int cgxQtWindowsAlive(void)
{
  return s_mainWin ? 1 : 0;
}

CgxMainWindow *cgxMainWindow()
{
  return s_mainWin;
}

int cgxMenuId(void)
{
  return s_menuId;
}

int cgxGraphicsId(void)
{
  return w1;
}

/* ---- Step 4: input handler storage (per legacy window id) ---- */
namespace
{
std::map<int, CgxMouseFunc> s_mouseFuncs;
std::map<int, CgxMotionFunc> s_motionFuncs;
std::map<int, CgxMotionFunc> s_passiveFuncs;
std::map<int, CgxKeyFunc> s_keyFuncs;
std::map<int, CgxSpecialFunc> s_specialFuncs;
} // namespace

void cgxMouseFunc(CgxMouseFunc f)
{
  s_mouseFuncs[activWindow] = f;
}

void cgxMotionFunc(CgxMotionFunc f)
{
  s_motionFuncs[activWindow] = f;
}

void cgxPassiveMotionFunc(CgxMotionFunc f)
{
  s_passiveFuncs[activWindow] = f;
}

void cgxKeyboardFunc(CgxKeyFunc f)
{
  // Preserves the pickFunktions.c pattern of swapping Keyboard() against
  // pick()/defineDiv()/defineValue() per window — no call-site edits needed.
  s_keyFuncs[activWindow] = f;
}

void cgxSpecialFunc(CgxSpecialFunc f)
{
  s_specialFuncs[activWindow] = f;
}

void cgxForwardMouse(int win, int button, int state, int x, int y)
{
  cgxSetWindow(win);
  auto it = s_mouseFuncs.find(win);
  if (it != s_mouseFuncs.end() && it->second)
    it->second(button, state, x, y);
}

void cgxForwardMotion(int win, int x, int y, int passive)
{
  cgxSetWindow(win);
  const auto &map = passive ? s_passiveFuncs : s_motionFuncs;
  auto it = map.find(win);
  if (it != map.end() && it->second)
    it->second(x, y);
}

void cgxForwardKey(int win, unsigned char key, int x, int y)
{
  cgxSetWindow(win);
  auto it = s_keyFuncs.find(win);
  if (it != s_keyFuncs.end() && it->second)
    it->second(key, x, y);
}

void cgxForwardSpecial(int win, int key, int x, int y)
{
  cgxSetWindow(win);
  auto it = s_specialFuncs.find(win);
  if (it != s_specialFuncs.end() && it->second)
    it->second(key, x, y);
}

void cgxReshape(int width, int height)
{
  reshape(width, height);
  // Full-window 3D viewport: GraphicsView fills the entire window, so
  // w1's dimensions and aspect ratio match the full view:
  if (s_mainWin && s_mainWin->graphicsView())
  {
    width_w1 = s_mainWin->graphicsView()->width();
    height_w1 = s_mainWin->graphicsView()->height();
    if (height_w1 > 0)
      aspectRatio_w1 = (double)width_w1 / (double)height_w1;
  }
}

namespace
{
// Runs a legacy display callback inside a QOpenGLWidget::paintGL. Qt has
// already made the right widget's context/FBO current, so do NOT call
// cgxSetWindow here (viewFor(w2) used to resolve to the 3D view and the axes
// clear then wiped the 3D framebuffer).
void runPaint(PaintTarget t, CgxDisplayFunc f)
{
  const PaintTarget saved = s_target;
  s_target = t;
  ++s_paintDepth;
  f();
  --s_paintDepth;
  s_target = saved;
}
} // namespace

void cgxCallMenuDisplay(void)
{
  auto it = s_displayFuncs.find(s_menuId);
  if (it != s_displayFuncs.end() && it->second)
    runPaint(TgtLegend, it->second);
}

void cgxCallAxesDisplay(void)
{
  // Step 7: w2's callback (DrawAxes). Absent until the w2 subwindow is
  // created in the tail setup; then repaints follow it automatically.
  auto it = s_displayFuncs.find(w2);
  if (it != s_displayFuncs.end() && it->second)
    runPaint(TgtAxes, it->second);
}

void cgxCallGraphicsDisplay(void)
{
  // w1 is the first subwindow id handed out (== 2); fall back to any
  // stored non-menu func so early paints still draw something sane.
  auto it = s_displayFuncs.find(2);
  if (it != s_displayFuncs.end() && it->second)
  {
    runPaint(TgtGraphics, it->second);
    return;
  }
  for (const auto &kv : s_displayFuncs)
  {
    if (kv.first != s_menuId && kv.second)
    {
      runPaint(TgtGraphics, kv.second);
      return;
    }
  }
}

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

// In-app end-to-end input test, gated by env CGX_QT_SELFTEST=1.
//
// Compiled only with -DCGX_QT_TESTHOOKS (CMake option CGX_QT_TESTHOOKS, OFF
// by default): a normal build contains none of the CGX_QT_* environment
// hooks (selftest, scripted startup keys) — see the stub at the end.
//
// Why this exists: on Wayland sessions the compositor gates synthetic X11
// input (xdotool) behind an approval dialog that cannot be clicked
// headlessly, so driven GUI tests are impossible from the outside. This
// harness delivers real QMouseEvent/QKeyEvent/QWheelEvent objects straight
// into the widgets (same code path as X-server events below Qt) and captures
// framebuffer screenshots, giving a scriptable, portal-proof regression
// test that also serves future replays (see QT-PORT.md §7).
//
// Coverage: left-drag rotate, wheel zoom, ASCII keys, Return exec,
// `cl` command-line toggle (dock show), QLineEdit exec, Up-history recall,
// toggle-off (dock hide). Markers go to stderr; command output (help text)
// goes to stdout for shell-side grep. Exits 0 on SELFTEST-DONE.
#ifdef CGX_QT_TESTHOOKS
#include "CgxMainWindow.h"
#include "CgxViews.h"
#include "ConsoleCapture.h"
#include "ConsoleView.h"
#include "DisplaySets.h"
#include "DisplaySetsBridge.h"
#include "glue.h"

#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QByteArray>
#include <QTemporaryDir>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPixmap>
#include <QScreen>
#include <QScrollBar>
#include <QTableWidget>
#include <QStringList>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" char iniActionsFlag;
extern "C" char commandLineFlag;
extern "C" int w3;
extern "C" char *keystroke;
extern "C" char pickFlag; // cgx.c: 1 while pick() owns the keyboard

namespace
{
// Screenshot directory: CGX_QT_SHOT_DIR if set, otherwise a private
// QTemporaryDir (mode 0700, removed at exit) — never a fixed /tmp path.
const char *shotDir()
{
  static QByteArray dir;
  static QTemporaryDir tmp;
  if (dir.isEmpty())
  {
    if (const char *e = std::getenv("CGX_QT_SHOT_DIR"))
      dir = e;
    else
      dir = tmp.path().toLocal8Bit();
    std::fprintf(stderr, "SELFTEST shotdir=%s\n", dir.constData());
  }
  return dir.constData();
}
int s_waits = 0;
int s_settle = 0;
int s_failures = 0;
int s_viewWPre = -1; // 3D view size before `view cl` (Step 13 no-squeeze)
int s_viewHPre = -1;

CgxMainWindow *MW()
{
  return cgxMainWindow();
}

GraphicsView *GV()
{
  CgxMainWindow *m = MW();
  return m ? m->graphicsView() : nullptr;
}

QLineEdit *CL()
{
  CgxMainWindow *m = MW();
  return m ? m->cmdLine() : nullptr;
}

void shot(const char *name)
{
  if (GraphicsView *g = GV())
  {
    char path[256];
    std::snprintf(path, sizeof(path), "%s/s7t_%s.png", shotDir(), name);
    g->grabFramebuffer().save(QString::fromUtf8(path));
    std::fprintf(stderr, "SELFTEST shot=%s\n", path);
  }
}

void fullshot(const char *name)
{
  if (CgxMainWindow *m = MW())
  {
    char path[256];
    std::snprintf(path, sizeof(path), "%s/s7t_%s.png", shotDir(), name);
    m->grab().save(QString::fromUtf8(path));
    std::fprintf(stderr, "SELFTEST fullshot=%s\n", path);
  }
}

void menuShot(const char *name)
{
  if (CgxMainWindow *m = MW())
  {
    if (m->menuView())
    {
      char path[256];
      std::snprintf(path, sizeof(path), "%s/s7t_menu_%s.png", shotDir(), name);
      m->menuView()->grabFramebuffer().save(QString::fromUtf8(path));
      std::fprintf(stderr, "SELFTEST menu-shot=%s\n", path);
    }
  }
}

void axesShot(const char *name)
{
  CgxMainWindow *m = MW();
  if (!m || !m->axesView())
  {
    std::fprintf(stderr, "SELFTEST axes-missing\n");
    return;
  }
  QImage img = m->axesView()->grabFramebuffer();
  char path[256];
  std::snprintf(path, sizeof(path), "%s/s7t_%s.png", shotDir(), name);
  img.save(QString::fromUtf8(path));
  // The axes overlay is cleared fully transparent: the tripod is the set of
  // pixels with alpha > 0. Expect a small, non-zero fraction.
  int dark = 0;
  const QImage argb = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const int total = argb.width() * argb.height();
  for (int y = 0; y < argb.height(); ++y)
  {
    const QRgb *scan = (const QRgb *)argb.constScanLine(y);
    for (int x = 0; x < argb.width(); ++x)
      if (qAlpha(scan[x]) > 0)
        ++dark;
  }
  const double frac = total ? (double)dark / total : 0.0;
  std::fprintf(stderr, "SELFTEST axes-shot=%s size=%dx%d darkfrac=%.3f\n", path, img.width(),
               img.height(), frac);
  if (frac < 0.002 || frac > 0.5)
  {
    std::fprintf(stderr, "SELFTEST-FAIL axes tripod coverage out of range\n");
    ++s_failures;
  }
}

// Compares the composited top-level window (QWidget::grab) against the 3D view's own framebuffer (the opaque
// scene without any overlay). Guards against the failure the old checks
// missed: widget framebuffers fine, composited window black.
//  - 3D region outside the overlays must equal the 3D framebuffer (opaque,
//    full window, not black, not transparent).
//  - legend/axes regions must differ only where they draw text/bars/lines;
//    their background must show the 3D scene through (100% transparent).
void screenCheck(const char *name)
{
  CgxMainWindow *m = MW();
  GraphicsView *g = GV();
  if (!m || !g)
  {
    std::fprintf(stderr, "SELFTEST-FAIL screen-check %s: no window\n", name);
    ++s_failures;
    return;
  }
  // NOTE: QScreen::grabWindow() returns solid black for GL-composited windows
  // here (X-level grab), so it cannot be used. QWidget::grab() composites the
  // QOpenGLWidgets through Qt (same blending as on screen) and is reliable.
  const QImage real = m->grab().toImage().convertToFormat(QImage::Format_RGB32);
  const QImage fb = g->grabFramebuffer().convertToFormat(QImage::Format_RGB32);
  char path[256];
  std::snprintf(path, sizeof(path), "%s/s7t_real_%s.png", shotDir(), name);
  real.save(QString::fromUtf8(path));

  const QPoint o = g->mapTo(m, QPoint(0, 0)); // 3D view origin inside the window
  const int W = std::min(fb.width(), real.width() - o.x());
  const int H = std::min(fb.height(), real.height() - o.y());
  QRect legend(0, 0, 184, H);
  if (m->menuView())
    legend = QRect(0, 0, std::min(m->menuView()->width(), W), H);
  QRect axes;
  if (m->axesView())
    axes = m->axesView()->geometry();
  QRect consoleRect; // excluded: it is meant to tint what is below it
  if (m->consoleView() && m->consoleView()->isVisible())
    consoleRect = m->consoleView()->geometry();
  QRect clRect; // Step 13: the opaque command line overlays the 3D view
  if (m->cmdLine() && m->cmdLine()->isVisible())
    clRect = m->cmdLine()->geometry();
  auto differs = [&](int x, int y) {
    const QRgb a = real.pixel(o.x() + x, o.y() + y);
    const QRgb b = fb.pixel(x, y);
    return std::abs(qRed(a) - qRed(b)) > 12 || std::abs(qGreen(a) - qGreen(b)) > 12 ||
           std::abs(qBlue(a) - qBlue(b)) > 12;
  };
  long black = 0, total3d = 0, restDiff = 0, restN = 0, legDiff = 0, legN = 0, axDiff = 0, axN = 0;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x)
    {
      if (consoleRect.isValid() && consoleRect.contains(x, y))
        continue;
      if (clRect.isValid() && clRect.contains(x, y))
        continue; // the command line deliberately covers what is below it
      const bool inLegend = legend.contains(x, y);
      const bool inAxes = axes.isValid() && axes.contains(x, y);
      if (qRgb(0, 0, 0) == (real.pixel(o.x() + x, o.y() + y) | 0xff000000))
        ++black;
      ++total3d;
      if (inLegend)
      {
        ++legN;
        legDiff += differs(x, y);
      }
      else if (inAxes)
      {
        ++axN;
        axDiff += differs(x, y);
      }
      else
      {
        ++restN;
        restDiff += differs(x, y);
      }
    }
  const double blackFrac = total3d ? (double)black / total3d : 1.0;
  const double restFrac = restN ? (double)restDiff / restN : 0.0;
  const double legFrac = legN ? (double)legDiff / legN : 0.0;
  const double axFrac = axN ? (double)axDiff / axN : 0.0;
  // Axes box check: the four inset corners of the axes widget must show the
  // 3D scene (no opaque/black square around the tripod).
  int cornerBad = 0;
  if (axes.isValid())
  {
    const int pts[4][2] = {{axes.left() + 2, axes.top() + 2},
                           {axes.right() - 2, axes.top() + 2},
                           {axes.left() + 2, axes.bottom() - 2},
                           {axes.right() - 2, axes.bottom() - 2}};
    for (const auto &c : pts)
    {
      if (consoleRect.isValid() && consoleRect.contains(c[0], c[1]))
        continue; // the console deliberately tints what is below it
      if (clRect.isValid() && clRect.contains(c[0], c[1]))
        continue; // ditto the command line band
      if (c[0] >= 0 && c[1] >= 0 && c[0] < W && c[1] < H && differs(c[0], c[1]))
        ++cornerBad;
    }
  }
  std::fprintf(stderr,
               "SELFTEST screen-check %s: 3dBlack=%.3f outsideOverlayDiff=%.4f legendDiff=%.3f "
               "axesDiff=%.3f axesCornerDiff=%d shot=%s\n",
               name, blackFrac, restFrac, legFrac, axFrac, cornerBad, path);
  if (blackFrac > 0.5)
  {
    std::fprintf(stderr, "SELFTEST-FAIL %s: 3D view is black on screen\n", name);
    ++s_failures;
  }
  if (restFrac > 0.02)
  {
    std::fprintf(stderr, "SELFTEST-FAIL %s: 3D view on screen differs from its framebuffer\n",
                 name);
    ++s_failures;
  }
  if (legFrac > 0.25)
  {
    std::fprintf(stderr, "SELFTEST-FAIL %s: legend background is not transparent\n", name);
    ++s_failures;
  }
  if (cornerBad)
  {
    std::fprintf(stderr, "SELFTEST-FAIL %s: axes widget has an opaque box\n", name);
    ++s_failures;
  }
}

// Console panel (Step 11) checks. `expectVisible` mirrors the command line.
void consoleCheck(const char *name, bool expectVisible, bool expectHelpText)
{
  CgxMainWindow *m = MW();
  ConsoleView *c = m ? m->consoleView() : nullptr;
  if (!c)
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: missing\n", name);
    ++s_failures;
    return;
  }
  const bool vis = c->isVisible();
  std::fprintf(stderr,
               "SELFTEST console %s: visible=%d height=%d (panelHeight=%d) width=%d windowW=%d "
               "visualLines=%d scroll=%d/%d white=%d captured=%llu\n",
               name, vis ? 1 : 0, c->height(), c->panelHeight(), c->width(), m->width(),
               c->visualLineCount(), c->scrollValue(), c->scrollMax(), c->isWhiteTheme() ? 1 : 0,
               cgxconsole::bytesCaptured());
  if (vis != expectVisible)
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: visibility %d, expected %d\n", name, vis,
                 expectVisible);
    ++s_failures;
  }
  if (!vis)
    return;
  if (c->height() != c->panelHeight())
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: height not 5 lines\n", name);
    ++s_failures;
  }
  if (c->width() < m->width() - 4) // full width of the window (central widget)
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: not full width\n", name);
    ++s_failures;
  }
  // bottom edge = directly above the command line
  QLineEdit *cl = CL();
  if (cl && cl->isVisible())
  {
    const int panelBottom = c->mapTo(m, QPoint(0, c->height())).y();
    const int clTop = cl->mapTo(m, QPoint(0, 0)).y();
    if (panelBottom != clTop)
    {
      std::fprintf(stderr, "SELFTEST-FAIL console %s: not directly above command line (%d vs %d)\n",
                   name, panelBottom, clTop);
      ++s_failures;
    }
  }
  const QString text = c->plainText();
  if (!text.contains(QStringLiteral("reading")))
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: startup output missing\n", name);
    ++s_failures;
  }
  if (expectHelpText && !text.contains(QStringLiteral("Quick help for the experienced user")))
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: help output missing\n", name);
    ++s_failures;
  }
  if (c->scrollMax() <= 0)
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: nothing to scroll\n", name);
    ++s_failures;
  }
  if (c->scrollValue() != c->scrollMax())
  {
    std::fprintf(stderr, "SELFTEST-FAIL console %s: not following the bottom\n", name);
    ++s_failures;
  }
}

// NOTE: the console-above-legend stacking order is verified on the REAL screen
// by tools/console_screencheck.py. QWidget::grab() does not reproduce the
// on-screen stacking of these GL widgets (a negative test passed with the
// legend above the console), so it must not be used for that.

// Selection / copy checks (Step 11 amendment). Data: see case 15.
void selectionChecks()
{
  CgxMainWindow *m = MW();
  ConsoleView *c = m ? m->consoleView() : nullptr;
  if (!c)
  {
    std::fprintf(stderr, "SELFTEST-FAIL selection: no console\n");
    ++s_failures;
    return;
  }
  QClipboard *cb = QGuiApplication::clipboard();
  auto fail = [&](const char *what) {
    std::fprintf(stderr, "SELFTEST-FAIL selection: %s\n", what);
    ++s_failures;
  };
  c->setScrollValue(c->scrollMax());
  const int L = c->visualLineCount() - 2; // the SELTEST line (last line is the live empty one)

  // 1. programmatic range inside one line
  c->setSelection(L, 8, L, 13);
  const QString part = c->selectedText();
  std::fprintf(stderr, "SELFTEST selection-range: [%s]\n", part.toUtf8().constData());
  if (part != QStringLiteral("alpha"))
    fail("range text wrong");

  // 2. select all == the whole logical text; wrapped long line pastes unbroken
  c->selectAll();
  const QString all = c->selectedText();
  const QString longRun = QStringLiteral("WRAPTEST ") + QString(300, QLatin1Char('a')) +
                          QStringLiteral(" END");
  const bool eq = (all == c->plainText());
  const bool unbroken = all.contains(longRun);
  std::fprintf(stderr, "SELFTEST selection-all: equalsPlainText=%d wrappedLineUnbroken=%d\n",
               eq ? 1 : 0, unbroken ? 1 : 0);
  if (!eq)
    fail("select-all differs from plainText");
  if (!unbroken)
    fail("wrapped line was split by the copy");

  // 3. clipboard paths
  c->setSelection(L, 8, L, 13);
  cb->clear();
  const bool copied = c->copySelection();
  if (!copied || cb->text() != QStringLiteral("alpha"))
    fail("copySelection did not fill the clipboard");
  cb->clear();
  c->copyAll();
  if (cb->text() != c->plainText())
    fail("copyAll mismatch");
  // Ctrl+C with a selection copies it; without one copies everything
  c->setSelection(L, 8, L, 13);
  cb->clear();
  {
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
    QApplication::sendEvent(c, &ev);
  }
  if (cb->text() != QStringLiteral("alpha"))
    fail("Ctrl+C with selection");
  c->clearSelection();
  cb->clear();
  {
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier, QStringLiteral("\x03"));
    QApplication::sendEvent(c, &ev);
  }
  if (cb->text() != c->plainText())
    fail("Ctrl+C without selection should copy all");
  std::fprintf(stderr, "SELFTEST selection-clipboard: copySelection/copyAll/Ctrl+C ok=%d\n",
               s_failures == 0 ? 1 : 0);

  // 4. mouse drag selects the same text (hit-testing == paint metrics)
  c->clearSelection();
  const QPoint a = c->cellPos(L, 8), b = c->cellPos(L, 13);
  QMouseEvent press(QEvent::MouseButtonPress, a, a, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(c, &press);
  QMouseEvent mv(QEvent::MouseMove, b, b, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(c, &mv);
  QMouseEvent rel(QEvent::MouseButtonRelease, b, b, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(c, &rel);
  const QString dragged = c->selectedText();
  std::fprintf(stderr, "SELFTEST selection-drag: [%s]\n", dragged.toUtf8().constData());
  if (dragged != QStringLiteral("alpha"))
    fail("drag selection text wrong");
  // dragging also fills the X11 primary selection
  if (cb->supportsSelection() && cb->text(QClipboard::Selection) != QStringLiteral("alpha"))
    fail("primary selection not filled after drag");
  // reverse drag (right to left) gives the same text
  c->clearSelection();
  QMouseEvent press2(QEvent::MouseButtonPress, b, b, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(c, &press2);
  QMouseEvent mv2(QEvent::MouseMove, a, a, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(c, &mv2);
  QMouseEvent rel2(QEvent::MouseButtonRelease, a, a, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(c, &rel2);
  if (c->selectedText() != QStringLiteral("alpha"))
    fail("reverse drag selection text wrong");

  // 5. double click = word, triple click = line
  c->clearSelection();
  const QPoint w = c->cellPos(L, 10); // inside "alpha"
  QMouseEvent dbl(QEvent::MouseButtonDblClick, w, w, Qt::LeftButton, Qt::LeftButton,
                  Qt::NoModifier);
  QApplication::sendEvent(c, &dbl);
  const QString word = c->selectedText();
  QMouseEvent tri(QEvent::MouseButtonPress, w, w, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(c, &tri);
  QMouseEvent trel(QEvent::MouseButtonRelease, w, w, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
  QApplication::sendEvent(c, &trel);
  const QString line = c->selectedText();
  std::fprintf(stderr, "SELFTEST selection-word/line: word=[%s] line=[%s]\n",
               word.toUtf8().constData(), line.toUtf8().constData());
  if (word != QStringLiteral("alpha"))
    fail("double click word wrong");
  if (line != QStringLiteral("SELTEST alpha beta gamma"))
    fail("triple click line wrong");

  // 6. new output clears the selection (stale indices)
  c->setSelection(L, 8, L, 13);
  c->appendChunk(QStringLiteral("more output\n"));
  if (c->hasSelection())
    fail("selection survived new output");
}

void next(int n, int ms = 500);

// Small ASCII -> Qt::Key mapper (letters/digits only; other bytes use 0,
// which safely falls through to the text-based ASCII path in CgxViews).
int qtKeyFor(char c)
{
  if (c >= 'a' && c <= 'z')
    return Qt::Key_A + (c - 'a');
  if (c >= 'A' && c <= 'Z')
    return Qt::Key_A + (c - 'A');
  if (c >= '0' && c <= '9')
    return Qt::Key_0 + (c - '0');
  return 0;
}

void sendKey(QObject *target, char c, const QPoint &pos)
{
  (void)pos;
  QString text = (c == '\r') ? QStringLiteral("\r") : QString(QChar::fromLatin1(c));
  int code = (c == '\r') ? Qt::Key_Return : qtKeyFor(c);
  QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier, text);
  QApplication::sendEvent(target, &press);
  QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier, text);
  QApplication::sendEvent(target, &release);
}

void sendText(QObject *target, const char *s)
{
  for (const char *p = s; *p; ++p)
    sendKey(target, *p, QPoint());
}

void sendSpecial(QObject *target, int qtKey)
{
  QKeyEvent press(QEvent::KeyPress, qtKey, Qt::NoModifier);
  QApplication::sendEvent(target, &press);
  QKeyEvent release(QEvent::KeyRelease, qtKey, Qt::NoModifier);
  QApplication::sendEvent(target, &release);
}


// Step 16: "Display Sets" menu. Creates 40 sets through the real command
// path, then drives the popup exactly like clicks do (clickCell) and checks
// the legacy pset state through the bridge.
void displaySetsChecks()
{
  CgxMainWindow *m = MW();
  GraphicsView *g = GV();
  if (!m || !g)
    return;
  auto fail = [](const char *what) {
    std::fprintf(stderr, "SELFTEST-FAIL displaysets: %s\n", what);
    ++s_failures;
  };

  // 1) menubar position: right after Viewing
  QStringList menus;
  for (QAction *act : m->menuBar()->actions())
    menus << act->text();
  const int vi = menus.indexOf(QStringLiteral("Viewing"));
  const int di = menus.indexOf(QStringLiteral("Display Sets"));
  std::fprintf(stderr, "SELFTEST displaysets-menubar viewing=%d displaysets=%d\n", vi, di);
  if (vi < 0 || di != vi + 1)
    fail("menu is not directly after Viewing");

  // 2) many sets -> scrollbar
  for (int i = 1; i <= 40; ++i)
  {
    char cmd[64];
    std::snprintf(cmd, sizeof cmd, "seta DSt%02d e all", i);
    sendText(g, cmd);
    sendKey(g, '\r', QPoint());
    std::snprintf(cmd, sizeof cmd, "seta DSt%02d n all", i);
    sendText(g, cmd);
    sendKey(g, '\r', QPoint());
  }
  DisplaySetsMenu *dm = cgxDisplaySetsMenu(m);
  dm->popup(m->mapToGlobal(QPoint(60, 40)));
  QApplication::processEvents();
  QTableWidget *tb = dm->table();
  std::fprintf(stderr, "SELFTEST displaysets-rows rows=%d tableH=%d scrollMax=%d winH=%d\n",
               tb->rowCount(), tb->height(), tb->verticalScrollBar()->maximum(), m->height());
  if (tb->rowCount() < 41) // 40 + all
    fail("expected >= 41 rows");
  if (tb->verticalScrollBar()->maximum() <= 0)
    fail("no vertical scroll range with 41 rows");
  if (tb->height() > m->height())
    fail("popup taller than the window");

  // 3) set checkbox on/off
  const int r = dm->rowForSet(QStringLiteral("DSt02"));
  if (r < 0)
  {
    fail("set DSt02 missing in list");
    dm->hide();
    return;
  }
  const int idx = tb->item(r, 0)->data(Qt::UserRole).toInt();
  const int eBit = 1 << 1, fBit = 1 << 0;
  dm->clickCell(r, 0);
  int mk = cgxDsDisplayedMask(idx);
  std::fprintf(stderr, "SELFTEST displaysets-on mask=%d checked=%d\n", mk,
               tb->item(r, 0)->checkState() == Qt::Checked);
  if (!(mk & eBit) || tb->item(r, 0)->checkState() != Qt::Checked)
    fail("set on: elements not displayed / box not checked");
  // 4) entity checkbox on a displayed set adds a type, second click removes it
  dm->clickCell(r, 1 + 0); // n
  mk = cgxDsDisplayedMask(idx);
  if (!(mk & fBit) || !(mk & eBit))
    fail("entity n not added on displayed set");
  dm->clickCell(r, 1 + 0);
  mk = cgxDsDisplayedMask(idx);
  if ((mk & fBit) || !(mk & eBit))
    fail("entity n not removed");
  dm->clickCell(r, 0);
  mk = cgxDsDisplayedMask(idx);
  std::fprintf(stderr, "SELFTEST displaysets-off mask=%d checked=%d\n", mk,
               tb->item(r, 0)->checkState() == Qt::Checked);
  if (mk != 0 || tb->item(r, 0)->checkState() == Qt::Checked)
    fail("set off: still displayed / still checked");
  // 5) entity click on a HIDDEN set only changes the selection
  dm->clickCell(r, 1 + 0);
  if (cgxDsDisplayedMask(idx) != 0)
    fail("entity click on hidden set displayed something");
  dm->clickCell(r, 0);
  mk = cgxDsDisplayedMask(idx);
  if (!(mk & fBit))
    fail("remembered entity selection not applied when switching the set on");
  dm->clickCell(r, 0); // off
  // 5b) untick the only visible type, then tick the set again: must come back
  dm->clickCell(r, 0); // on (n)
  dm->clickCell(r, 1 + 1); // remove e (n still on)
  dm->clickCell(r, 1 + 0); // remove n -> nothing left, set off
  if (cgxDsDisplayedMask(idx) != 0)
    fail("last type untick did not turn the set off");
  dm->clickCell(r, 0);
  if (cgxDsDisplayedMask(idx) == 0)
    fail("switch-on after unticking the last type shows nothing");
  dm->clickCell(r, 0);
  dm->hide();
  QApplication::processEvents();

  // 6) a set plotted from the command line shows up checked after reopening
  sendText(g, "plus e DSt05 r");
  sendKey(g, '\r', QPoint());
  dm->popup(m->mapToGlobal(QPoint(60, 40)));
  QApplication::processEvents();
  const int r5 = dm->rowForSet(QStringLiteral("DSt05"));
  const bool c5 = r5 >= 0 && tb->item(r5, 0)->checkState() == Qt::Checked;
  std::fprintf(stderr, "SELFTEST displaysets-cmdplot checked=%d\n", c5);
  if (!c5)
    fail("set plotted by command not shown as checked");
  {
    // eyes-on artefacts: the popup itself and the window behind it
    const QString dir = QString::fromLocal8Bit(shotDir());
    dm->grab().save(dir + QStringLiteral("/s16_displaysets_popup.png"));
    m->grab().save(dir + QStringLiteral("/s16_displaysets_window.png"));
  }

  // 7) changes while the popup is OPEN: create / modify / delete / slot reuse
  auto run = [&](const char *cmd) {
    sendText(g, cmd);
    sendKey(g, '\r', QPoint());
  };
  const int rows0 = tb->rowCount();
  tb->verticalScrollBar()->setValue(10);
  run("seta DSnew e all");
  dm->pollNow();
  if (tb->rowCount() != rows0 + 1 || dm->rowForSet(QStringLiteral("DSnew")) < 0)
    fail("open popup: created set did not appear");
  if (tb->verticalScrollBar()->value() != 10)
    fail("open popup: scroll position lost on refresh");
  int rn = dm->rowForSet(QStringLiteral("DSnew"));
  if (rn >= 0 && (tb->item(rn, 1 + 2)->flags() & Qt::ItemIsEnabled))
    fail("face cell enabled before the set has faces");
  run("seta DSnew f all");
  dm->pollNow();
  rn = dm->rowForSet(QStringLiteral("DSnew"));
  if (rn < 0 || !(tb->item(rn, 1 + 2)->flags() & Qt::ItemIsEnabled))
    fail("open popup: modified set (new faces) not reflected");
  run("plus e DSnew g");
  dm->pollNow();
  rn = dm->rowForSet(QStringLiteral("DSnew"));
  if (rn < 0 || tb->item(rn, 0)->checkState() != Qt::Checked)
    fail("open popup: plus from command line not reflected");
  // slot reuse: delete DSnew, create DSreuse (takes the freed slot), then click
  // the OLD row without letting the timer run first
  run("del se DSnew");
  run("seta DSreuse e all");
  const int stale = rn;
  const int reuseIdx = [&]() {
    for (int i = 0; i < cgxDsSetSlots(); i++)
      if (cgxDsSetValid(i) && QString::fromUtf8(cgxDsSetName(i)) == QLatin1String("DSreuse"))
        return i;
    return -1;
  }();
  dm->clickCell(stale, 0);
  if (reuseIdx < 0 || cgxDsDisplayedMask(reuseIdx) != 0)
    fail("stale row click acted on a different (slot-reused) set");
  if (dm->rowForSet(QStringLiteral("DSnew")) >= 0 || dm->rowForSet(QStringLiteral("DSreuse")) < 0)
    fail("open popup: delete/slot reuse not refreshed after stale click");
  // recreated name starts from the default selection (remembered one pruned)
  run("seta DSnew n all");
  run("seta DSnew e all");
  dm->pollNow();
  rn = dm->rowForSet(QStringLiteral("DSnew"));
  std::fprintf(stderr, "SELFTEST displaysets-live rows=%d\n", tb->rowCount());
  if (rn < 0 || tb->item(rn, 0)->checkState() == Qt::Checked)
    fail("recreated set wrongly shown as displayed");
  dm->hide();
  QApplication::processEvents();
}

void step(int n);
void next(int n, int ms)
{
  QTimer::singleShot(ms, [n]() { step(n); });
}

void step(int n)
{
  GraphicsView *g = GV();
  if (!g)
  {
    std::fprintf(stderr, "SELFTEST-FAIL no-graphics-view\n");
    QCoreApplication::exit(1);
    return;
  }
  switch (n)
  {
  case 0: // wait for model load, then settle
    if (iniActionsFlag != 0)
    {
      if (++s_waits > 300)
      {
        std::fprintf(stderr, "SELFTEST-TIMEOUT waiting for model\n");
        QCoreApplication::exit(1);
        return;
      }
      next(0, 200);
      return;
    }
    if (++s_settle < 5)
    {
      next(0, 200);
      return;
    }
    std::fprintf(stderr, "SELFTEST loaded\n");
    if (CgxMainWindow *m = MW())
    {
      QStringList menus;
      for (QAction *act : m->menuBar()->actions())
        menus << act->text();
      std::fprintf(stderr, "SELFTEST menubar=[%s]\n", menus.join(QStringLiteral(", ")).toUtf8().constData());
    }
    // Optional: CGX_QT_SELFTEST_CMDS="ds 1 e 1" runs commands first (e.g. to
    // show a result so the legend is active).
    if (const char *cmds = std::getenv("CGX_QT_SELFTEST_CMDS"))
    {
      sendText(g, cmds);
      sendKey(g, '\r', QPoint());
      std::fprintf(stderr, "SELFTEST step=cmds [%s]\n", cmds);
      next(1, 2500);
      return;
    }
    next(1);
    return;
  case 1:
    shot("base");
    axesShot("axes_base");
    menuShot("base");
    screenCheck("base");
    next(2);
    return;
  case 2:
  {
    // Left-drag: press + 10 interpolated moves + release.
    const QPointF p0 = g->rect().center() + QPointF(-40, 0);
    QMouseEvent press(QEvent::MouseButtonPress, p0, p0, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(g, &press);
    for (int i = 1; i <= 10; ++i)
    {
      const QPointF p = p0 + QPointF(i * 10, i * 4);
      QMouseEvent mv(QEvent::MouseMove, p, p, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent(g, &mv);
    }
    const QPointF p1 = p0 + QPointF(100, 40);
    QMouseEvent rel(QEvent::MouseButtonRelease, p1, p1, Qt::LeftButton, Qt::NoButton,
                    Qt::NoModifier);
    QApplication::sendEvent(g, &rel);
    std::fprintf(stderr, "SELFTEST step=drag\n");
    next(3);
    return;
  }
  case 3:
    shot("rot");
    axesShot("axes");
    screenCheck("rot");
    if (CgxMainWindow *m = MW())
    {
      if (MenuView *mv = m->menuView())
      {
        const QPointF p0(50, 100);
        QMouseEvent press(QEvent::MouseButtonPress, p0, p0, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(mv, &press);
        for (int i = 1; i <= 5; ++i)
        {
          const QPointF p = p0 + QPointF(i * 10, i * 4);
          QMouseEvent mvEv(QEvent::MouseMove, p, p, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
          QApplication::sendEvent(mv, &mvEv);
        }
        const QPointF p1 = p0 + QPointF(50, 20);
        QMouseEvent rel(QEvent::MouseButtonRelease, p1, p1, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(mv, &rel);
        std::fprintf(stderr, "SELFTEST step=legend-drag\n");
      }
    }
    next(4);
    return;
  case 4:
  {
    const QPointF p = g->rect().center();
    const char *wenv = std::getenv("CGX_QT_SELFTEST_WHEEL");
    const int wheelArg = wenv ? std::atoi(wenv) : 3; // negative = zoom the other way
    const int wheelSteps = std::abs(wheelArg);
    const int wheelDelta = wheelArg < 0 ? -120 : 120;
    for (int i = 0; i < wheelSteps; ++i)
    {
      QWheelEvent wheel(p, p, QPoint(0, 0), QPoint(0, wheelDelta), Qt::NoButton, Qt::NoModifier,
                        Qt::NoScrollPhase, false);
      QApplication::sendEvent(g, &wheel);
    }
    std::fprintf(stderr, "SELFTEST step=wheel\n");
    next(5);
    return;
  }
  case 5:
    shot("wheel");
    screenCheck("wheel");
    next(6);
    return;
  case 6:
    sendText(g, "help");
    sendKey(g, '\r', QPoint());
    std::fprintf(stderr, "SELFTEST step=keys-help\n");
    next(7, 800);
    return;
  case 7:
    // `view cl` toggles the command line: w3 created -> overlay visible.
    // (`cl` is a pre_view parameter, not a top-level command.)
    s_viewWPre = g->width();
    s_viewHPre = g->height();
    sendText(g, "view cl");
    sendKey(g, '\r', QPoint());
    std::fprintf(stderr, "SELFTEST step=cl-on (view %dx%d)\n", s_viewWPre, s_viewHPre);
    next(8, 800);
    return;
  case 8:
  {
    QLineEdit *cl = CL();
    std::fprintf(stderr, "SELFTEST cmdline-visible=%d w3=%d flag=%d\n",
                 cl && cl->isVisible() ? 1 : 0, w3, (int)commandLineFlag);
    // Step 13: the command line must NOT squeeze the 3D view — it overlays
    // the bottom band inside the container, so the view keeps its size.
    if (g->width() != s_viewWPre || g->height() != s_viewHPre)
    {
      std::fprintf(stderr, "SELFTEST-FAIL cmdline-on squeezed 3D view: %dx%d -> %dx%d\n",
                   s_viewWPre, s_viewHPre, g->width(), g->height());
      ++s_failures;
    }
    if (cl && cl->parentWidget() && g->parentWidget() &&
        cl->parentWidget() != g->parentWidget())
    {
      std::fprintf(stderr, "SELFTEST-FAIL cmdline not inside the graphics container\n");
      ++s_failures;
    }
    // Step 13: focus-on-show (legacy makes w3 the active window) and the
    // bottom band must be hit-testable: a click there has to reach the
    // command line, not the 3D view underneath it. widgetAt() runs Qt's
    // real routing (incl. the stacked native overlays), so this fails if
    // any overlay covers the band again.
    if (cl && cl->isVisible())
    {
      const bool focused = cl->hasFocus();
      const QPoint p = cl->mapToGlobal(cl->rect().center());
      QWidget *hit = QApplication::widgetAt(p);
      std::fprintf(stderr, "SELFTEST cmdline focus=%d band-hit=%s\n", focused ? 1 : 0,
                   hit ? hit->metaObject()->className() : "(null)");
      if (!focused)
      {
        std::fprintf(stderr, "SELFTEST-FAIL cmdline not focused after toggle-on\n");
        ++s_failures;
      }
      if (!hit || (hit != cl && !cl->isAncestorOf(hit)))
      {
        std::fprintf(stderr, "SELFTEST-FAIL band click would not reach the command line\n");
        ++s_failures;
      }
    }
    fullshot("dock");
    consoleCheck("on", true, true);
    screenCheck("console-on");
    // wheel scrolls the panel (and does not reach the 3D view)
    if (CgxMainWindow *m = MW())
      if (ConsoleView *c = m->consoleView())
      {
        const int before = c->scrollValue();
        const QPointF p = c->rect().center();
        QWheelEvent wheel(p, p, QPoint(0, 0), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(c, &wheel);
        std::fprintf(stderr, "SELFTEST console-wheel: scroll %d -> %d\n", before,
                     c->scrollValue());
        if (c->scrollValue() >= before)
        {
          std::fprintf(stderr, "SELFTEST-FAIL console wheel did not scroll up\n");
          ++s_failures;
        }
        // scrollbar: click above the thumb pages up, below pages down (5 lines)
        c->setScrollValue(100);
        const QPoint above(c->width() - 6, 3);
        const QPoint below(c->width() - 6, c->height() - 4);
        auto click = [&](const QPoint &tp) {
          QMouseEvent press(QEvent::MouseButtonPress, tp, tp, Qt::LeftButton, Qt::LeftButton,
                            Qt::NoModifier);
          QApplication::sendEvent(c, &press);
          QMouseEvent rel(QEvent::MouseButtonRelease, tp, tp, Qt::LeftButton, Qt::NoButton,
                          Qt::NoModifier);
          QApplication::sendEvent(c, &rel);
        };
        click(above);
        const int afterUp = c->scrollValue();
        click(below);
        const int afterDown = c->scrollValue();
        // drag the thumb to the very top, then to the very bottom
        auto drag = [&](int fromY, int toY) {
          const int x = c->width() - 6;
          QMouseEvent press(QEvent::MouseButtonPress, QPoint(x, fromY), QPoint(x, fromY),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
          QApplication::sendEvent(c, &press);
          QMouseEvent mv(QEvent::MouseMove, QPoint(x, toY), QPoint(x, toY), Qt::NoButton,
                         Qt::LeftButton, Qt::NoModifier);
          QApplication::sendEvent(c, &mv);
          QMouseEvent rel(QEvent::MouseButtonRelease, QPoint(x, toY), QPoint(x, toY),
                          Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
          QApplication::sendEvent(c, &rel);
        };
        c->setScrollValue(c->scrollMax() / 2);
        drag(c->height() / 2, -50);
        const int dragTop = c->scrollValue();
        drag(5, c->height() + 50);
        const int dragBottom = c->scrollValue();
        std::fprintf(stderr,
                     "SELFTEST console-scrollbar: pageUp 100->%d pageDown ->%d dragTop=%d "
                     "dragBottom=%d/%d\n",
                     afterUp, afterDown, dragTop, dragBottom, c->scrollMax());
        if (afterUp != 95 || afterDown != 100 || dragTop != 0 || dragBottom != c->scrollMax())
        {
          std::fprintf(stderr, "SELFTEST-FAIL console scrollbar paging/drag wrong\n");
          ++s_failures;
        }
        c->setScrollValue(c->scrollMax()); // back to following the bottom

        // Focus: a click into the text gives the panel focus (Ctrl+C needs it).
        const int rowLine = c->scrollValue() + 1;
        const QPoint tp = c->cellPos(rowLine, 3);
        QMouseEvent fpress(QEvent::MouseButtonPress, tp, tp, Qt::LeftButton, Qt::LeftButton,
                           Qt::NoModifier);
        QApplication::sendEvent(c, &fpress);
        QMouseEvent frel(QEvent::MouseButtonRelease, tp, tp, Qt::LeftButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(c, &frel);
        const bool gotFocus = QApplication::focusWidget() == c;
        std::fprintf(stderr, "SELFTEST console-focus: click focuses panel=%d\n", gotFocus ? 1 : 0);
        if (!gotFocus)
        {
          std::fprintf(stderr, "SELFTEST-FAIL console: click did not focus the panel\n");
          ++s_failures;
        }
        // Keys the panel does not use are forwarded to the 3D view, so cgx
        // hotkeys/commands keep working with the panel focused. (This runs a
        // third `help`; the harness expects 3 now.)
        const unsigned long long capBefore = cgxconsole::bytesCaptured();
        sendText(c, "help");
        sendKey(c, '\r', QPoint());
        std::fprintf(stderr, "SELFTEST console-forward: typed help into the focused panel\n");
        (void)capBefore;
        // Esc clears the selection and returns the focus to the 3D view.
        c->setSelection(rowLine, 0, rowLine + 1, 12);
        c->grabFramebuffer().save(QString::fromLocal8Bit(shotDir()) + QStringLiteral("/s11_selection.png"));
        sendSpecial(c, Qt::Key_Escape);
        const bool back = QApplication::focusWidget() == GV() && !c->hasSelection();
        std::fprintf(stderr, "SELFTEST console-esc: focus back on 3D view and selection cleared=%d\n",
                     back ? 1 : 0);
        if (!back)
        {
          std::fprintf(stderr, "SELFTEST-FAIL console: Esc did not restore focus/selection\n");
          ++s_failures;
        }
      }
    next(9);
    return;
  }
  case 9:
    // Type into the dock (exercises QLineEdit + returnPressed submit path).
    if (QLineEdit *cl = CL())
    {
      sendText(cl, "help");
      sendKey(cl, '\r', QPoint());
    }
    std::fprintf(stderr, "SELFTEST step=dock-help\n");
    next(10, 800);
    return;
  case 10:
  {
    // The command typed into the dock is echoed into the console panel.
    if (CgxMainWindow *m = MW())
      if (ConsoleView *c = m->consoleView())
      {
        const bool echoed = c->plainText().contains(QStringLiteral(": help\n"));
        std::fprintf(stderr, "SELFTEST console-echo: %d\n", echoed ? 1 : 0);
        if (!echoed)
        {
          std::fprintf(stderr, "SELFTEST-FAIL console: dock command not echoed\n");
          ++s_failures;
        }
      }
    // Up recalls history into the dock (legacy keystroke buffer is truth).
    if (QLineEdit *cl = CL())
    {
      sendSpecial(cl, Qt::Key_Up);
      std::fprintf(stderr, "SELFTEST history-text=%s\n", cl->text().toUtf8().constData());
    }
    next(11, 600);
    return;
  }
  case 11:
  {
    // Selection focus hand-off (capture + release + line-edit backstop):
    // a pick command issued from the dock must move the keyboard to the
    // 3D view — a/r/e/q are selection keys there, never text in the line —
    // and `q` must bring the focus back to the line. Fully synchronous:
    // qenq only installs pick(), and a `q` key exits it immediately.
    QLineEdit *cl = CL();
    if (cl)
    {
      cl->clear();
      // Real flow: the user's focus sits in the line (click, or the
      // focus-on-show of the Alt+C toggle) when the command is issued —
      // synthetic sendEvent() alone does not move focus.
      cl->setFocus(Qt::OtherFocusReason);
      sendText(cl, "qenq");
      sendKey(cl, '\r', QPoint());
      const int modal = cgxKeyFuncIsModal();
      const int handed = (QApplication::focusWidget() == g);
      std::fprintf(stderr, "SELFTEST pick-capture: pickFlag=%d modal=%d focusView=%d\n",
                   (int)pickFlag, modal, handed);
      if (!pickFlag || !modal || !handed)
      {
        std::fprintf(stderr, "SELFTEST-FAIL pick: no focus hand-off from the command line\n");
        ++s_failures;
      }
      // (a) normal path: `q` typed into the view ends the selection and
      // the focus returns to the command line automatically.
      sendKey(g, 'q', QPoint());
      const int ended = (!pickFlag && !cgxKeyFuncIsModal());
      const int back = (QApplication::focusWidget() == cl);
      std::fprintf(stderr, "SELFTEST pick-release: ended=%d focusBackOnLine=%d\n", ended, back);
      if (!ended || !back)
      {
        std::fprintf(stderr, "SELFTEST-FAIL pick: selection did not end with focus restore\n");
        ++s_failures;
      }
      // (b) backstop: focus moved back into the line mid-pick — the key
      // must still reach pick() and must never appear as text.
      cl->setFocus(Qt::OtherFocusReason);
      sendText(cl, "qenq");
      sendKey(cl, '\r', QPoint());
      sendKey(cl, 'q', QPoint()); // eventFilter forwards it while modal
      const int swallowed = cl->text().isEmpty();
      const int ended2 = (!pickFlag && !cgxKeyFuncIsModal());
      std::fprintf(stderr, "SELFTEST pick-backstop: swallowed=%d ended=%d\n", swallowed, ended2);
      if (!swallowed || !ended2)
      {
        std::fprintf(stderr, "SELFTEST-FAIL pick: key reached the line edit instead of pick()\n");
        ++s_failures;
      }
      // ...and ordinary typing works again afterwards.
      sendText(cl, "ab");
      const int typing = (cl->text() == QStringLiteral("ab"));
      cl->clear();
      if (!typing)
      {
        std::fprintf(stderr, "SELFTEST-FAIL pick: line edit dead after the selection\n");
        ++s_failures;
      }
    }
    // Switch back off from inside the dock (`view cl` alone only turns on;
    // the toggle lives in menu item 5, off-switch is `view cl off`).
    // NOTE: clear first — step 10 left recalled history in the widget and
    // key events append to it.
    if (QLineEdit *cl = CL())
    {
      cl->clear();
      sendText(cl, "view cl off");
      sendKey(cl, '\r', QPoint());
    }
    std::fprintf(stderr, "SELFTEST step=cl-off\n");
    next(12, 600);
    return;
  }
  case 12:
  {
    QLineEdit *cl = CL();
    std::fprintf(stderr, "SELFTEST cmdline-visible=%d w3=%d flag=%d\n",
                 cl && cl->isVisible() ? 1 : 0, w3, (int)commandLineFlag);
    fullshot("final");
    consoleCheck("off", false, false);
    // Alt+C toggle (Step 11 amendment): the binding exists, both directions
    // work through the same menu(5) path, and the Alt-modified keypress never
    // leaks a 'c' into the legacy keystroke buffer from any focus.
    if (CgxMainWindow *m = MW())
    {
      // Synthetic key events DO reach the QShortcut (sendEvent goes through
      // notify()), so the press below exercises the real end-to-end path.
      auto altCPress = [&](QObject *target) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_C, Qt::AltModifier);
        QApplication::sendEvent(target, &press);
      };
      QShortcut *sc = nullptr;
      for (QShortcut *cand : m->findChildren<QShortcut *>())
        if (cand->key() == QKeySequence(Qt::ALT | Qt::Key_C) &&
            cand->context() == Qt::WindowShortcut)
          sc = cand;
      std::fprintf(stderr, "SELFTEST alt-c: shortcut bound=%d\n", sc ? 1 : 0);
      if (!sc)
      {
        std::fprintf(stderr, "SELFTEST-FAIL alt-c: shortcut missing\n");
        ++s_failures;
      }
      else
      {
        // Anti-leak guards: with the shortcut disabled the keypress must
        // reach the widgets and be swallowed (no 'c' in the parser, no
        // visibility change) from every focus.
        sc->setEnabled(false);
        if (keystroke)
          keystroke[0] = '\0';
        if (GraphicsView *g = GV())
          altCPress(g);
        if (QLineEdit *dock = CL())
          altCPress(dock);
        if (ConsoleView *c = m->consoleView())
          altCPress(c);
        const bool leaked = (keystroke && keystroke[0] != '\0') || commandLineFlag ||
                            (CL() && CL()->isVisible());
        std::fprintf(stderr, "SELFTEST alt-c: leak with shortcut disabled=%d\n", leaked ? 1 : 0);
        if (leaked)
        {
          std::fprintf(stderr, "SELFTEST-FAIL alt-c: keypress had a side effect\n");
          ++s_failures;
        }
        sc->setEnabled(true);
        // End-to-end through the shortcut: off -> on -> off.
        auto shown = [&]() {
          return CL() && CL()->isVisible() && m->consoleView() &&
                 m->consoleView()->isVisible() && commandLineFlag;
        };
        if (GraphicsView *g = GV())
          altCPress(g);
        const bool on = shown();
        if (GraphicsView *g = GV())
          altCPress(g);
        const bool off = !shown() && !commandLineFlag;
        std::fprintf(stderr, "SELFTEST alt-c: toggle on=%d off=%d\n", on ? 1 : 0, off ? 1 : 0);
        if (!on || !off)
        {
          std::fprintf(stderr, "SELFTEST-FAIL alt-c: toggle did not flip both widgets\n");
          ++s_failures;
        }
      }
    }
    if (CgxMainWindow *m = MW())
      m->resize(m->width() + 90, m->height() + 70); // resize path
    next(13, 1200);
    return;
  }
  case 13:
    screenCheck("resized");
    next(14, 200);
    return;
  case 14:
  {
    // Deadlock regression: print ~1.5 MB from THIS (GUI) thread without
    // returning to the event loop. Without the reader thread the 64 KB pipe
    // fills and this loop would block forever.
    const unsigned long long before = cgxconsole::bytesCaptured();
    for (int i = 0; i < 30000; ++i)
      std::printf("FLOOD %06d the quick brown fox jumps over the lazy dog\n", i);
    std::fflush(stdout);
    std::fprintf(stderr, "SELFTEST flood: printed 30000 lines (%llu bytes so far)\n",
                 cgxconsole::bytesCaptured() - before);
    next(15, 1500);
    return;
  }
  case 15:
  {
    CgxMainWindow *m = MW();
    ConsoleView *c = m ? m->consoleView() : nullptr;
    if (c)
    {
      const QString t = c->plainText();
      const int lines = t.count(QLatin1Char('\n')) + 1;
      std::fprintf(stderr,
                   "SELFTEST flood-result: lines=%d (cap %d) hasLast=%d hasFirst=%d captured=%llu "
                   "dropped=%llu\n",
                   lines, ConsoleView::kMaxLines, t.contains(QStringLiteral("FLOOD 029999")) ? 1 : 0,
                   t.contains(QStringLiteral("FLOOD 000000")) ? 1 : 0, cgxconsole::bytesCaptured(),
                   cgxconsole::bytesDropped());
      if (!t.contains(QStringLiteral("FLOOD 029999")))
      {
        std::fprintf(stderr, "SELFTEST-FAIL flood: last line missing\n");
        ++s_failures;
      }
      if (lines > ConsoleView::kMaxLines + 1)
      {
        std::fprintf(stderr, "SELFTEST-FAIL flood: line cap exceeded\n");
        ++s_failures;
      }
      if (t.contains(QStringLiteral("FLOOD 000000")))
      {
        std::fprintf(stderr, "SELFTEST-FAIL flood: oldest line should have been trimmed\n");
        ++s_failures;
      }
    }
    // Selection test data: one very long line (wraps over several display
    // lines) and one short known line, printed through the real capture path.
    std::printf("WRAPTEST %s END\n", std::string(300, 'a').c_str());
    std::printf("SELTEST alpha beta gamma\n");
    std::fflush(stdout);
    next(16, 900);
    return;
  }
  case 16:
  {
    selectionChecks();
    displaySetsChecks();
    if (s_failures)
    {
      std::fprintf(stderr, "SELFTEST-FAILED failures=%d\n", s_failures);
      QCoreApplication::exit(1);
      return;
    }
    std::fprintf(stderr, "SELFTEST-DONE\n");
    QCoreApplication::quit();
    return;
  }
  }
}
} // namespace

namespace
{
// CGX_QT_KEYS="ds 1 e 1;view cl;help": type these commands (';' = Return)
// into the 3D view after the model has loaded, then just stay open. For
// eyes-on checks and external screenshots; independent of CGX_QT_SELFTEST.
int s_keysIdx = 0;
bool s_keysReported = false;
QStringList s_keys;
void keysStep()
{
  GraphicsView *g = GV();
  if (!g || iniActionsFlag != 0)
  {
    QTimer::singleShot(250, keysStep);
    return;
  }
  if (s_keysIdx >= s_keys.size())
  {
    if (!s_keysReported)
    {
      s_keysReported = true;
      CgxMainWindow *m = MW();
      auto G = [&](QWidget *w) {
        const QPoint t = w->mapToGlobal(QPoint(0, 0));
        return QRect(t, w->size());
      };
      const QRect v = m && m->graphicsView() ? G(m->graphicsView()) : QRect();
      const QRect c = m && m->consoleView() ? G(m->consoleView()) : QRect();
      std::fprintf(stderr, "KEYS-DONE view=%d,%d,%d,%d console=%d,%d,%d,%d consoleVisible=%d\n",
                   v.x(), v.y(), v.width(), v.height(), c.x(), c.y(), c.width(), c.height(),
                   m && m->consoleView() && m->consoleView()->isVisible() ? 1 : 0);
    }
    return;
  }
  const QByteArray cmd = s_keys[s_keysIdx++].trimmed().toLatin1();
  sendText(g, cmd.constData());
  sendKey(g, '\r', QPoint());
  std::fprintf(stderr, "KEYS sent [%s]\n", cmd.constData());
  QTimer::singleShot(1200, keysStep);
}
} // namespace

extern "C" void cgxMaybeStartSelftest(void)
{
  if (const char *keys = std::getenv("CGX_QT_KEYS"))
  {
    s_keys = QString::fromLatin1(keys).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    QTimer::singleShot(1500, keysStep);
  }
  if (!std::getenv("CGX_QT_SELFTEST"))
    return;
  std::fprintf(stderr, "SELFTEST armed\n");
  next(0, 500);
}

#else // !CGX_QT_TESTHOOKS

// Default build: no environment-driven hooks at all.
extern "C" void cgxMaybeStartSelftest(void) {}

#endif // CGX_QT_TESTHOOKS

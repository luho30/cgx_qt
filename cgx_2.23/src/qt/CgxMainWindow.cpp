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

#include "CgxMainWindow.h"
#include "CgxViews.h"
#include "ConsoleView.h"
#include "glue.h"

#include <cstdio>

#include <QApplication>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenuBar>
#include <QResizeEvent>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWidget>

// Legacy globals live in cgx.c (non-static).
extern int width_menu;
extern "C" int curshft;
extern "C" char *keystroke;

GraphicsContainer::GraphicsContainer(GraphicsView *graphics, MenuView *menu, QWidget *parent)
    : QWidget(parent), m_graphics(graphics), m_menu(menu), m_menuW((width_menu > 0) ? width_menu : 184)
{
  // Full-window 3D graphics: GraphicsView is layout-managed and fills 100%
  // of the container viewport. The semi-transparent legend (MenuView) and
  // the coordinate axes (AxesView) float on top as overlay widgets.
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(graphics);

  if (m_menu)
  {
    m_menu->setParent(this);
    m_menu->show();
  }

  m_axes = new AxesView(this);
  m_axes->setFixedSize(kAxesSize, kAxesSize);
  m_axes->show();

  // Step 11: console panel (last 5 lines of stdout), full width, anchored to
  // the bottom edge = directly above the command line. Hidden until the
  // command line is switched on. Stacked above legend and axes.
  m_console = new ConsoleView(this);
  m_console->setFocusReturn(graphics); // Esc / unused keys go to the 3D view
  m_console->hide();

  // Step 13: the command line (legacy w3) overlays the bottom band of the
  // 3D view instead of living in the central layout (which squeezed the
  // 3D view by its height). An overlay widget — placed by layoutOverlays
  // with the console stacked directly above it. Fixed height so the band
  // size is deterministic for layoutOverlays.
  m_cmdLine = new QLineEdit(this);
  m_cmdLine->setFixedHeight(m_cmdLine->sizeHint().height());
  m_cmdLine->hide();

  layoutOverlays();
}

void GraphicsContainer::layoutOverlays()
{
  // z-order, bottom to top: 3D view, legend, axes, command line, console.
  // The bottom clH band belongs to the command line: legend, axes and
  // console are kept OUT of it (Step 13) so the line — an ordinary widget,
  // which cannot be stacked above the WA_AlwaysStackOnTop overlays — has no
  // overlap with them and still gets its clicks and its pixels.
  const int clH = (m_cmdLine && m_cmdLine->isVisible()) ? m_cmdLine->height() : 0;
  if (m_menu)
  {
    m_menu->setGeometry(0, 0, m_menuW, height() - clH);
    m_menu->raise();
  }
  if (m_axes)
  {
    // Position tripod in bottom-right corner clear of the legend and, when
    // the command line is on, clear of the command-line band (same absolute
    // position as before the band moved into the container).
    m_axes->move(width() - kAxesMargin - kAxesSize,
                 height() - clH - kAxesMargin - kAxesSize);
    m_axes->raise();
  }
  if (m_cmdLine && m_cmdLine->isVisible())
  {
    m_cmdLine->setGeometry(0, height() - clH, width(), clH);
    m_cmdLine->raise(); // above the 3D view; disjoint from the stacked overlays
  }
  if (m_console)
  {
    const int h = m_console->panelHeight();
    m_console->setGeometry(0, height() - clH - h, width(), h);
    m_console->raise();
  }
}

void GraphicsContainer::setConsoleVisible(bool visible)
{
  if (!m_console)
    return;
  layoutOverlays();
  m_console->setVisible(visible);
  if (visible)
    m_console->raise();
}

void GraphicsContainer::setCommandLineVisible(bool visible)
{
  // Line first: layoutOverlays derives the bottom band height from its
  // visibility. The console shows/hides together with the line (it sits
  // directly above the band) and its setter re-runs layoutOverlays.
  if (m_cmdLine)
    m_cmdLine->setVisible(visible);
  setConsoleVisible(visible);
  if (visible)
  {
    if (m_cmdLine)
    {
      m_cmdLine->raise();
      // Legacy creates w3 as the active window (cgx.c menu(5): activWindow
      // = w3), i.e. typing goes to the command line right after toggling.
      m_cmdLine->setFocus(Qt::OtherFocusReason);
    }
  }
  else if (m_graphics)
  {
    m_graphics->setFocus(Qt::OtherFocusReason);
  }
}

void GraphicsContainer::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  layoutOverlays();
}

CgxMainWindow::CgxMainWindow(QWidget *parent) : QMainWindow(parent)
{
  QWidget *central = new QWidget(this);
  QVBoxLayout *outer = new QVBoxLayout(central);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  MenuView *menu = new MenuView();
  GraphicsView *graphics = new GraphicsView();
  graphics->setMinimumSize(400, 400);

  m_graphicsContainer = new GraphicsContainer(graphics, menu, central);
  outer->addWidget(m_graphicsContainer, 1);

  // Step 13: the command line is owned by the GraphicsContainer (bottom
  // overlay band, see there) — no layout slot here, or it would squeeze the
  // 3D view again. This window only wires it up.
  // Step 7: command line (legacy w3). Hidden until w3 is created
  // (menu item 5 / `cl` command); visibility follows w3, not focus.
  m_cmdLine = m_graphicsContainer->commandLine();
  m_cmdLine->setPlaceholderText(QStringLiteral("cgx command (Return to execute, Up/Down for history)"));
  m_cmdLinePlaceholder = m_cmdLine->placeholderText();
  m_cmdLine->installEventFilter(this);
  connect(m_cmdLine, &QLineEdit::returnPressed, this, [this]() { submitCmdLine(); });

  setCentralWidget(central);
  // Alt+C toggles the command line + console panel, exactly like the menubar
  // "Toggle CommandLine" entry (same legacy menu(5) path via
  // cgxToggleCommandLine). Window scope: fires from any focus in the window
  // (3D view, legend, dock, console). Deliberately NOT a shortcut on the
  // menubar action itself: the bar is rebuilt on every menu mutation and two
  // triggers on one action would double-toggle.
  QShortcut *cmdLineShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_C), this);
  cmdLineShortcut->setContext(Qt::WindowShortcut);
  cmdLineShortcut->setAutoRepeat(false);
  connect(cmdLineShortcut, &QShortcut::activated, []() { cgxToggleCommandLine(); });
  // Ensure the menu bar is always physically embedded in the window header,
  // never exported to an external/desktop global panel.
  menuBar()->setNativeMenuBar(false);
  // Initial size comes from glue (width_w0 x height_w0 at creation time).
}

MenuView *CgxMainWindow::menuView() const
{
  return m_graphicsContainer ? m_graphicsContainer->menuView() : nullptr;
}

GraphicsView *CgxMainWindow::graphicsView() const
{
  return m_graphicsContainer ? m_graphicsContainer->findChild<GraphicsView *>() : nullptr;
}

AxesView *CgxMainWindow::axesView() const
{
  return m_graphicsContainer ? m_graphicsContainer->axesView() : nullptr;
}

ConsoleView *CgxMainWindow::consoleView() const
{
  return m_graphicsContainer ? m_graphicsContainer->consoleView() : nullptr;
}

void CgxMainWindow::setCmdLineVisible(bool visible)
{
  // Step 13: overlay visibility + focus live in the container now; the
  // console follows the line (they show/hide together).
  if (m_graphicsContainer)
    m_graphicsContainer->setCommandLineVisible(visible);
  // Edge: the line toggled on while a selection is already in progress —
  // show the capture state even though setKeyCapture ran earlier.
  if (visible && cgxKeyFuncIsModal())
    applyCmdLineCaptureHint(true);
}

void CgxMainWindow::applyCmdLineCaptureHint(bool capture)
{
  if (!m_cmdLine)
    return;
  m_cmdLine->setReadOnly(capture);
  m_cmdLine->setPlaceholderText(capture
                                    ? QStringLiteral("selecting — keys go to the 3D view, "
                                                     "q ends the selection")
                                    : m_cmdLinePlaceholder);
}

void CgxMainWindow::setKeyCapture(bool capture)
{
  if (capture)
  {
    // Remember the focus owner only if it would swallow the pick keys; the
    // 3D view and the console panel already forward them, so leave those
    // alone (reading/copying the console during a selection must not lose
    // its focus).
    QWidget *focus = QApplication::focusWidget();
    const bool swallowsKeys =
        focus && focus != static_cast<QWidget *>(graphicsView()) && focus != consoleView();
    if (swallowsKeys && !m_focusBeforeCapture)
    {
      m_focusBeforeCapture = focus;
      // Hand the keyboard to the legacy handler: pick()/defineDiv() need
      // the keys, exactly like in the GLUT build where no line edit existed
      // to steal them.
      if (GraphicsView *g = graphicsView())
        g->setFocus(Qt::OtherFocusReason);
    }
    applyCmdLineCaptureHint(true);
  }
  else
  {
    applyCmdLineCaptureHint(false);
    QWidget *prev = m_focusBeforeCapture.data();
    m_focusBeforeCapture.clear();
    // Give the focus back — but only if the user has not moved it elsewhere
    // in the meantime (clicking the console or the view stays put).
    if (prev && prev->isVisible() && prev->isEnabled() &&
        QApplication::focusWidget() == graphicsView())
      prev->setFocus(Qt::OtherFocusReason);
  }
}

void CgxMainWindow::resizeEvent(QResizeEvent *event)
{
  QMainWindow::resizeEvent(event);
  GraphicsView *view = graphicsView();
  if (view)
  {
    view->makeCurrent();
    // Full viewport dims, matching GraphicsView's full-window geometry.
    cgxReshape(view->width(), view->height());
    view->doneCurrent();
  }
}

void CgxMainWindow::submitCmdLine()
{
  // Feed the final string char-by-char into w1's keyboard handler: exactly
  // what clean typing produces (parser accumulates, Return executes).
  // Reset the legacy buffer first: after a history recall it still holds
  // the recalled line, and the parser would append to it ("help"+"view cl
  // off" executed HELP). The widget text is the single source of truth.
  if (keystroke)
    keystroke[0] = '\0';
  curshft = 0;
  const QString text = m_cmdLine ? m_cmdLine->text() : QString();
  // Step 11: cgx does not echo typed characters while the command line is on
  // (see the `!commandLineFlag` guards in Keyboard()), so show the command in
  // the console panel. poll() first so earlier output stays above the echo.
  if (ConsoleView *c = consoleView())
  {
    std::fflush(stdout);
    c->poll();
    c->appendChunk(QStringLiteral(": ") + text + QLatin1Char('\n'));
  }
  const int win = cgxGraphicsId();
  const QPoint center = graphicsView() ? graphicsView()->rect().center() : QPoint();
  for (const QChar c : text)
  {
    const char latin = c.toLatin1();
    if (latin == 0 && c.unicode() != 0)
      continue; // not representable as a legacy byte
    cgxForwardKey(win, (unsigned char)latin, center.x(), center.y());
  }
  cgxForwardKey(win, 13, center.x(), center.y()); // Return executes
  if (m_cmdLine)
    m_cmdLine->clear();
}

void CgxMainWindow::syncCmdLineFromLegacy()
{
  // After Up/Down history recall, the legacy keystroke buffer is the truth.
  if (m_cmdLine)
    m_cmdLine->setText(QString::fromUtf8(keystroke ? keystroke : ""));
}

bool CgxMainWindow::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_cmdLine && event->type() == QEvent::KeyPress)
  {
    QKeyEvent *key = static_cast<QKeyEvent *>(event);
    // Alt-modified keys (in particular Alt+C) are window shortcuts, never
    // dock input: swallow so no character leaks into the command parser.
    if (key->modifiers() & Qt::AltModifier)
      return true;
    if (key->key() == Qt::Key_Escape)
    {
      // Same contract as the console panel: Esc leaves the line and puts
      // the focus back on the 3D view (the text stays, it may be long).
      if (GraphicsView *g = graphicsView())
        g->setFocus(Qt::OtherFocusReason);
      return true;
    }
    // Backstop for a selection in progress (pick/defineDiv/defineValue owns
    // w1's keyboard, see glue's cgxKeyboardFunc): if focus is (back) on the
    // line — click, or the line toggled on mid-pick — its keys must reach
    // the legacy handler instead of becoming text/history/commands. This is
    // what makes a/r/e/q work no matter where the focus sits.
    if (cgxKeyFuncIsModal())
    {
      forwardKeyPress(graphicsView(), cgxGraphicsId(), key);
      return true;
    }
    const int win = cgxGraphicsId();
    const QPoint center = graphicsView() ? graphicsView()->rect().center() : QPoint();
    if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)
    {
      // Legacy history recall (specialKeyboard 101/103); echo the result.
      cgxForwardSpecial(win, key->key() == Qt::Key_Up ? 101 : 103, center.x(), center.y());
      syncCmdLineFromLegacy();
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

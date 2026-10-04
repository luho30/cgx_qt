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
// Step 3: Qt replacement for the GLUT top-level window w0.
// Layout: MenuView (fixed left strip, legacy width_menu) + graphics area.
// Step 7: the graphics area is a GraphicsContainer holding the expanding
// GraphicsView with the AxesView gizmo (legacy w2) floating bottom-left.
// Step 13: the QLineEdit command line (legacy w3) is an overlay INSIDE the
// container (bottom band, full width), like the console above it — it must
// never take layout space, or it would squeeze the 3D view (legacy w3 was a
// subwindow drawn over the bottom of the drawing window, cgx.c reshape).
// No Q_OBJECT extras: virtual overrides and lambda connects need no moc.
#include <QMainWindow>
#include <QPointer>

class MenuView;
class GraphicsView;
class AxesView;
class ConsoleView;
class QLineEdit;
class QResizeEvent;

// Plain holder so the 3D graphics view fills 100% of the viewport,
// while the semi-transparent legend (MenuView) floats on the left, the
// coordinate axes gizmo (AxesView) floats in the bottom-right corner and
// the command line (Step 13) and console panel overlay the bottom band.
class GraphicsContainer : public QWidget
{
public:
  explicit GraphicsContainer(GraphicsView *graphics, MenuView *menu, QWidget *parent = nullptr);
  AxesView *axesView() const { return m_axes; }
  MenuView *menuView() const { return m_menu; }
  ConsoleView *consoleView() const { return m_console; }
  QLineEdit *commandLine() const { return m_cmdLine; }
  void setConsoleVisible(bool visible);
  void setCommandLineVisible(bool visible);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  GraphicsView *m_graphics;
  MenuView *m_menu;
  AxesView *m_axes;
  ConsoleView *m_console;
  QLineEdit *m_cmdLine;
  void layoutOverlays();
  int m_menuW;
  static const int kAxesSize = 110;
  static const int kAxesMargin = 8;
};

class CgxMainWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit CgxMainWindow(QWidget *parent = nullptr);

  MenuView *menuView() const;
  GraphicsView *graphicsView() const;
  AxesView *axesView() const;
  ConsoleView *consoleView() const;
  QLineEdit *cmdLine() const { return m_cmdLine; }
  void setCmdLineVisible(bool visible);

  // Focus hand-off while a selection (pick/defineDiv/defineValue) owns the
  // keyboard: called by glue's cgxKeyboardFunc() on every Keyboard<->pick
  // swap. On capture the 3D view gets the focus (the line would swallow the
  // pick keys as text); on release the focus returns to whoever had it
  // before — unless the user moved it elsewhere in the meantime.
  void setKeyCapture(bool capture);

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void submitCmdLine();
  void syncCmdLineFromLegacy();
  void applyCmdLineCaptureHint(bool capture);

  GraphicsContainer *m_graphicsContainer;
  QLineEdit *m_cmdLine;
  // Focus owner before a selection captured the keyboard (null if none)
  QPointer<QWidget> m_focusBeforeCapture;
  QString m_cmdLinePlaceholder;
};

// Implemented in glue.cpp (owns the singleton instance).
CgxMainWindow *cgxMainWindow();

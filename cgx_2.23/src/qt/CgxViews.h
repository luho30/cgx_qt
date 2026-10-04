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
// Step 3: Qt replacements for the GLUT subwindows w0 (menu panel) and
// w1 (3D graphics). Each paintGL() invokes the currently registered legacy
// display callback for its window id (see glue.h).
// Step 4: both views forward mouse/keyboard input to the per-window legacy
// handlers stored by the cgx*Func shims. Legacy semantics preserved:
//   - w1 (graphics): LEFT/MIDDLE/RIGHT + wheel + motion + keys all delivered
//     (no GLUT menu is attached to w1; the main menu lives on w0).
//   - w0 (menu strip): LEFT is swallowed (menus accessed via the header QMenuBar),
//     MIDDLE/RIGHT/wheel/motion/keys delivered as in legacy.
#include <QOpenGLWidget>

class QKeyEvent;
class QWidget;

// Translate a Qt key event the way GLUT would (ASCII from text()/explicit
// fallbacks, F-keys/arrows as GLUT special codes) and deliver it to the
// legacy handler stored for win. pos comes from the cursor mapped into
// <view> (legacy key events carry the mouse position). Shared by the views
// and by the command line's selection-mode backstop (CgxMainWindow).
bool qtToGlutSpecial(int qtKey, int &glutKey);
bool qtToGlutAscii(QKeyEvent *event, unsigned char &ascii);
void forwardKeyPress(const QWidget *view, int win, QKeyEvent *event);

class MenuView : public QOpenGLWidget
{
  Q_OBJECT
public:
  explicit MenuView(QWidget *parent = nullptr);

protected:
  void paintGL() override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
};

class GraphicsView : public QOpenGLWidget
{
  Q_OBJECT
public:
  explicit GraphicsView(QWidget *parent = nullptr);

protected:
  void paintGL() override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
};

// Step 7: Qt replacement for the GLUT axes-gizmo subwindow w2 (child of w1).
// Floats bottom-left over the graphics view; paintGL() invokes w2's stored
// display callback (DrawAxes). The viewport is the widget size, exactly like
// the legacy w2 subwindow geometry.
class AxesView : public QOpenGLWidget
{
  Q_OBJECT
public:
  explicit AxesView(QWidget *parent = nullptr);

protected:
  void paintGL() override;
};

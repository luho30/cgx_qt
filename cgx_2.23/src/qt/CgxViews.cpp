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

#include "CgxViews.h"
#include "glue.h"

#include <GL/glut_cgx.h>

#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QWheelEvent>

// GLUT wheel buttons are a cgx.c-local convention, not in glut_cgx.h:
//   #define GLUT_WEEL_UP 3 / GLUT_WEEL_DOWN 4  (matches X button 4/5)
enum
{
  CgxWeelUp = 3,
  CgxWeelDown = 4
};

namespace
{
// Qt::Key -> GLUT special code (GLUT_KEY_F1..F12 = 1..12, arrows = 100..108).
bool qtToGlutSpecial(int qtKey, int &glutKey)
{
  if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
  {
    glutKey = qtKey - Qt::Key_F1 + 1;
    return true;
  }
  switch (qtKey)
  {
  case Qt::Key_Left:
    glutKey = 100;
    return true;
  case Qt::Key_Up:
    glutKey = 101;
    return true;
  case Qt::Key_Right:
    glutKey = 102;
    return true;
  case Qt::Key_Down:
    glutKey = 103;
    return true;
  case Qt::Key_PageUp:
    glutKey = 104;
    return true;
  case Qt::Key_PageDown:
    glutKey = 105;
    return true;
  case Qt::Key_Home:
    glutKey = 106;
    return true;
  case Qt::Key_End:
    glutKey = 107;
    return true;
  case Qt::Key_Insert:
    glutKey = 108;
    return true;
  default:
    return false;
  }
}

// QKeyEvent -> legacy ASCII code. Prefers event text (respects Shift), with
// explicit fallbacks for non-printing keys whose text() is empty.
bool qtToGlutAscii(QKeyEvent *event, unsigned char &ascii)
{
  const QString text = event->text();
  if (text.length() == 1)
  {
    ascii = (unsigned char)text.at(0).toLatin1();
    return true;
  }
  switch (event->key())
  {
  case Qt::Key_Escape:
    ascii = 27;
    return true;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    ascii = 13;
    return true;
  case Qt::Key_Tab:
    ascii = 9;
    return true;
  case Qt::Key_Backspace:
    ascii = 8;
    return true;
  case Qt::Key_Delete:
    ascii = 127;
    return true;
  case Qt::Key_Space:
    ascii = 32;
    return true;
  default:
    return false;
  }
}

int qtToGlutButton(Qt::MouseButton button, bool &known)
{
  known = true;
  switch (button)
  {
  case Qt::LeftButton:
    return 0; // GLUT_LEFT_BUTTON
  case Qt::MiddleButton:
    return 1; // GLUT_MIDDLE_BUTTON
  case Qt::RightButton:
    return 2; // GLUT_RIGHT_BUTTON
  default:
    known = false;
    return -1;
  }
}

// Legacy callbacks receive the mouse position with key events; GLUT uses
// top-left-origin client coords, identical to Qt widget coords.
QPoint keyEventPos(const QOpenGLWidget *view)
{
  return view->mapFromGlobal(QCursor::pos());
}

void forwardKeyPress(const QOpenGLWidget *view, int win, QKeyEvent *event)
{
  // Legacy GLUT has no Alt concept (glutGetModifiers is unused in cgx), so an
  // Alt-modified key must never reach the parser — in particular Alt+C (the
  // command-line toggle) must not inject a 'c' into the keystroke buffer.
  if (event->modifiers() & Qt::AltModifier)
    return;
  int special;
  if (qtToGlutSpecial(event->key(), special))
  {
    const QPoint p = keyEventPos(view);
    cgxForwardSpecial(win, special, p.x(), p.y());
    return;
  }
  unsigned char ascii;
  if (qtToGlutAscii(event, ascii))
  {
    const QPoint p = keyEventPos(view);
    cgxForwardKey(win, ascii, p.x(), p.y());
  }
}
} // namespace

// Legacy globals owned by cgx.c (probed for paint diagnostics only).
extern char drawMode;

MenuView::MenuView(QWidget *parent) : QOpenGLWidget(parent)
{
  setAttribute(Qt::WA_AlwaysStackOnTop);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
}

GraphicsView::GraphicsView(QWidget *parent) : QOpenGLWidget(parent)
{
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
}

void MenuView::paintGL()
{
  cgxCallMenuDisplay();
}

AxesView::AxesView(QWidget *parent) : QOpenGLWidget(parent)
{
  setAttribute(Qt::WA_AlwaysStackOnTop);
  // No input: the gizmo is display-only (legacy w2 had no callbacks).
  setFocusPolicy(Qt::NoFocus);
}

void AxesView::paintGL()
{
  cgxCallAxesDisplay();
}

void GraphicsView::paintGL()
{
  cgxCallGraphicsDisplay();
}

void MenuView::mousePressEvent(QMouseEvent *event)
{
  bool known = false;
  const int b = qtToGlutButton(event->button(), known);
  if (!known)
    return;
  // Seamless 3D interaction: clicking/dragging over the transparent legend
  // rotates/pans/zooms the underlying GraphicsView (w1).
  cgxForwardMouse(cgxGraphicsId(), b, 0 /*GLUT_DOWN*/, event->pos().x(), event->pos().y());
}

void MenuView::mouseReleaseEvent(QMouseEvent *event)
{
  bool known = false;
  const int b = qtToGlutButton(event->button(), known);
  if (!known)
    return;
  cgxForwardMouse(cgxGraphicsId(), b, 1 /*GLUT_UP*/, event->pos().x(), event->pos().y());
}

void MenuView::mouseMoveEvent(QMouseEvent *event)
{
  const bool drag = (event->buttons() != Qt::NoButton);
  cgxForwardMotion(cgxGraphicsId(), event->pos().x(), event->pos().y(), drag ? 0 : 1);
}

void MenuView::wheelEvent(QWheelEvent *event)
{
  int delta = event->angleDelta().y();
  if (!delta)
    delta = event->pixelDelta().y();
  if (!delta)
    return;
  const int b = (delta > 0) ? CgxWeelUp : CgxWeelDown;
  const QPoint p = event->position().toPoint();
  cgxForwardMouse(cgxGraphicsId(), b, 0 /*GLUT_DOWN*/, p.x(), p.y());
}

void MenuView::keyPressEvent(QKeyEvent *event)
{
  forwardKeyPress(this, cgxGraphicsId(), event);
}

void GraphicsView::mousePressEvent(QMouseEvent *event)
{
  bool known = false;
  const int b = qtToGlutButton(event->button(), known);
  if (!known)
    return;
  // w1 has no menu attached in legacy: all buttons reach MouseState.
  cgxForwardMouse(cgxGraphicsId(), b, 0 /*GLUT_DOWN*/, event->pos().x(), event->pos().y());
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
  bool known = false;
  const int b = qtToGlutButton(event->button(), known);
  if (!known)
    return;
  cgxForwardMouse(cgxGraphicsId(), b, 1 /*GLUT_UP*/, event->pos().x(), event->pos().y());
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
  const bool drag = (event->buttons() != Qt::NoButton);
  cgxForwardMotion(cgxGraphicsId(), event->pos().x(), event->pos().y(), drag ? 0 : 1);
}

void GraphicsView::wheelEvent(QWheelEvent *event)
{
  int delta = event->angleDelta().y();
  if (!delta)
    delta = event->pixelDelta().y();
  if (!delta)
    return;
  const int b = (delta > 0) ? CgxWeelUp : CgxWeelDown;
  const QPoint p = event->position().toPoint();
  cgxForwardMouse(cgxGraphicsId(), b, 0 /*GLUT_DOWN*/, p.x(), p.y());
}

void GraphicsView::keyPressEvent(QKeyEvent *event)
{
  forwardKeyPress(this, cgxGraphicsId(), event);
}

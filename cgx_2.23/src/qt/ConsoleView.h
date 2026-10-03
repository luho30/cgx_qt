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
// Step 11: console panel showing the last lines of cgx's stdout (see
// ConsoleCapture.h). Exactly kVisibleLines text lines high, own vertical
// scrollbar, semi-transparent (~70% opaque = 30% transparent) background that follows cgx's
// white/black background mode. Shown/hidden together with the command line.
//
// Why a QOpenGLWidget painted with QPainter instead of a QPlainTextEdit:
// the legend and axes overlays are stacked-on-top QOpenGLWidgets (the only
// way to get true transparency over the 3D QOpenGLWidget here; ordinary
// widgets cannot be placed above them, and a non-stacked QOpenGLWidget is
// composited as an opaque black box). The console must sit above them, so it
// has to be a stacked-on-top QOpenGLWidget too (raised last = on top).
// Hence text layout, wrapping and the scrollbar are done here.
//
// Selection / copy: drag with the left mouse button to select text, double
// click selects a word (a run of non-blank characters), triple click a line,
// Ctrl+A everything. Ctrl+C or the right-click menu copy to the clipboard;
// finishing a selection also fills the X11 primary selection (middle-click
// paste elsewhere). Copied text is rebuilt from the LOGICAL lines, so a line
// that was only wrapped for display pastes as one line. The panel is
// read-only. New output clears the selection (the indices would be stale).
//
// Focus: clicking the text gives the panel keyboard focus (needed for Ctrl+C).
// Keys it does not use are forwarded to the focus-return widget (the 3D view,
// see setFocusReturn) so cgx hotkeys keep working; Esc clears the selection
// and moves the focus back. The wheel and the scrollbar scroll the panel;
// clicks on the panel never reach the 3D view below it.
#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPoint>

#include <deque>
#include <vector>

class QTimer;

class ConsoleView : public QOpenGLWidget
{
public:
  static const int kVisibleLines = 5;
  static const int kMaxLines = 5000; // logical lines kept
  static const int kBackgroundAlpha = 178; // ~70% of 255 (30% transparent)
  static const int kScrollBarWidth = 12;

  explicit ConsoleView(QWidget *parent = nullptr);

  // Terminal-like append: \n starts a new line, \b erases, a lone \r clears
  // the current line (progress output), \t becomes spaces.
  void appendChunk(const QString &text);

  // Height that shows exactly kVisibleLines lines (+ border/margins).
  int panelHeight() const;

  // Pull new text from ConsoleCapture and re-evaluate the colour theme.
  void poll();

  // --- state access (selftest / debugging) ---
  QString plainText() const; // all logical lines joined with \n
  int visualLineCount() const { return (int)m_vis.size(); }
  int scrollValue() const { return m_scroll; }
  int scrollMax() const;
  void setScrollValue(int v);
  bool isWhiteTheme() const { return m_white; }

  // --- selection / copy ---
  // Widget the focus goes back to on Esc and that receives unused keys.
  void setFocusReturn(QWidget *w) { m_focusReturn = w; }
  bool hasSelection() const { return m_hasSel; }
  QString selectedText() const; // logical-line text of the selection
  void selectAll();
  void clearSelection();
  // Selection by (visual line, column) over the wrapped lines; ends inclusive
  // of the character before the end column.
  void setSelection(int line0, int col0, int line1, int col1);
  bool copySelection() const; // false if nothing selected
  void copyAll() const;
  // Widget position of the left edge / vertical middle of column `col` in
  // visual line `visLine` (must be on screen) — for tests and hit-testing.
  QPoint cellPos(int visLine, int col) const;
  int charAdvance() const;

protected:
  void paintGL() override;
  void resizeGL(int w, int h) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  int charsPerLine() const;
  int lineHeight() const;
  void wrapLogical(const QString &s, std::vector<QString> &out) const;
  void rebuildVisual();
  void relayoutLast(); // re-wrap the last (possibly partial) logical line
  void trimOldest();
  QRect scrollTrack() const;
  QRect scrollThumb() const;

  struct Pos
  {
    int line = 0; // visual line index into m_vis
    int col = 0; // 0..length of that line
    bool operator<(const Pos &o) const { return line != o.line ? line < o.line : col < o.col; }
    bool operator==(const Pos &o) const { return line == o.line && col == o.col; }
  };
  Pos hitTest(const QPoint &p) const; // clamps to the visible text
  void normalizedSelection(Pos &a, Pos &b) const;
  void publishSelection() const; // X11 primary selection
  void showContextMenu(const QPoint &globalPos);
  void setClipboardText(const QString &text, bool primaryOnly) const;

  QTimer *m_timer;
  std::deque<QString> m_lines; // logical lines; last one is the live one
  std::deque<int> m_wrapCount; // visual lines per logical line
  std::vector<QString> m_vis; // wrapped lines, top to bottom
  int m_wrapWidthChars = 0;
  int m_scroll = 0;
  bool m_pendingCR = false;
  bool m_white = true;
  bool m_dragging = false;
  int m_dragOffset = 0;

  QWidget *m_focusReturn = nullptr;
  bool m_hasSel = false;
  bool m_selecting = false; // text drag in progress
  Pos m_selA, m_selB; // anchor and moving end
  QElapsedTimer m_lastDouble; // for triple click detection
  QPoint m_lastDoublePos;
};

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

#include "ConsoleView.h"
#include "ConsoleCapture.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cstdio>

// Legacy globals owned by cgx.c.
extern "C" double backgrndcol_rgb[4];

namespace
{
const int kMargin = 3; // text inset
}

ConsoleView::ConsoleView(QWidget *parent) : QOpenGLWidget(parent)
{
  // Stacked-on-top GL widgets are composited above everything else and keep
  // true alpha (same mechanism as the legend and the axes overlays).
  setAttribute(Qt::WA_AlwaysStackOnTop);
  // Keyboard focus only on click (Ctrl+C / Ctrl+A / Esc); see header comment.
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(false);

  QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  f.setPointSize(9);
  setFont(f);
  setFixedHeight(panelHeight());

  m_lines.push_back(QString());
  m_wrapCount.push_back(1);
  m_vis.push_back(QString());

  // 50 ms poll: cheap (one mutex + swap) and responsive enough for a console.
  // Also flushes stdout so prompts without a trailing newline become visible.
  m_timer = new QTimer(this);
  QObject::connect(m_timer, &QTimer::timeout, [this]() {
    std::fflush(stdout);
    poll();
  });
  m_timer->start(50);
}

int ConsoleView::lineHeight() const
{
  return QFontMetrics(font()).lineSpacing();
}

int ConsoleView::panelHeight() const
{
  return kVisibleLines * lineHeight() + 2 * kMargin + 2; // +2: 1px border
}

int ConsoleView::charsPerLine() const
{
  const int adv = std::max(1, QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')));
  const int textW = width() - kScrollBarWidth - 2 * kMargin - 2;
  return std::max(8, textW / adv);
}

int ConsoleView::scrollMax() const
{
  return std::max(0, (int)m_vis.size() - kVisibleLines);
}

void ConsoleView::setScrollValue(int v)
{
  v = std::clamp(v, 0, scrollMax());
  if (v != m_scroll)
  {
    m_scroll = v;
    update();
  }
}

void ConsoleView::wrapLogical(const QString &s, std::vector<QString> &out) const
{
  const int n = std::max(1, m_wrapWidthChars);
  if (s.isEmpty())
  {
    out.push_back(QString());
    return;
  }
  for (int i = 0; i < s.size(); i += n)
    out.push_back(s.mid(i, n));
}

void ConsoleView::rebuildVisual()
{
  m_hasSel = false; // visual indices change
  m_selecting = false;
  m_wrapWidthChars = charsPerLine();
  m_vis.clear();
  m_wrapCount.clear();
  for (const QString &l : m_lines)
  {
    const size_t before = m_vis.size();
    wrapLogical(l, m_vis);
    m_wrapCount.push_back((int)(m_vis.size() - before));
  }
  m_scroll = std::clamp(m_scroll, 0, scrollMax());
}

void ConsoleView::relayoutLast()
{
  // Re-wrap only the last logical line (the one being typed/printed).
  const int oldCount = m_wrapCount.back();
  m_vis.resize(m_vis.size() - (size_t)oldCount);
  const size_t before = m_vis.size();
  wrapLogical(m_lines.back(), m_vis);
  m_wrapCount.back() = (int)(m_vis.size() - before);
}

void ConsoleView::trimOldest()
{
  while ((int)m_lines.size() > kMaxLines)
  {
    const int drop = m_wrapCount.front();
    m_vis.erase(m_vis.begin(), m_vis.begin() + drop);
    m_lines.pop_front();
    m_wrapCount.pop_front();
    m_scroll = std::max(0, m_scroll - drop);
  }
}

void ConsoleView::appendChunk(const QString &text)
{
  if (m_wrapWidthChars == 0)
    m_wrapWidthChars = charsPerLine();
  const bool follow = m_scroll >= scrollMax();
  m_hasSel = false; // new output invalidates (trim/rewrap shifts) the indices
  m_selecting = false;

  bool lastDirty = false;
  auto finishLast = [&]() {
    if (lastDirty)
    {
      relayoutLast();
      lastDirty = false;
    }
  };
  for (const QChar ch : text)
  {
    const ushort c = ch.unicode();
    if (m_pendingCR && c != '\n')
    {
      m_lines.back().clear(); // lone CR: the legacy code redraws the line
      lastDirty = true;
    }
    m_pendingCR = false;
    if (c == '\n')
    {
      finishLast();
      m_lines.push_back(QString());
      m_wrapCount.push_back(1);
      m_vis.push_back(QString());
    }
    else if (c == '\r')
    {
      m_pendingCR = true;
    }
    else if (c == '\b')
    {
      if (!m_lines.back().isEmpty())
      {
        m_lines.back().chop(1);
        lastDirty = true;
      }
    }
    else if (c == '\t')
    {
      m_lines.back() += QStringLiteral("    ");
      lastDirty = true;
    }
    else
    {
      m_lines.back() += ch;
      lastDirty = true;
    }
  }
  finishLast();
  trimOldest();
  m_scroll = follow ? scrollMax() : std::clamp(m_scroll, 0, scrollMax());
  update();
}

void ConsoleView::poll()
{
  // cgx background mode: white (1,1,1) or black (0,0,0).
  const bool white = backgrndcol_rgb[0] > 0.5;
  if (white != m_white)
  {
    m_white = white;
    update();
  }
  const QString text = cgxconsole::takeNew();
  if (!text.isEmpty())
    appendChunk(text);
}

QString ConsoleView::plainText() const
{
  QStringList l;
  for (const QString &s : m_lines)
    l << s;
  return l.join(QLatin1Char('\n'));
}

void ConsoleView::resizeGL(int, int)
{
  const int chars = charsPerLine();
  if (chars != m_wrapWidthChars)
  {
    const bool follow = m_scroll >= scrollMax();
    rebuildVisual();
    if (follow)
      m_scroll = scrollMax();
  }
}

void ConsoleView::showEvent(QShowEvent *event)
{
  QOpenGLWidget::showEvent(event);
  m_scroll = scrollMax();
}

QRect ConsoleView::scrollTrack() const
{
  return QRect(width() - kScrollBarWidth - 1, 1, kScrollBarWidth, height() - 2);
}

QRect ConsoleView::scrollThumb() const
{
  const QRect t = scrollTrack();
  const int total = std::max<int>((int)m_vis.size(), kVisibleLines);
  const int thumbH = std::max(14, t.height() * kVisibleLines / total);
  const int range = t.height() - thumbH;
  const int smax = scrollMax();
  const int y = smax > 0 ? t.top() + range * m_scroll / smax : t.top();
  return QRect(t.left() + 1, y, t.width() - 2, thumbH);
}

void ConsoleView::paintGL()
{
  // The FBO is shared-context territory: start from a fully transparent
  // surface (premultiplied), then QPainter composes the panel on it.
  if (QOpenGLContext *ctx = QOpenGLContext::currentContext())
  {
    QOpenGLFunctions *f = ctx->functions();
    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);
  }
  QPainter p(this);
  p.setRenderHint(QPainter::TextAntialiasing, true);
  const QColor bg = m_white ? QColor(255, 255, 255, kBackgroundAlpha)
                            : QColor(0, 0, 0, kBackgroundAlpha);
  const QColor fg = m_white ? QColor(0, 0, 0) : QColor(255, 255, 255);
  const QColor edge = m_white ? QColor(0, 0, 0, 90) : QColor(255, 255, 255, 90);

  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.fillRect(rect(), bg);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setPen(edge);
  p.drawRect(rect().adjusted(0, 0, -1, -1));

  p.setFont(font());
  p.setPen(fg);
  const QFontMetrics fm(font());
  const int lh = lineHeight();
  const int x = 1 + kMargin;
  int y = 1 + kMargin + fm.ascent();

  if (m_hasSel)
  {
    Pos a, b;
    normalizedSelection(a, b);
    const QColor accent(51, 153, 255, 120);
    for (int i = 0; i < kVisibleLines; ++i)
    {
      const int idx = m_scroll + i;
      if (idx >= (int)m_vis.size())
        break;
      if (idx < a.line || idx > b.line)
        continue;
      const QString &ln = m_vis[(size_t)idx];
      const int c0 = (idx == a.line) ? a.col : 0;
      const int c1 = (idx == b.line) ? b.col : ln.size();
      if (c1 <= c0)
        continue;
      // Measured text widths, not 'M'-advance multiples: the highlight must
      // cover the glyph ink exactly as drawText renders it.
      const int x0 = x + fm.horizontalAdvance(ln.left(c0));
      const int w = fm.horizontalAdvance(ln.mid(c0, c1 - c0));
      p.fillRect(QRect(x0, 1 + kMargin + i * lh, w, lh), accent);
    }
  }
  for (int i = 0; i < kVisibleLines; ++i)
  {
    const int idx = m_scroll + i;
    if (idx >= (int)m_vis.size())
      break;
    p.drawText(x, y + i * lh, m_vis[(size_t)idx]);
  }

  // scrollbar: always visible
  const QRect track = scrollTrack();
  p.fillRect(track, m_white ? QColor(0, 0, 0, 28) : QColor(255, 255, 255, 36));
  p.fillRect(scrollThumb(), m_white ? QColor(0, 0, 0, 120) : QColor(255, 255, 255, 140));
}

int ConsoleView::charAdvance() const
{
  return std::max(1, QFontMetrics(font()).horizontalAdvance(QLatin1Char('M')));
}

QPoint ConsoleView::cellPos(int visLine, int col) const
{
  // Left edge of column `col`: measured prefix width, consistent with paint.
  if (m_vis.empty())
    return QPoint(1 + kMargin, 1 + kMargin + (visLine - m_scroll) * lineHeight() +
                                   lineHeight() / 2);
  const QString &ln = m_vis[(size_t)std::clamp(visLine, 0, (int)m_vis.size() - 1)];
  const int c = std::clamp(col, 0, (int)ln.size());
  return QPoint(1 + kMargin + QFontMetrics(font()).horizontalAdvance(ln.left(c)),
                1 + kMargin + (visLine - m_scroll) * lineHeight() + lineHeight() / 2);
}

ConsoleView::Pos ConsoleView::hitTest(const QPoint &p) const
{
  Pos r;
  if (m_vis.empty())
    return r;
  int row = (p.y() - (1 + kMargin)) / lineHeight();
  row = std::clamp(row, 0, kVisibleLines - 1);
  r.line = std::clamp(m_scroll + row, 0, (int)m_vis.size() - 1);
  // Nearest character boundary by cumulative advance, not 'M' multiples.
  const QString &ln = m_vis[(size_t)r.line];
  const QFontMetrics fm(font());
  const int rel = p.x() - (1 + kMargin);
  int best = 0, bestDist = abs(rel);
  int adv = 0;
  for (int c = 1; c <= ln.size(); ++c)
  {
    adv = fm.horizontalAdvance(ln.left(c));
    const int d = abs(rel - adv);
    if (d < bestDist)
    {
      bestDist = d;
      best = c;
    }
  }
  r.col = best;
  return r;
}

void ConsoleView::normalizedSelection(Pos &a, Pos &b) const
{
  a = m_selA;
  b = m_selB;
  if (b < a)
    std::swap(a, b);
}

void ConsoleView::clearSelection()
{
  if (m_hasSel)
  {
    m_hasSel = false;
    update();
  }
  m_selecting = false;
}

void ConsoleView::setSelection(int line0, int col0, int line1, int col1)
{
  if (m_vis.empty())
    return;
  auto clampPos = [&](int l, int c) {
    Pos r;
    r.line = std::clamp(l, 0, (int)m_vis.size() - 1);
    r.col = std::clamp(c, 0, (int)m_vis[(size_t)r.line].size());
    return r;
  };
  m_selA = clampPos(line0, col0);
  m_selB = clampPos(line1, col1);
  m_hasSel = !(m_selA == m_selB);
  update();
}

void ConsoleView::selectAll()
{
  if (m_vis.empty())
    return;
  const int last = (int)m_vis.size() - 1;
  setSelection(0, 0, last, m_vis[(size_t)last].size());
}

QString ConsoleView::selectedText() const
{
  if (!m_hasSel)
    return QString();
  Pos a, b;
  normalizedSelection(a, b);
  // A visual line is a continuation when it is not the first display line of
  // its logical line; consecutive pieces of one logical line are re-joined
  // without a newline so wrapping never leaks into the copied text.
  std::vector<char> cont(m_vis.size(), 0);
  {
    size_t v = 0;
    for (int n : m_wrapCount)
      for (int k = 0; k < n && v < cont.size(); ++k, ++v)
        cont[v] = (k > 0);
  }
  QString out;
  for (int v = a.line; v <= b.line && v < (int)m_vis.size(); ++v)
  {
    const QString &l = m_vis[(size_t)v];
    const int c0 = (v == a.line) ? a.col : 0;
    const int c1 = (v == b.line) ? b.col : l.size();
    if (v > a.line && !cont[(size_t)v])
      out += QLatin1Char('\n');
    if (c1 > c0)
      out += l.mid(c0, c1 - c0);
  }
  return out;
}

void ConsoleView::setClipboardText(const QString &text, bool primaryOnly) const
{
  QClipboard *cb = QGuiApplication::clipboard();
  if (!cb)
    return;
  if (!primaryOnly)
    cb->setText(text, QClipboard::Clipboard);
  if (cb->supportsSelection())
    cb->setText(text, QClipboard::Selection); // middle-click paste on X11
}

void ConsoleView::publishSelection() const
{
  if (m_hasSel)
    setClipboardText(selectedText(), true);
}

bool ConsoleView::copySelection() const
{
  if (!m_hasSel)
    return false;
  setClipboardText(selectedText(), false);
  return true;
}

void ConsoleView::copyAll() const
{
  setClipboardText(plainText(), false);
}

void ConsoleView::showContextMenu(const QPoint &globalPos)
{
  QMenu menu;
  QAction *copySel = menu.addAction(QStringLiteral("Copy selection"));
  copySel->setEnabled(m_hasSel);
  QAction *copyAllA = menu.addAction(QStringLiteral("Copy all"));
  QAction *selAll = menu.addAction(QStringLiteral("Select all"));
  QObject::connect(copySel, &QAction::triggered, [this]() { copySelection(); });
  QObject::connect(copyAllA, &QAction::triggered, [this]() { copyAll(); });
  QObject::connect(selAll, &QAction::triggered, [this]() { selectAll(); });
  menu.exec(globalPos);
}

void ConsoleView::mousePressEvent(QMouseEvent *event)
{
  const QPoint pos = event->position().toPoint();
  if (event->button() == Qt::LeftButton && scrollTrack().contains(pos))
  {
    const QRect thumb = scrollThumb();
    if (thumb.contains(pos))
    {
      m_dragging = true;
      m_dragOffset = pos.y() - thumb.top();
    }
    else
    {
      setScrollValue(m_scroll + (pos.y() < thumb.top() ? -kVisibleLines : kVisibleLines));
    }
  }
  else if (event->button() == Qt::RightButton)
  {
    showContextMenu(event->globalPosition().toPoint());
  }
  else if (event->button() == Qt::LeftButton)
  {
    setFocus(Qt::MouseFocusReason); // Ctrl+C needs it
    // Triple click = a quick press after a double click at the same spot.
    if (m_lastDouble.isValid() && m_lastDouble.elapsed() < QApplication::doubleClickInterval() &&
        (pos - m_lastDoublePos).manhattanLength() < 6)
    {
      const Pos h = hitTest(pos);
      setSelection(h.line, 0, h.line, m_vis[(size_t)h.line].size());
      publishSelection();
      m_lastDouble.invalidate();
    }
    else
    {
      m_selA = m_selB = hitTest(pos);
      m_hasSel = false;
      m_selecting = true;
      update();
    }
  }
  event->accept(); // swallow: do not reach the 3D view
}

void ConsoleView::mouseDoubleClickEvent(QMouseEvent *event)
{
  const QPoint pos = event->position().toPoint();
  if (event->button() == Qt::LeftButton && !scrollTrack().contains(pos) && !m_vis.empty())
  {
    // word = run of non-blank characters around the clicked character
    const Pos h = hitTest(pos);
    const QString &l = m_vis[(size_t)h.line];
    int i = std::min(h.col, (int)l.size() - 1);
    if (i >= 0 && !l.at(i).isSpace())
    {
      int c0 = i, c1 = i + 1;
      while (c0 > 0 && !l.at(c0 - 1).isSpace())
        --c0;
      while (c1 < l.size() && !l.at(c1).isSpace())
        ++c1;
      setSelection(h.line, c0, h.line, c1);
      publishSelection();
    }
    m_selecting = false;
    m_lastDouble.start();
    m_lastDoublePos = pos;
  }
  event->accept();
}

void ConsoleView::mouseMoveEvent(QMouseEvent *event)
{
  if (m_dragging)
  {
    const QRect t = scrollTrack();
    const int thumbH = scrollThumb().height();
    const int range = std::max(1, t.height() - thumbH);
    const int y = event->position().toPoint().y() - m_dragOffset - t.top();
    setScrollValue((int)((double)y * scrollMax() / range + 0.5));
  }
  else if (m_selecting && (event->buttons() & Qt::LeftButton))
  {
    m_selB = hitTest(event->position().toPoint());
    m_hasSel = !(m_selA == m_selB);
    update();
  }
  event->accept();
}

void ConsoleView::mouseReleaseEvent(QMouseEvent *event)
{
  if (m_selecting)
    publishSelection(); // X11 primary selection, like a terminal
  m_selecting = false;
  m_dragging = false;
  event->accept();
}

void ConsoleView::keyPressEvent(QKeyEvent *event)
{
  if (event->matches(QKeySequence::Copy))
  {
    if (!copySelection())
      copyAll(); // nothing selected: Ctrl+C copies the whole console
    event->accept();
    return;
  }
  if (event->matches(QKeySequence::SelectAll))
  {
    selectAll();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Escape)
  {
    clearSelection();
    if (m_focusReturn)
      m_focusReturn->setFocus(Qt::OtherFocusReason);
    event->accept();
    return;
  }
  // Alt-modified keys never go to the legacy parser (no Alt concept there);
  // the Alt+C toggle is handled by the window shortcut, not by forwarding.
  if (event->modifiers() & Qt::AltModifier)
  {
    event->accept();
    return;
  }
  // Everything else is a cgx hotkey / command character: hand it to the 3D
  // view so a click into the console does not disable the keyboard.
  if (m_focusReturn)
  {
    QApplication::sendEvent(m_focusReturn, event);
    return;
  }
  QOpenGLWidget::keyPressEvent(event);
}

void ConsoleView::wheelEvent(QWheelEvent *event)
{
  int delta = event->angleDelta().y();
  if (!delta)
    delta = event->pixelDelta().y();
  if (delta)
    setScrollValue(m_scroll - (delta > 0 ? 3 : -3) * std::max(1, std::abs(delta) / 120));
  event->accept();
}

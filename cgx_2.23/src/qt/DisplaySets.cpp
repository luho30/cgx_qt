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

#include "DisplaySets.h"
#include "DisplaySetsBridge.h"

#include <QApplication>
#include <QColor>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QPixmap>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyle>
#include <QScrollBar>
#include <QTableWidget>
#include <QTimer>
#include <QTableWidgetItem>
#include <QWidgetAction>

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace
{
const char *kTypeTips[CGX_DS_NTYPES] = {
  "Nodes (n)",           "Elements (e)", "Faces (f)",           "Points (p)", "Lines (l)",
  "Surfaces (s)",        "Bodies (b)",   "Nurbs surfaces (S)",  "Nurbs lines (L)"
};
// First non-empty type wins when a hidden set is switched on without a
// remembered selection: elements, faces, surfaces, bodies, lines, points,
// nodes, nurbs.
const int kDefaultOrder[CGX_DS_NTYPES] = { 1, 2, 5, 6, 4, 3, 0, 7, 8 };
const int kEntityColW = 30;
const int kMaxNameW = 280;

DisplaySetsMenu *s_menu = nullptr;

QIcon colorIcon(int setIdx)
{
  float r, g, b;
  cgxDsSetColor(setIdx, &r, &g, &b);
  QPixmap pm(12, 12);
  pm.fill(QColor::fromRgbF(r, g, b));
  return QIcon(pm);
}
} // namespace

DisplaySetsMenu *cgxDisplaySetsMenu(QWidget *parent)
{
  if (!s_menu)
    s_menu = new DisplaySetsMenu(parent);
  return s_menu;
}

DisplaySetsMenu::DisplaySetsMenu(QWidget *parent) : QMenu(parent)
{
  setTitle(QStringLiteral("Display Sets"));
  m_table = new QTableWidget(this);
  m_table->setColumnCount(1 + CGX_DS_NTYPES);
  m_table->setSelectionMode(QAbstractItemView::NoSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setFocusPolicy(Qt::NoFocus);
  m_table->setShowGrid(false);
  m_table->setFrameShape(QFrame::NoFrame);
  m_table->verticalHeader()->hide();
  m_table->verticalHeader()->setDefaultSectionSize(QFontMetrics(font()).height() + 8);
  m_table->horizontalHeader()->setSectionsClickable(false);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); // right-hand scrollbar when needed
  m_table->setMouseTracking(true);
  connect(m_table, &QTableWidget::cellClicked, this, [this](int r, int c) { onCellClicked(r, c); });

  auto *act = new QWidgetAction(this);
  act->setDefaultWidget(m_table);
  addAction(act);

  // The popup must always show the live legacy state (sets can be created or
  // plotted from the command line at any time).
  connect(this, &QMenu::aboutToShow, this, [this]() {
    rebuild();
    m_timer->start();
  });
  connect(this, &QMenu::aboutToHide, this, [this]() { m_timer->stop(); });

  // While open the popup also follows changes made behind its back (scripts,
  // idle work, finished mesh threads): seta/del do not trigger a redraw, so
  // a cheap signature poll is the one hook that sees every change.
  m_timer = new QTimer(this);
  m_timer->setInterval(150);
  connect(m_timer, &QTimer::timeout, this, [this]() { checkForChanges(); });
}

void DisplaySetsMenu::checkForChanges()
{
  if (cgxDsSignature() == m_signature)
    return;
  const int scroll = m_table->verticalScrollBar()->value();
  rebuild();
  m_table->verticalScrollBar()->setValue(scroll);
}

int DisplaySetsMenu::maskFor(int setIdx) const
{
  const int shown = cgxDsDisplayedMask(setIdx);
  if (shown)
    return shown;
  auto it = m_selected.find(QString::fromUtf8(cgxDsSetName(setIdx)));
  if (it != m_selected.end() && it->second != 0) // 0 = nothing remembered -> default
    return it->second;
  for (int k = 0; k < CGX_DS_NTYPES; k++)
    if (cgxDsEntityCount(setIdx, kDefaultOrder[k]) > 0)
      return 1 << kDefaultOrder[k];
  return 0;
}

void DisplaySetsMenu::rebuild()
{
  const QSignalBlocker block(m_table);
  m_signature = cgxDsSignature();
  // forget remembered selections of sets that no longer exist (a recreated
  // set of the same name must start from the default selection)
  for (auto it = m_selected.begin(); it != m_selected.end();)
  {
    bool alive = false;
    for (int i = 0; i < cgxDsSetSlots() && !alive; i++)
      alive = cgxDsSetValid(i) && QString::fromUtf8(cgxDsSetName(i)) == it->first;
    it = alive ? std::next(it) : m_selected.erase(it);
  }
  m_table->clear();
  m_table->setRowCount(0);

  QStringList hdr;
  hdr << QStringLiteral("Set");
  for (int t = 0; t < CGX_DS_NTYPES; t++)
    hdr << QString(QLatin1Char(cgxDsTypeLetter(t)));
  m_table->setHorizontalHeaderLabels(hdr);
  for (int t = 0; t < CGX_DS_NTYPES; t++)
    if (QTableWidgetItem *h = m_table->horizontalHeaderItem(1 + t))
      h->setToolTip(QString::fromLatin1(kTypeTips[t]));

  const QFontMetrics fm(font());
  int nameW = fm.horizontalAdvance(QStringLiteral("Set"));
  int rows = 0;
  for (int i = 0; i < cgxDsSetSlots(); i++)
  {
    if (!cgxDsSetValid(i))
      continue;
    m_table->setRowCount(rows + 1);
    const QString name = QString::fromUtf8(cgxDsSetName(i));
    nameW = std::max(nameW, fm.horizontalAdvance(name));

    auto *n = new QTableWidgetItem(name);
    n->setData(Qt::UserRole, i);
    n->setFlags(Qt::ItemIsEnabled);
    n->setIcon(colorIcon(i));
    m_table->setItem(rows, 0, n);
    for (int t = 0; t < CGX_DS_NTYPES; t++)
    {
      auto *it = new QTableWidgetItem();
      const int cnt = cgxDsEntityCount(i, t);
      if (cnt > 0)
      {
        it->setFlags(Qt::ItemIsEnabled);
        it->setToolTip(QStringLiteral("%1: %2").arg(QString::fromLatin1(kTypeTips[t])).arg(cnt));
      }
      else
      {
        it->setFlags(Qt::NoItemFlags); // greyed out: the set has none of these
        it->setToolTip(QStringLiteral("no %1").arg(QString::fromLatin1(kTypeTips[t]).toLower()));
      }
      it->setTextAlignment(Qt::AlignCenter);
      m_table->setItem(rows, 1 + t, it);
    }
    rows++;
  }
  if (rows == 0)
  {
    m_table->setRowCount(1);
    auto *it = new QTableWidgetItem(QStringLiteral("(no sets)"));
    it->setFlags(Qt::NoItemFlags);
    m_table->setItem(0, 0, it);
  }

  m_table->setColumnWidth(0, std::min(nameW + 16 + 40, kMaxNameW)); // icon + checkbox + text
  for (int t = 0; t < CGX_DS_NTYPES; t++)
    m_table->setColumnWidth(1 + t, kEntityColW);
  fitSize();
  refreshStates();
}

void DisplaySetsMenu::refreshStates()
{
  const QSignalBlocker block(m_table);
  for (int r = 0; r < m_table->rowCount(); r++)
  {
    QTableWidgetItem *n = m_table->item(r, 0);
    if (!n || !(n->flags() & Qt::ItemIsEnabled))
      continue;
    const int idx = n->data(Qt::UserRole).toInt();
    const int shown = cgxDsDisplayedMask(idx);
    const int mask = maskFor(idx);
    n->setCheckState(shown ? Qt::Checked : Qt::Unchecked);
    for (int t = 0; t < CGX_DS_NTYPES; t++)
    {
      QTableWidgetItem *it = m_table->item(r, 1 + t);
      if (it && (it->flags() & Qt::ItemIsEnabled))
        it->setCheckState((mask & (1 << t)) ? Qt::Checked : Qt::Unchecked);
    }
  }
}

void DisplaySetsMenu::fitSize()
{
  const int rowH = m_table->verticalHeader()->defaultSectionSize();
  const int headH = m_table->horizontalHeader()->height() > 0 ? m_table->horizontalHeader()->height()
                                                              : rowH;
  int w = 2;
  for (int c = 0; c < m_table->columnCount(); c++)
    w += m_table->columnWidth(c);
  const int content = headH + m_table->rowCount() * rowH + 2;

  // Cap the height at ~70% of the window (screen as a fallback); beyond that
  // the vertical scrollbar on the right takes over.
  int limit = 400;
  if (QWidget *p = parentWidget() ? parentWidget()->window() : nullptr)
    limit = int(p->height() * 0.7);
  else if (QScreen *s = QGuiApplication::primaryScreen())
    limit = int(s->availableGeometry().height() * 0.7);
  limit = std::max(limit, headH + 4 * rowH);

  const bool scroll = content > limit;
  const int sb = scroll ? m_table->style()->pixelMetric(QStyle::PM_ScrollBarExtent) : 0;
  m_table->setFixedSize(w + sb, std::min(content, limit));
}

int DisplaySetsMenu::rowForSet(const QString &name) const
{
  for (int r = 0; r < m_table->rowCount(); r++)
    if (QTableWidgetItem *n = m_table->item(r, 0))
      if (n->text() == name)
        return r;
  return -1;
}

void DisplaySetsMenu::clickCell(int row, int col)
{
  onCellClicked(row, col);
}

void DisplaySetsMenu::onCellClicked(int row, int col)
{
  QTableWidgetItem *n = m_table->item(row, 0);
  QTableWidgetItem *cell = m_table->item(row, col);
  if (!n || !cell || !(n->flags() & Qt::ItemIsEnabled) || !(cell->flags() & Qt::ItemIsEnabled))
    return;
  const int idx = n->data(Qt::UserRole).toInt();
  const QString name = QString::fromUtf8(cgxDsSetName(idx));
  // The row may be stale (set deleted and its slot reused): never act on a
  // different set than the one the user clicked - refresh instead.
  if (!cgxDsSetValid(idx) || name != n->text() || cgxDsSignature() != m_signature)
  {
    checkForChanges();
    return;
  }
  const int shown = cgxDsDisplayedMask(idx);

  if (col == 0)
  {
    if (shown)
    {
      m_selected[name] = shown; // remember what was visible
      for (int t = 0; t < CGX_DS_NTYPES; t++)
        if (shown & (1 << t))
          cgxDsHide(idx, t);
    }
    else
    {
      const int mask = maskFor(idx);
      m_selected[name] = mask;
      for (int t = 0; t < CGX_DS_NTYPES; t++)
        if (mask & (1 << t))
          cgxDsShow(idx, t);
    }
  }
  else
  {
    const int t = col - 1;
    const int bit = 1 << t;
    if (shown)
    {
      if (shown & bit)
        cgxDsHide(idx, t);
      else
        cgxDsShow(idx, t);
      if (cgxDsDisplayedMask(idx))
        m_selected[name] = cgxDsDisplayedMask(idx);
      else
        m_selected[name] = shown; // last type unticked: keep it for the next switch-on
    }
    else
    {
      m_selected[name] = maskFor(idx) ^ bit; // selection only; set stays off
    }
  }
  std::fflush(stdout);
  m_signature = cgxDsSignature(); // our own change is not "stale"
  refreshStates();
}

#pragma once
// Step 16: "Display Sets" menu. A persistent QMenu holding one embedded
// QTableWidget: row = set, column 0 = set on/off checkbox + name, columns
// 1..9 = entity-type checkboxes (n e f p l s b S L). Toggles drive the
// unchanged legacy plus/minus commands through DisplaySetsBridge.
#include <QMenu>
#include <QString>

#include <map>

class QTimer;
class QTableWidget;
class QTableWidgetItem;

class DisplaySetsMenu : public QMenu
{
public:
  explicit DisplaySetsMenu(QWidget *parent = nullptr);

  // Re-read the legacy set list and the displayed state (also runs on show).
  void rebuild();
  // Re-read only the check states (cheap; after every toggle).
  void refreshStates();

  QTableWidget *table() const { return m_table; }
  // Selftest/automation entry points (same code path as a click).
  void clickCell(int row, int col);
  int rowForSet(const QString &name) const;
  void pollNow() { checkForChanges(); } // selftest: skip the timer wait

private:
  void onCellClicked(int row, int col);
  void checkForChanges(); // timer: rebuild when the legacy set state changed
  void fitSize();
  int maskFor(int setIdx) const; // remembered selection if the set is hidden
  QTableWidget *m_table;
  QTimer *m_timer;
  unsigned long long m_signature = 0;
  std::map<QString, int> m_selected; // per set name, entity mask while hidden
};

// Entry used by glue.cpp: returns the (persistent) menu, created on demand.
DisplaySetsMenu *cgxDisplaySetsMenu(QWidget *parent);

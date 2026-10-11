#pragma once
// Step 16: "Display Sets" menu. A persistent QMenu holding one embedded
// QTableWidget: row = set, column 0 = set on/off checkbox + name, columns
// 1..9 = entity-type checkboxes (n e f p l s b S L). Toggles drive the
// unchanged legacy plus/minus commands through DisplaySetsBridge.
#include <QMenu>
#include <QString>

#include <map>

class QAction;
class QLineEdit;
class QKeyEvent;
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
  QAction *hideAction() const { return m_hideAct; }
  // Selftest/automation entry points (same code path as a click).
  void clickCell(int row, int col);
  int rowForSet(const QString &name) const;
  void pollNow() { checkForChanges(); } // selftest: skip the timer wait

  // Step 19: rename. Double-click on the name (or F2 while hovering the row)
  // opens an inline editor; Enter accepts, Esc / click elsewhere cancels.
  void editName(int row);       // same code path as the double-click
  QLineEdit *editor() const { return m_editor; }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void onCellClicked(int row, int col);
  void commitEdit();
  void cancelEdit();
  void showEditError(const QString &msg);
  bool inToggleArea(int row, const QPoint &viewportPos) const;
  void checkForChanges(); // timer: rebuild when the legacy set state changed
  void fitSize();
  int maskFor(int setIdx) const; // remembered selection if the set is hidden
  QTableWidget *m_table;
  QTimer *m_timer;
  QAction *m_hideAct = nullptr;
  QLineEdit *m_editor = nullptr;
  int m_editIdx = -1;      // legacy set index being renamed
  QString m_editOld;       // its name when the edit started
  int m_hoverRow = -1;     // row under the mouse (target of F2)
  unsigned long long m_signature = 0;
  std::map<QString, int> m_selected; // per set name, entity mask while hidden
};

// Entry used by glue.cpp: returns the (persistent) menu, created on demand.
DisplaySetsMenu *cgxDisplaySetsMenu(QWidget *parent);

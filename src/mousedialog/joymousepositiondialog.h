#ifndef JOYMOUSEPOSITIONDIALOG_H
#define JOYMOUSEPOSITIONDIALOG_H

#include <QDialog>
#include "joybuttontypes/joybutton.h"
#include "joybuttonslot.h"

namespace Ui {
class JoyMousePositionDialog;
}

class PositionPickerOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit PositionPickerOverlay(QWidget *parent = nullptr);

signals:
    void positionPicked(QPoint pos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
};

class JoyMousePositionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JoyMousePositionDialog(JoyButtonSlot *slot, QWidget *parent = nullptr);
    ~JoyMousePositionDialog();

private slots:
    void on_buttonBox_accepted();
    void updateSpinBoxesEnabledState(int index);
    void on_pickPositionButton_clicked();

private:
    Ui::JoyMousePositionDialog *ui;
    JoyButtonSlot *m_slot;
};

#endif // JOYMOUSEPOSITIONDIALOG_H

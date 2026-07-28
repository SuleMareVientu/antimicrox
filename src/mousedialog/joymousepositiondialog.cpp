#include "joymousepositiondialog.h"
#include "ui_joymousepositiondialog.h"

#include <QApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

#include "event.h"

#if defined(Q_OS_WIN)
    #include <windows.h>
#elif defined(Q_OS_UNIX) && defined(WITH_X11)
    #include "x11extras.h"
    #include <X11/Xlib.h>
    #undef Status
#endif

PositionPickerOverlay::PositionPickerOverlay(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void PositionPickerOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QPoint globalPos = event->globalPos();
#else
        QPoint globalPos = event->globalPosition().toPoint();
#endif
        hide();
        emit positionPicked(globalPos);
        deleteLater();
    }
}

void PositionPickerOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        hide();
        deleteLater();
    }
}

void PositionPickerOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 80));
}

JoyMousePositionDialog::JoyMousePositionDialog(JoyButtonSlot *slot, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::JoyMousePositionDialog)
    , m_slot(slot)
{
    ui->setupUi(this);

    // Populate combo box options
    ui->positionSpaceComboBox->addItem(tr("Active Window"), JoyButtonSlot::PositionRelativeToActiveWindow);
    ui->positionSpaceComboBox->addItem(tr("Screen (Primary or Current)"), JoyButtonSlot::PositionRelativeToScreen);

    // Set initial values from slot
    int spaceIndex = ui->positionSpaceComboBox->findData(m_slot->getPositionSpace());
    if (spaceIndex != -1)
    {
        ui->positionSpaceComboBox->setCurrentIndex(spaceIndex);
    }

    ui->targetXSpinBox->setValue(m_slot->getTargetX() / 65535.0 * 100.0);
    ui->targetYSpinBox->setValue(m_slot->getTargetY() / 65535.0 * 100.0);

    ui->snapBackCheckBox->setChecked(m_slot->getSnapBack());

    connect(ui->positionSpaceComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateSpinBoxesEnabledState(int)));
    updateSpinBoxesEnabledState(ui->positionSpaceComboBox->currentIndex());
}

JoyMousePositionDialog::~JoyMousePositionDialog() { delete ui; }

void JoyMousePositionDialog::updateSpinBoxesEnabledState(int index) { Q_UNUSED(index); }

void JoyMousePositionDialog::on_buttonBox_accepted()
{
    m_slot->setPositionSpace(
        static_cast<JoyButtonSlot::JoyMousePositionSpace>(ui->positionSpaceComboBox->currentData().toInt()));
    m_slot->setTargetPosition(qRound(ui->targetXSpinBox->value() / 100.0 * 65535.0),
                              qRound(ui->targetYSpinBox->value() / 100.0 * 65535.0));
    m_slot->setSnapBack(ui->snapBackCheckBox->isChecked());
}

void JoyMousePositionDialog::on_pickPositionButton_clicked()
{
    JoyButtonSlot::JoyMousePositionSpace space =
        static_cast<JoyButtonSlot::JoyMousePositionSpace>(ui->positionSpaceComboBox->currentData().toInt());

    if (space == JoyButtonSlot::PositionRelativeToActiveWindow &&
        QGuiApplication::platformName() == QStringLiteral("wayland"))
    {
        QMessageBox::warning(this, tr("Unsupported Feature"),
                             tr("Active Window mouse positioning is not supported on Wayland."));
        return;
    }

    PositionPickerOverlay *overlay = new PositionPickerOverlay(this);

    QRect virtualGeo;
    for (QScreen *screen : QGuiApplication::screens())
    {
        virtualGeo = virtualGeo.united(screen->geometry());
    }
    overlay->setGeometry(virtualGeo);

    connect(overlay, &PositionPickerOverlay::positionPicked, this, [this](QPoint pos) {
        JoyButtonSlot::JoyMousePositionSpace space =
            static_cast<JoyButtonSlot::JoyMousePositionSpace>(ui->positionSpaceComboBox->currentData().toInt());

        QRect targetRect;
        if (space == JoyButtonSlot::PositionRelativeToScreen)
        {
            QScreen *screen = QGuiApplication::screenAt(pos);
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            targetRect = screen->geometry();
        } else if (space == JoyButtonSlot::PositionRelativeToActiveWindow)
        {
#if defined(Q_OS_WIN)
            qreal ratio = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
            POINT pt = {(LONG)(pos.x() * ratio), (LONG)(pos.y() * ratio)};
            HWND hwnd = WindowFromPoint(pt);
            if (hwnd)
            {
                hwnd = GetAncestor(hwnd, GA_ROOT);
                RECT rect;
                if (GetWindowRect(hwnd, &rect))
                {
                    targetRect = QRect(rect.left / ratio, rect.top / ratio, (rect.right - rect.left) / ratio,
                                       (rect.bottom - rect.top) / ratio);
                }
            }
#elif defined(Q_OS_UNIX) && defined(WITH_X11)
            Display *display = X11Extras::getInstance()->display();
            if (display)
            {
                Window root = DefaultRootWindow(display);
                int destX, destY;
                Window child = None;
                if (XTranslateCoordinates(display, root, root, pos.x(), pos.y(), &destX, &destY, &child))
                {
                    if (child != None)
                    {
                        targetRect = X11Extras::getInstance()->getWindowGeometry(child);
                    }
                }
            }
#endif
            if (targetRect.isEmpty())
            {
                resolveMousePositionTargetRect(m_slot, targetRect);
            }
        }

        if (targetRect.width() > 0 && targetRect.height() > 0)
        {
            double relX = (double)(pos.x() - targetRect.x()) / targetRect.width() * 100.0;
            double relY = (double)(pos.y() - targetRect.y()) / targetRect.height() * 100.0;

            relX = qBound(0.0, relX, 100.0);
            relY = qBound(0.0, relY, 100.0);

            ui->targetXSpinBox->setValue(relX);
            ui->targetYSpinBox->setValue(relY);
        }
    });

    connect(overlay, &QObject::destroyed, this, [this]() {
        for (QWidget *widget : QApplication::topLevelWidgets())
        {
            if (widget)
            {
                widget->setWindowOpacity(1.0);
            }
        }
        this->raise();
        this->activateWindow();
    });

    for (QWidget *widget : QApplication::topLevelWidgets())
    {
        if (widget && widget != overlay)
        {
            widget->setWindowOpacity(0.0);
        }
    }

    overlay->show();
    overlay->raise();
    overlay->activateWindow();
}

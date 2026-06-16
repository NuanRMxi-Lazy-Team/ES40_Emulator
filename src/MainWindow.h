#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QGridLayout>
#include <map>
#include <vector>
#include <string>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);
    void saveConfig();
    void addPciCard();
    void removePciCard();
    void updateScsiVisibility();

private:
    void setupUi();
    QWidget* createGeneralPage();
    QWidget* createStoragePage();
    QWidget* createNetworkPage();
    QWidget* createSerialPage();
    QWidget* createOtherPage();

    QListWidget *sidebar;
    QStackedWidget *pages;

    // General
    QComboBox *guiCombo;
    QSpinBox *videoScaleSpin;
    QCheckBox *videoLinearCheck;
    QCheckBox *videoHotkeysCheck;
    QComboBox *ramCombo;
    QLineEdit *srmRomEdit;
    QLineEdit *vgaRomEdit;
    QLineEdit *flashRomEdit;
    QLineEdit *dprRomEdit;
    QSpinBox *cpuCountSpin;
    QSpinBox *cpuMhzSpin;
    QComboBox *arcYearCombo;
    QCheckBox *fixedTimeCheck;
    QLineEdit *dateTimeEdit;

    // Storage
    struct DiskInfo {
        QComboBox *typeCombo;
        QLineEdit *pathEdit;
        QComboBox *deviceTypeCombo; // disk or cd-rom
    };
    std::map<std::string, DiskInfo> ideDisks;
    std::map<std::string, DiskInfo> floppyDisks;
    std::map<std::string, DiskInfo> scsiDisks;

    QWidget *scsiGroup;
    QGridLayout *scsiGrid;

    // Network
    QComboBox *nicAdapterCombo;
    QLineEdit *nicMacEdit;

    // Serial
    struct SerialInfo {
        QLineEdit *portEdit;
        QLineEdit *execEdit;
        QLineEdit *argsEdit;
    };
    SerialInfo serialPorts[2];

    // PCI Cards
    QTableWidget *pciTable;
    std::vector<std::pair<std::string, std::string>> pciCards; // <slot, type>
    
    // Other
    QCheckBox *mpuCheck;
    QComboBox *midiOutCombo;
    QCheckBox *mouseCheck;
    QComboBox *consoleOutCombo;
    QLineEdit *lptEdit;
    QCheckBox *pmuCheck;
    QCheckBox *usbCheck;
};

#endif // MAINWINDOW_H

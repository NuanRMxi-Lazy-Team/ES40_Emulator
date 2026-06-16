#include "MainWindow.h"
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <fstream>
#include <iostream>
#include <set>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef HAVE_PCAP
#ifdef _WIN32
#include <winsock.h>
// These are defined in es40-cfg.cpp, we might need to be careful about duplication
// For now, let's assume we can use the same logic or just use the system ones if available
#endif
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    QHBoxLayout *contentLayout = new QHBoxLayout();

    sidebar = new QListWidget();
    sidebar->setMaximumWidth(150);
    
    pages = new QStackedWidget();

    contentLayout->addWidget(sidebar);
    contentLayout->addWidget(pages);

    mainLayout->addLayout(contentLayout);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton("Save es40.cfg");
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveConfig);
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveBtn);
    mainLayout->addLayout(buttonLayout);

    sidebar->addItem("General");
    sidebar->addItem("Storage");
    sidebar->addItem("Network");
    sidebar->addItem("Serial");
    sidebar->addItem("Other");

    pages->addWidget(createGeneralPage());
    pages->addWidget(createStoragePage());
    pages->addWidget(createNetworkPage());
    pages->addWidget(createSerialPage());
    pages->addWidget(createOtherPage());

    connect(sidebar, &QListWidget::currentItemChanged, this, &MainWindow::changePage);
    sidebar->setCurrentRow(0);

    setWindowTitle("ES40 Configurator");
    resize(900, 700);
}

QWidget* MainWindow::createGeneralPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    QFormLayout *form = new QFormLayout();

    guiCombo = new QComboBox();
    guiCombo->addItem("None", "");
#ifdef HAVE_SDL
    guiCombo->addItem("SDL", "sdl");
#endif
    form->addRow("GUI:", guiCombo);

    videoScaleSpin = new QSpinBox();
    videoScaleSpin->setRange(1, 10);
    videoScaleSpin->setValue(1);
    form->addRow("Video Scale Ratio:", videoScaleSpin);

    videoLinearCheck = new QCheckBox("Bilinear Filtering");
    videoLinearCheck->setChecked(true);
    form->addRow("Video Linear:", videoLinearCheck);

    videoHotkeysCheck = new QCheckBox("Enable Scale Hotkeys");
    form->addRow("Video Hotkeys:", videoHotkeysCheck);

    ramCombo = new QComboBox();
    for (int i = 25; i < 35; i++) {
        long long bytes = 1LL << i;
        QString label;
        if (bytes >= (1024 * 1024 * 1024)) {
            label = QString::number(bytes / (1024 * 1024 * 1024)) + " GB";
        } else {
            label = QString::number(bytes / (1024 * 1024)) + " MB";
        }
        ramCombo->addItem(label, QString::number(i));
    }
    ramCombo->setCurrentText("256 MB");
    form->addRow("RAM Size:", ramCombo);

    srmRomEdit = new QLineEdit("rom/cl67srmrom.exe");
    form->addRow("SRM ROM Path:", srmRomEdit);

    vgaRomEdit = new QLineEdit("rom/S3VIRGE.ROM");
    form->addRow("VGA BIOS Path:", vgaRomEdit);

    flashRomEdit = new QLineEdit("rom/flash.rom");
    form->addRow("Flash ROM Path:", flashRomEdit);

    dprRomEdit = new QLineEdit("rom/dpr.rom");
    form->addRow("DPR ROM Path:", dprRomEdit);

    cpuCountSpin = new QSpinBox();
    cpuCountSpin->setRange(1, 4);
    cpuCountSpin->setValue(1);
    form->addRow("CPU Count:", cpuCountSpin);

    cpuMhzSpin = new QSpinBox();
    cpuMhzSpin->setRange(10, 2000);
    cpuMhzSpin->setValue(500);
    cpuMhzSpin->setSuffix(" MHz");
    form->addRow("CPU Reported Speed:", cpuMhzSpin);

    arcYearCombo = new QComboBox();
    arcYearCombo->addItem("VMS/Unix/Linux", "false");
    arcYearCombo->addItem("Windows NT/AlphaBIOS", "true");
    form->addRow("ARC Year Compat:", arcYearCombo);

    fixedTimeCheck = new QCheckBox("Set Fixed Date/Time at Startup");
    form->addRow("Fixed Time:", fixedTimeCheck);

    dateTimeEdit = new QLineEdit("2000-01-01 12:00:00");
    dateTimeEdit->setEnabled(false);
    connect(fixedTimeCheck, &QCheckBox::toggled, dateTimeEdit, &QLineEdit::setEnabled);
    form->addRow("Date/Time (YYYY-MM-DD HH:MM:SS):", dateTimeEdit);

    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::createStoragePage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("IDE Disks:"));
    QGridLayout *ideGrid = new QGridLayout();
    QStringList positions = {"0.0 (Pri Master)", "0.1 (Pri Slave)", "1.0 (Sec Master)", "1.1 (Sec Slave)"};
    QStringList ideKeys = {"0.0", "0.1", "1.0", "1.1"};
    
    for (int i = 0; i < 4; ++i) {
        ideGrid->addWidget(new QLabel(positions[i]), i, 0);
        
        QComboBox *type = new QComboBox();
        type->addItem("None", "");
        type->addItem("File", "file");
        type->addItem("Device", "device");
        type->addItem("RAM Disk", "ramdisk");
        ideGrid->addWidget(type, i, 1);

        QLineEdit *path = new QLineEdit();
        ideGrid->addWidget(path, i, 2);

        QComboBox *devType = new QComboBox();
        devType->addItem("Disk", "false");
        devType->addItem("CD-ROM", "true");
        ideGrid->addWidget(devType, i, 3);

        ideDisks[ideKeys[i].toStdString()] = {type, path, devType};
    }
    layout->addLayout(ideGrid);

    layout->addWidget(new QLabel("Floppy Disks:"));
    QGridLayout *fdGrid = new QGridLayout();
    QStringList fdPositions = {"0 (A:)", "1 (B:)"};
    QStringList fdKeys = {"0", "1"};
    for (int i = 0; i < 2; ++i) {
        fdGrid->addWidget(new QLabel(fdPositions[i]), i, 0);
        QComboBox *type = new QComboBox();
        type->addItem("None", "");
        type->addItem("File", "file");
        type->addItem("Device", "device");
        fdGrid->addWidget(type, i, 1);
        QLineEdit *path = new QLineEdit();
        fdGrid->addWidget(path, i, 2);
        floppyDisks[fdKeys[i].toStdString()] = {type, path, nullptr};
    }
    layout->addLayout(fdGrid);
    
    scsiGroup = new QWidget();
    QVBoxLayout *scsiLayout = new QVBoxLayout(scsiGroup);
    scsiLayout->setContentsMargins(0, 10, 0, 0);
    scsiLayout->addWidget(new QLabel("SCSI Disks (ID 0-7):"));
    scsiGrid = new QGridLayout();
    for (int i = 0; i < 8; ++i) {
        scsiGrid->addWidget(new QLabel(QString("ID %1:").arg(i)), i, 0);
        QComboBox *type = new QComboBox();
        type->addItem("None", "");
        type->addItem("File", "file");
        type->addItem("Device", "device");
        scsiGrid->addWidget(type, i, 1);
        QLineEdit *path = new QLineEdit();
        scsiGrid->addWidget(path, i, 2);
        QComboBox *devType = new QComboBox();
        devType->addItem("Disk", "false");
        devType->addItem("CD-ROM", "true");
        scsiGrid->addWidget(devType, i, 3);
        scsiDisks[std::to_string(i)] = {type, path, devType};
    }
    scsiLayout->addLayout(scsiGrid);
    layout->addWidget(scsiGroup);
    scsiGroup->setVisible(false);

    layout->addStretch();
    return page;
}

QWidget* MainWindow::createNetworkPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);
    QFormLayout *form = new QFormLayout();

    nicAdapterCombo = new QComboBox();
    nicAdapterCombo->addItem("List at runtime", "");
    
#ifdef HAVE_PCAP
    // Logic to populate pcap devices
    // This is a simplified version of the logic in es40-cfg.cpp
    // Since we are running in the configurator, we can try to load wpcap.dll and find devs
    
    typedef struct pcap_if {
        struct pcap_if *next;
        char           *name;
        char           *description;
        void           *addresses;
        unsigned int    flags;
    } pcap_if_t;

    typedef int (*pcap_findalldevs_t)(pcap_if_t **, char *);
    typedef void (*pcap_freealldevs_t)(pcap_if_t *);

    HMODULE libhandle = LoadLibraryA("wpcap.dll");
    if (libhandle) {
        auto f_findalldevs = (pcap_findalldevs_t)GetProcAddress(libhandle, "pcap_findalldevs");
        auto f_freealldevs = (pcap_freealldevs_t)GetProcAddress(libhandle, "pcap_freealldevs");
        if (f_findalldevs && f_freealldevs) {
            pcap_if_t *alldevs;
            char errbuf[256];
            if (f_findalldevs(&alldevs, errbuf) != -1) {
                for (pcap_if_t *d = alldevs; d; d = d->next) {
                    QString desc = d->description ? d->description : d->name;
                    nicAdapterCombo->addItem(desc, QString(d->name));
                }
                f_freealldevs(alldevs);
            }
        }
        // FreeLibrary(libhandle); // Keep it loaded or let OS handle it
    }
#endif

    form->addRow("Host Adapter:", nicAdapterCombo);

    nicMacEdit = new QLineEdit("08-00-2B-E5-40-00");
    form->addRow("MAC Address:", nicMacEdit);

    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::createSerialPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    for (int i = 0; i < 2; ++i) {
        layout->addWidget(new QLabel(QString("Serial Port %1:").arg(i)));
        QFormLayout *form = new QFormLayout();
        serialPorts[i].portEdit = new QLineEdit(QString::number(21264 + i));
        form->addRow("Telnet Port:", serialPorts[i].portEdit);
        serialPorts[i].execEdit = new QLineEdit("C:\\Program Files\\Putty\\Putty.exe");
        form->addRow("Action Program:", serialPorts[i].execEdit);
        serialPorts[i].argsEdit = new QLineEdit(QString("telnet://localhost:%1").arg(21264 + i));
        form->addRow("Arguments:", serialPorts[i].argsEdit);
        layout->addLayout(form);
    }

    layout->addStretch();
    return page;
}

QWidget* MainWindow::createOtherPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    layout->addWidget(new QLabel("PCI Cards:"));
    pciTable = new QTableWidget(0, 2);
    pciTable->setHorizontalHeaderLabels({"PCI Slot", "Card Type"});
    pciTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(pciTable);

    QHBoxLayout *pciBtnLayout = new QHBoxLayout();
    QPushButton *addPciBtn = new QPushButton("Add Card");
    QPushButton *remPciBtn = new QPushButton("Remove Card");
    connect(addPciBtn, &QPushButton::clicked, this, &MainWindow::addPciCard);
    connect(remPciBtn, &QPushButton::clicked, this, &MainWindow::removePciCard);
    pciBtnLayout->addWidget(addPciBtn);
    pciBtnLayout->addWidget(remPciBtn);
    layout->addLayout(pciBtnLayout);

    QFormLayout *form = new QFormLayout();

    mpuCheck = new QCheckBox("Enable MPU-401");
    form->addRow("MPU-401:", mpuCheck);

    midiOutCombo = new QComboBox();
    midiOutCombo->addItem("List at runtime", "");
    
#ifdef _WIN32
    MIDIOUTCAPSA caps;
    for (UINT i = 0; i < midiOutGetNumDevs(); i++) {
        if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            midiOutCombo->addItem(caps.szPname, QString::number(i));
        }
    }
#endif

    form->addRow("MIDI Out Device:", midiOutCombo);

    mouseCheck = new QCheckBox("Enable Mouse");
    mouseCheck->setChecked(true);
    form->addRow("Mouse:", mouseCheck);

    consoleOutCombo = new QComboBox();
    consoleOutCombo->addItem("Graphics Controller", "true");
    consoleOutCombo->addItem("Serial Port 0", "false");
    form->addRow("Console Output:", consoleOutCombo);

    lptEdit = new QLineEdit();
    form->addRow("LPT Output File:", lptEdit);

    pmuCheck = new QCheckBox("Enable PMU (M7101)");
    pmuCheck->setChecked(true);
    form->addRow("PMU:", pmuCheck);

    usbCheck = new QCheckBox("Enable USB OHCI");
    usbCheck->setChecked(true);
    form->addRow("USB:", usbCheck);

    layout->addLayout(form);
    layout->addStretch();
    return page;
}

void MainWindow::addPciCard()
{
    QStringList sl;
    std::set<QString> occupiedSlots;
    for (int i = 0; i < pciTable->rowCount(); ++i) {
        occupiedSlots.insert(pciTable->item(i, 0)->text());
    }

    for (int b = 0; b <= 1; ++b) {
        for (int s = 1; s <= 6; ++s) {
            QString slot = QString("pci%1.%2").arg(b).arg(s);
            if (occupiedSlots.find(slot) == occupiedSlots.end()) {
                sl.append(slot);
            }
        }
    }

    bool ok;
    QString slot = QInputDialog::getItem(this, "Select Slot", "Slot:", sl, 0, false, &ok);
    if (!ok || slot.isEmpty()) return;

    QStringList tl;
    tl.append("dec21143");
    tl.append("sym53c810");
    tl.append("sym53c895");
    tl.append("es1370");
    tl.append("s3");
    QString type = QInputDialog::getItem(this, "Select Card Type", "Type:", tl, 0, false, &ok);
    if (!ok || type.isEmpty()) return;

    int row = pciTable->rowCount();
    pciTable->insertRow(row);
    pciTable->setItem(row, 0, new QTableWidgetItem(slot));
    pciTable->setItem(row, 1, new QTableWidgetItem(type));
    updateScsiVisibility();
}

void MainWindow::removePciCard()
{
    int row = pciTable->currentRow();
    if (row >= 0) {
        pciTable->removeRow(row);
        updateScsiVisibility();
    }
}

void MainWindow::updateScsiVisibility()
{
    bool hasScsi = false;
    for (int i = 0; i < pciTable->rowCount(); ++i) {
        QString type = pciTable->item(i, 1)->text();
        if (type.contains("sym53c", Qt::CaseInsensitive)) {
            hasScsi = true;
            break;
        }
    }
    if (scsiGroup) scsiGroup->setVisible(hasScsi);
}

void MainWindow::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        return;
    pages->setCurrentIndex(sidebar->row(current));
}

void MainWindow::saveConfig()
{
    std::ofstream os("es40.cfg");
    if (!os.is_open()) {
        QMessageBox::critical(this, "Error", "Could not open es40.cfg for writing.");
        return;
    }

    QString gui = guiCombo->currentData().toString();
    if (!gui.isEmpty()) {
        os << "gui = " << gui.toStdString() << "\n";
        os << "{\n";
        os << "  video.scale_ratio = " << videoScaleSpin->value() << ";\n";
        os << "  video.linear = " << (videoLinearCheck->isChecked() ? "true" : "false") << ";\n";
        os << "  video.scale_change_enable = " << (videoHotkeysCheck->isChecked() ? "true" : "false") << ";\n";
        os << "}\n\n";
    }

    os << "sys0 = tsunami\n";
    os << "{\n";
    os << "  memory.bits = " << ramCombo->currentData().toString().toStdString() << ";\n";
    os << "  rom.srm = \"" << srmRomEdit->text().toStdString() << "\";\n";
    os << "  rom.flash = \"" << flashRomEdit->text().toStdString() << "\";\n";
    os << "  rom.dpr = \"" << dprRomEdit->text().toStdString() << "\";\n\n";

    for (int i = 0; i < cpuCountSpin->value(); ++i) {
        os << "  cpu" << i << " = ev68cb\n";
        os << "  {\n";
        os << "    speed = " << cpuMhzSpin->value() << "M;\n";
        os << "    icache = true;\n";
        os << "  }\n\n";
    }

    for (int i = 0; i < 2; ++i) {
        os << "  serial" << i << " = serial\n";
        os << "  {\n";
        if (serialPorts[i].portEdit->text() == "none") {
            os << "    null_attach = true;\n";
        } else {
            os << "    port = " << serialPorts[i].portEdit->text().toStdString() << ";\n";
            if (serialPorts[i].execEdit->text() != "none") {
                os << "    action = \"\"\"" << serialPorts[i].execEdit->text().toStdString() << "\"\" " 
                   << serialPorts[i].argsEdit->text().toStdString() << "\";\n";
            }
        }
        os << "  }\n\n";
    }

    os << "  fdc0 = floppy\n";
    os << "  {\n";
    for (auto const& [key, disk] : floppyDisks) {
        QString type = disk.typeCombo->currentData().toString();
        if (!type.isEmpty()) {
            os << "    disk0." << key << " = " << type.toStdString() << "\n";
            os << "    {\n";
            os << "      file = \"" << disk.pathEdit->text().toStdString() << "\";\n";
            os << "    }\n";
        }
    }
    os << "  }\n\n";

    os << "  pci0.15 = ali_ide\n";
    os << "  {\n";
    for (auto const& [key, disk] : ideDisks) {
        QString type = disk.typeCombo->currentData().toString();
        if (!type.isEmpty()) {
            os << "    disk" << key << " = " << type.toStdString() << "\n";
            os << "    {\n";
            os << "      " << type.toStdString() << " = \"" << disk.pathEdit->text().toStdString() << "\";\n";
            os << "      cdrom = " << disk.deviceTypeCombo->currentData().toString().toStdString() << ";\n";
            os << "    }\n";
        }
    }
    os << "  }\n\n";

    // Simplified PCI cards (can be improved)
    if (!nicAdapterCombo->currentText().isEmpty()) {
        os << "  pci0.1 = dec21143\n";
        os << "  {\n";
        if (!nicAdapterCombo->currentData().toString().isEmpty())
            os << "    adapter = \"" << nicAdapterCombo->currentData().toString().toStdString() << "\";\n";
        os << "    mac = \"" << nicMacEdit->text().toStdString() << "\";\n";
        os << "  }\n\n";
    }

    os << "  arc_year_compat = " << arcYearCombo->currentData().toString().toStdString() << ";\n\n";
    
    if (fixedTimeCheck->isChecked()) {
        os << "  time = \"" << dateTimeEdit->text().toStdString() << "\";\n\n";
    }

    os << "  pci0.7 = ali\n";
    os << "  {\n";
    os << "    mouse.enabled = " << (mouseCheck->isChecked() ? "true" : "false") << ";\n";
    os << "    vga_console = " << consoleOutCombo->currentData().toString().toStdString() << ";\n";
    if (!lptEdit->text().isEmpty())
        os << "    lpt.outfile = \"" << lptEdit->text().toStdString() << "\"\n";
    os << "  }\n\n";

    if (usbCheck->isChecked()) {
        os << "  pci0.19 = ali_usb\n  {\n  }\n";
    }
    if (pmuCheck->isChecked()) {
        os << "  pci0.17 = ali_pmu\n  {\n  }\n";
    }

    for (int i = 0; i < pciTable->rowCount(); ++i) {
        QString slot = pciTable->item(i, 0)->text();
        QString type = pciTable->item(i, 1)->text();
        os << "  " << slot.toStdString() << " = " << type.toStdString() << "\n";
        os << "  {\n";
        if (type.contains("s3", Qt::CaseInsensitive)) {
            os << "    bios = \"" << vgaRomEdit->text().toStdString() << "\";\n";
        }
        if (type.contains("sym53c", Qt::CaseInsensitive)) {
            for (auto const& [key, disk] : scsiDisks) {
                QString dType = disk.typeCombo->currentData().toString();
                if (!dType.isEmpty()) {
                    os << "    device" << key << " = " << dType.toStdString() << "\n";
                    os << "    {\n";
                    os << "      file = \"" << disk.pathEdit->text().toStdString() << "\";\n";
                    os << "      cdrom = " << disk.deviceTypeCombo->currentData().toString().toStdString() << ";\n";
                    os << "    }\n";
                }
            }
        }
        os << "  }\n\n";
    }

    os << "}\n";
    os.close();
    QMessageBox::information(this, "Success", "Configuration saved to es40.cfg");
}

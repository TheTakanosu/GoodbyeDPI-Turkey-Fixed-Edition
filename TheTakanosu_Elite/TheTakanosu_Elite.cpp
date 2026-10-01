#pragma warning(disable : 4996)

#include "TheTakanosu_Elite.h"
#include <QTimer>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <QPixmap>
#include <QComboBox>
#include <QProgressBar>
#include <QStyledItemDelegate>
#include <QListView>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QMessageBox> 
#include <QDateTime>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

PowerToysDashboard::PowerToysDashboard(QWidget* parent) : QWidget(parent) {
    this->setStyleSheet("background-color: transparent;");

    motor_process = nullptr;
    ping_process = nullptr;
    is_auto_detect_mode = false;
    is_dark_mode = true;
    current_auto_test_index = 0;
    current_run_param = "";

    outer_layout = new QVBoxLayout(this);
    outer_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    outer_layout->setContentsMargins(0, 20, 0, 20);

    inner_container = new QWidget();
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    inner_container->setMaximumWidth(1050);

    flex_layout = new QBoxLayout(QBoxLayout::LeftToRight, inner_container);
    flex_layout->setContentsMargins(10, 10, 10, 10);
    flex_layout->setSpacing(20);

    col_left = new QWidget();
    col_left->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    QVBoxLayout* col_left_layout = new QVBoxLayout(col_left);
    col_left_layout->setContentsMargins(0, 0, 0, 0);

    block_quick = new QFrame();
    block_quick->setObjectName("TargetBlock");
    block_quick->setMinimumWidth(300);
    block_quick->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* layout_quick = new QVBoxLayout(block_quick);

    title_quick = new QLabel("Hızlı Erişim");
    title_quick->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    layout_quick->addWidget(title_quick);

    lbl_iss = new QLabel("İnternet Servis Sağlayıcısı (ISS):");
    lbl_iss->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 13px; margin-top: 15px; border: none; background: transparent;");
    layout_quick->addWidget(lbl_iss);

    combo_iss = new QComboBox();
    combo_iss->addItems({ "Otomatik Algıla (Önerilen)", "Superonline", "Türk Telekom", "TurkNet", "Kablonet", "Vodafone" });
    combo_iss->setFixedHeight(38);
    combo_iss->setCursor(Qt::PointingHandCursor);

    QListView* combo_list_view = new QListView();
    combo_list_view->setCursor(Qt::PointingHandCursor);
    combo_iss->setView(combo_list_view);
    combo_iss->setItemDelegate(new QStyledItemDelegate());
    layout_quick->addWidget(combo_iss);

    btn_start = new QPushButton("OTONOM SİSTEMİ BAŞLAT");
    btn_start->setFixedHeight(50);
    btn_start->setCursor(Qt::PointingHandCursor);
    btn_start->setStyleSheet(
        "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; } "
        "QPushButton:hover { background-color: #8e24aa; } "
        "QPushButton:pressed { background-color: #4a148c; }"
    );
    layout_quick->addWidget(btn_start);

    connect(btn_start, &QPushButton::clicked, this, &PowerToysDashboard::start_autonomous_system);

    progress_bar = new QProgressBar();
    progress_bar->setFixedHeight(6);
    progress_bar->setTextVisible(false);
    progress_bar->setValue(0);
    progress_bar->setStyleSheet(
        "QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } "
        "QProgressBar::chunk { background-color: #8a2be2; border-radius: 3px; }"
    );
    layout_quick->addWidget(progress_bar);

    lbl_status = new QLabel("Sistem Beklemede. Hedef ISS'yi seçin ve başlatın.");
    lbl_status->setWordWrap(true);
    lbl_status->setStyleSheet("color: #777777; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; background: transparent;");
    lbl_status->setAlignment(Qt::AlignCenter);
    layout_quick->addWidget(lbl_status);

    lbl_active_mod = new QLabel("Aktif Mod: BEKLENİYOR...");
    lbl_active_mod->setWordWrap(true);
    lbl_active_mod->setStyleSheet("color: #ff5252; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;");
    lbl_active_mod->setAlignment(Qt::AlignCenter);
    layout_quick->addWidget(lbl_active_mod);

    log_console = new QPlainTextEdit();
    log_console->setReadOnly(true);
    log_console->setFixedHeight(120);

    // 🎨 BUM! KUSURSUZ YUVARLAK UÇLU (HAP ŞEKLİNDE) LOG ÇUBUĞU!
    log_console->setStyleSheet(
        "QPlainTextEdit { background-color: #121212; color: #00E676; font-family: 'Consolas', 'Courier New'; font-size: 11px; border: 1px solid #3a3a3a; border-radius: 6px; padding: 5px; margin-top: 10px; } "
        "QScrollBar:vertical { border: none; background: transparent; width: 14px; margin: 0px; } "
        "QScrollBar::handle:vertical { background: #444444; min-height: 30px; border-radius: 2px; margin-left: 10px; margin-right: 0px; } " /* 4px kalınlık -> 2px yarıçap (Tam yuvarlak uç) */
        "QScrollBar::handle:vertical:hover { background: #777777; border-radius: 5px; margin-left: 4px; } " /* 10px şişti -> 5px yarıçap (Tam yuvarlak uç) */
        "QScrollBar::handle:vertical:pressed { background: #555555; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );
    log_console->appendPlainText("[SİSTEM] The Takanosu Elite Başlatıldı...");
    log_console->appendPlainText("[SİSTEM] Kullanıcı emirleri bekleniyor...");
    layout_quick->addWidget(log_console);

    // 🚀 BUM! UZATILABİLİR MENÜ (GENİŞLET/DARALT) BUTONU
    QPushButton* btn_expand_log = new QPushButton("▼ Log Ekranını Genişlet");
    btn_expand_log->setCursor(Qt::PointingHandCursor);
    btn_expand_log->setStyleSheet("QPushButton { color: #777777; background: transparent; font-family: 'Segoe UI Variable'; font-size: 11px; font-weight: bold; border: none; margin-top: 2px; } QPushButton:hover { color: #00E676; }");
    layout_quick->addWidget(btn_expand_log);

    // Butona tıklandığında log ekranını 120 piksellik ufak kutudan 300 piksellik dev terminale dönüştüren mekanizma
    connect(btn_expand_log, &QPushButton::clicked, this, [this, btn_expand_log]() {
        if (log_console->height() <= 120) {
            log_console->setFixedHeight(300);
            btn_expand_log->setText("▲ Log Ekranını Daralt");
        }
        else {
            log_console->setFixedHeight(120);
            btn_expand_log->setText("▼ Log Ekranını Genişlet");
        }
        });

    QSettings settings("TheTakanosu", "EliteEngine");
    int last_index = settings.value("last_iss_index", 0).toInt();
    QString last_mode = settings.value("last_active_mod", "BEKLENİYOR...").toString();
    QString saved_param = settings.value("last_working_param", "").toString();

    combo_iss->setCurrentIndex(last_index);
    if (last_mode != "BEKLENİYOR..." && !last_mode.contains("SANSÜR") && !last_mode.contains("YOK")) {
        lbl_active_mod->setText("Son Çalışan Mod: " + last_mode);
        lbl_active_mod->setStyleSheet("color: #00E676; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;");

        if (!saved_param.isEmpty()) {
            QTimer::singleShot(500, this, &PowerToysDashboard::restore_last_session);
        }
    }

    col_left_layout->addWidget(block_quick);
    col_left_layout->addStretch();

    col_right = new QWidget();
    col_right->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    QVBoxLayout* col_right_layout = new QVBoxLayout(col_right);
    col_right_layout->setContentsMargins(0, 0, 0, 0);

    block_utils = new QFrame();
    block_utils->setObjectName("TargetBlock");
    block_utils->setMinimumWidth(280);
    block_utils->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* layout_utils = new QVBoxLayout(block_utils);
    layout_utils->setSpacing(7);

    title_utils = new QLabel("Sistem Araçları (Utilities)");
    title_utils->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    layout_utils->addWidget(title_utils);

    QString utils_names[8] = {
        "🧹 DNS Önbelleğini Temizle (Flush DNS)",
        "💀 Zombi Süreçleri Katlet (Kill DPI)",
        "⚡ Hızlı DNS Optimizasyonu",
        "🔄 Ağ Adaptörünü Yeniden Başlat",
        "📡 Anlık Ping ve Gecikme Testi",
        "📜 Sistem Loglarını Görüntüle",
        "🌐 Güvenli İnternet Durum Kontrolü",
        "🛠️ Gelişmiş Ağ Sıfırlama (Winsock)"
    };

    for (int i = 0; i < 8; ++i) {
        QPushButton* row_btn = new QPushButton(utils_names[i]);
        row_btn->setFixedHeight(44);
        row_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row_btn->setCursor(Qt::PointingHandCursor);

        if (i == 0) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_flush_dns); }
        else if (i == 1) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::kill_dpi_service); }
        else if (i == 2) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_fast_dns); }
        else if (i == 3) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_restart_adapter); }
        else if (i == 4) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_ping_test); }
        else if (i == 5) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_export_logs); }
        else if (i == 6) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_safe_net_check); }
        else if (i == 7) { connect(row_btn, &QPushButton::clicked, this, &PowerToysDashboard::util_winsock_reset); }

        layout_utils->addWidget(row_btn);
        utility_btns.append(row_btn);
    }

    QPushButton* btn_github = new QPushButton("🐙 TheTakanosu GitHub Profili");;
    btn_github->setFixedHeight(44);
    btn_github->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn_github->setCursor(Qt::PointingHandCursor);
    connect(btn_github, &QPushButton::clicked, this, []() { QDesktopServices::openUrl(QUrl("https://github.com/TheTakanosu")); });

    layout_utils->addWidget(btn_github);
    utility_btns.append(btn_github);

    col_right_layout->addWidget(block_utils);
    col_right_layout->addStretch();

    flex_layout->addWidget(col_left);
    flex_layout->addWidget(col_right);
    outer_layout->addWidget(inner_container);
}

void PowerToysDashboard::restore_last_session() {
    QSettings settings("TheTakanosu", "EliteEngine");
    QString last_param = settings.value("last_working_param", "").toString();
    if (last_param.isEmpty()) return;

    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    log_console->appendPlainText(QString("\n[%1] [SİSTEM] Önceki başarılı oturum tespit edildi. Servis arka planda uyandırılıyor...").arg(zaman));

    QString app_dir = QCoreApplication::applicationDirPath();
    QString exe_path = QDir::toNativeSeparators(app_dir + "/goodbyedpi/x86_64/goodbyedpi.exe");
    QString list_path = QDir::toNativeSeparators(app_dir + "/goodbyedpi/turkey-blacklist.txt");
    QString bat_path = app_dir + "/elite_restore.bat";

    QFile bat_file(bat_path);
    if (bat_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&bat_file);
        out << "@echo off\n";
        out << "taskkill /F /IM goodbyedpi.exe >nul 2>&1\n";
        out << "sc stop \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc delete \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "timeout /t 1 /nobreak >nul\n";

        QString bin_cmd = "\"\\\"" + exe_path + "\\\" " + last_param + " --blacklist \\\"" + list_path + "\\\"\"";
        out << "sc create \"GoodbyeDPI_Elite\" binPath= " << bin_cmd << " start= auto >nul 2>&1\n";
        out << "sc start \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        bat_file.close();
    }

    QProcess::execute("cmd.exe", QStringList() << "/c" << bat_path);
    log_console->appendPlainText(QString("[%1] [BAŞARI] %2 profili başarıyla aktif edildi!").arg(zaman).arg(settings.value("last_active_mod").toString()));
}

void PowerToysDashboard::util_flush_dns() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] DNS Önbelleği (Flush DNS) temizleniyor...").arg(zaman));
    QProcess::execute("cmd.exe", QStringList() << "/c" << "ipconfig /flushdns");
    log_console->appendPlainText(QString("[%1] [BAŞARI] DNS Önbelleği başarıyla temizlendi!").arg(zaman));
}

void PowerToysDashboard::util_fast_dns() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] Hızlı DNS Optimizasyonu başlatıldı...").arg(zaman));
    QProcess::execute("cmd.exe", QStringList() << "/c" << "ipconfig /flushdns");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "ipconfig /registerdns");
    log_console->appendPlainText(QString("[%1] [BAŞARI] DNS kayıtları sisteme yeniden kaydedildi!").arg(zaman));
}

void PowerToysDashboard::util_restart_adapter() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] Ağ Adaptörünüz yenileniyor (İnternetiniz anlık kopabilir)...").arg(zaman));
    QProcess::execute("cmd.exe", QStringList() << "/c" << "ipconfig /release");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "ipconfig /renew");
    log_console->appendPlainText(QString("[%1] [BAŞARI] IP Adresi DHCP üzerinden başarıyla yenilendi!").arg(zaman));
}

void PowerToysDashboard::util_ping_test() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString target_ip = "8.8.8.8";

    QString active_text = lbl_active_mod->text().toUpper();
    int selected_combo = combo_iss->currentIndex();

    if (active_text.contains("TURKNET") || selected_combo == 3) {
        target_ip = "1.1.1.1";
    }
    else if (active_text.contains("TELEKOM") || active_text.contains("SUPERONLINE") ||
        active_text.contains("VODAFONE") || active_text.contains("KABLONET") ||
        selected_combo == 1 || selected_combo == 2 || selected_combo == 4 || selected_combo == 5) {
        target_ip = "77.88.8.8";
    }

    log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] Gecikme (Ping) testi başlatıldı (Hedef: %2)...").arg(zaman).arg(target_ip));

    QProcess* p = new QProcess(this);
    p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
    connect(p, &QProcess::readyReadStandardOutput, [this, p]() {
        QString output = p->readAllStandardOutput().trimmed();
        if (!output.isEmpty()) log_console->appendPlainText(output);
        });

    p->start("cmd.exe", QStringList() << "/c" << "ping -n 4 " + target_ip);
}

void PowerToysDashboard::util_export_logs() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString app_dir = QCoreApplication::applicationDirPath();
    QString log_path = app_dir + "/takanosu_logs.txt";

    QFile log_file(log_path);
    if (log_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&log_file);
        out << log_console->toPlainText();
        log_file.close();

        log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] Loglar başarıyla txt dosyasına aktarıldı: %2").arg(zaman).arg(log_path));
        QDesktopServices::openUrl(QUrl::fromLocalFile(log_path));
    }
}

void PowerToysDashboard::util_safe_net_check() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] Güvenli İnternet (Aile Profili) ağda aranıyor...").arg(zaman));

    QProcess* p = new QProcess(this);
    p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });

    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p, zaman](int exitCode) {
        QString output = p->readAllStandardOutput();
        bool is_safe_net_on = false;

        if (output.contains("195.175.") || output.contains("212.156.") || output.contains("Request timed out") || output.contains("could not find host")) {
            is_safe_net_on = true;
        }

        QMessageBox msgBox(this);
        msgBox.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        msgBox.setWindowTitle("Güvenli İnternet Kontrolü");

        QString app_dir = QCoreApplication::applicationDirPath();
        msgBox.setIconPixmap(QPixmap(app_dir + "/assets/icon-transparant.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

        if (is_safe_net_on) {
            msgBox.setText("⚠️ DİKKAT: Güvenli İnternet (Aile Profili) AÇIK Olabilir!");
            msgBox.setInformativeText("Test edilen yasaklı siteye ulaşılamadı veya servis sağlayıcınızın (ISS) uyarı sayfasına yönlendirildiniz.\n\nEğer DPI programımız çalışmasına rağmen sitelere giremiyorsanız, ISS'nizi arayıp 'Aile Profilini' kapattırınız.");
        }
        else {
            msgBox.setText("✅ BAŞARILI: Güvenli İnternet KAPALI Görünüyor!");
            msgBox.setInformativeText("Test edilen siteye ağınız üzerinden müdahale edilmeden doğrudan ulaşılabiliyor.\n\n(DPI Bypass motorumuz tam performansla çalışabilir!)");
        }

        if (is_dark_mode) {
            msgBox.setStyleSheet(
                "QMessageBox { background-color: #1a1a1a; border: 1px solid #444444; border-radius: 12px; }"
                "QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; padding-top: 5px; }"
                "QLabel#qt_msgbox_label { color: " + QString(is_safe_net_on ? "#ff5252" : "#00E676") + "; font-size: 14px; font-weight: bold; margin-bottom: 5px; }"
                "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; padding: 10px 30px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: none; margin-top: 15px; }"
                "QPushButton:hover { background-color: #8e24aa; }"
                "QPushButton:pressed { background-color: #4a148c; }"
            );
        }
        else {
            msgBox.setStyleSheet(
                "QMessageBox { background-color: #f5f5f5; border: 1px solid #aaaaaa; border-radius: 12px; }"
                "QLabel { color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; padding-top: 5px; }"
                "QLabel#qt_msgbox_label { color: " + QString(is_safe_net_on ? "#d32f2f" : "#008B00") + "; font-size: 14px; font-weight: bold; margin-bottom: 5px; }"
                "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; padding: 10px 30px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: none; margin-top: 15px; }"
                "QPushButton:hover { background-color: #8e24aa; }"
                "QPushButton:pressed { background-color: #4a148c; }"
            );
        }

        msgBox.exec();
        log_console->appendPlainText(QString("[%1] [BİLGİ] Kontrol sonucu ekranda gösterildi.").arg(zaman));
        });

    p->start("cmd.exe", QStringList() << "/c" << "ping -n 1 pastebin.com");
}

void PowerToysDashboard::util_winsock_reset() {
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    log_console->appendPlainText(QString("\n[%1] [ARAÇLAR] Winsock (Ağ Katalogu) sıfırlanıyor...").arg(zaman));
    QProcess::execute("cmd.exe", QStringList() << "/c" << "netsh winsock reset");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "netsh int ip reset");
    log_console->appendPlainText(QString("[%1] [BAŞARI] Ağ ayarları sıfırlandı! Değişikliklerin tamamen uygulanması için bilgisayarınızı YENİDEN BAŞLATMANIZ gerekebilir.").arg(zaman));
}

void PowerToysDashboard::update_theme(bool is_dark) {
    is_dark_mode = is_dark;
    QString success_color = is_dark ? "#00E676" : "#008B00";
    QString fail_color = is_dark ? "#ff5252" : "#d32f2f";
    QString pending_color = is_dark ? "#ff9800" : "#d84315";

    if (is_dark) {
        block_quick->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        block_utils->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        title_quick->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        title_utils->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        lbl_iss->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 13px; margin-top: 15px; border: none; background: transparent;");

        combo_iss->setStyleSheet("QComboBox { background-color: #333333; color: white; border-radius: 6px; padding-left: 15px; border: 1px solid #444444; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #3a3a3a; border: 1px solid #555555; } QComboBox::drop-down { border: none; width: 30px; }");
        combo_iss->view()->setStyleSheet(
            "QListView { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; outline: none; padding: 4px; } "
            "QListView::item { min-height: 32px; padding-left: 11px; border-radius: 4px; margin-bottom: 2px; } "
            "QListView::item:selected { background-color: #444444; color: white; } "
            "QListView::item:hover { background-color: #6a1b9a; color: white; }"
        );

        for (auto btn : utility_btns) {
            btn->setStyleSheet("QPushButton { background-color: #333333; border-radius: 6px; color: #e0e0e0; text-align: left; padding-left: 15px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; } QPushButton:hover { background-color: #444444; } QPushButton:pressed { background-color: #555555; }");
        }
    }
    else {
        block_quick->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 8px; border: 1px solid #aaaaaa; }");
        block_utils->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 8px; border: 1px solid #aaaaaa; }");
        title_quick->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        title_utils->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        lbl_iss->setStyleSheet("color: #555555; font-family: 'Segoe UI Variable'; font-size: 13px; margin-top: 15px; border: none; background: transparent;");

        combo_iss->setStyleSheet("QComboBox { background-color: #f9f9f9; color: #1a1a1a; border-radius: 6px; padding-left: 15px; border: 1px solid #aaaaaa; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #e0e0e0; border: 1px solid #888888; } QComboBox::drop-down { border: none; width: 30px; }");
        combo_iss->view()->setStyleSheet(
            "QListView { background-color: #ffffff; color: #1a1a1a; border: 1px solid #cccccc; border-radius: 6px; outline: none; padding: 4px; } "
            "QListView::item { min-height: 32px; padding-left: 11px; border-radius: 4px; margin-bottom: 2px; } "
            "QListView::item:selected { background-color: #e0e0e0; color: black; } "
            "QListView::item:hover { background-color: #6a1b9a; color: white; }"
        );

        for (auto btn : utility_btns) {
            btn->setStyleSheet("QPushButton { background-color: #f5f5f5; border-radius: 6px; color: #333333; text-align: left; padding-left: 15px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: 1px solid #dddddd; } QPushButton:hover { background-color: #e8e8e8; border: 1px solid #cccccc; } QPushButton:pressed { background-color: #d5d5d5; }");
        }
    }

    if (lbl_active_mod->text().contains("Çalışan Mod:") || lbl_active_mod->text().contains("Mod: T") || lbl_active_mod->text().contains("Mod: S") || lbl_active_mod->text().contains("Mod: K") || lbl_active_mod->text().contains("Mod: V") || lbl_active_mod->text().contains("Mod: O")) {
        if (lbl_status->text().contains("başarıyla") || lbl_active_mod->text().contains("Son Çalışan")) {
            lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(success_color));
            if (lbl_status->text().contains("başarıyla")) lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(success_color));
        }
    }
    if (lbl_status->text().contains("Sistem Beklemede") || lbl_status->text().contains("Adım 1:")) {
        lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; background: transparent;").arg(is_dark ? "#777777" : "#555555"));
    }
    if (lbl_active_mod->text() == "Aktif Mod: BEKLENİYOR..." || lbl_active_mod->text().contains("YOK") || lbl_active_mod->text().contains("SANSÜR")) {
        lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(fail_color));
    }
}

void PowerToysDashboard::kill_dpi_service() {
    QString fail_color = is_dark_mode ? "#ff5252" : "#d32f2f";
    QString pending_color = is_dark_mode ? "#ff9800" : "#d84315";

    btn_start->setEnabled(true);
    btn_start->setText("OTONOM SİSTEMİ BAŞLAT");
    progress_bar->setValue(0);
    progress_bar->setStyleSheet("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: #8a2be2; border-radius: 3px; }");

    lbl_status->setText("⚠️ Sistem Durduruldu! Tüm DPI servisleri katledildi.");
    lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(pending_color));

    lbl_active_mod->setText("Aktif Mod: BEKLENİYOR...");
    lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(fail_color));

    log_console->appendPlainText(QString("\n[%1] [KILL DPI] Kırmızı Alarm! Katliam emri verildi!").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

    QString app_dir = QCoreApplication::applicationDirPath();
    QString bat_path = app_dir + "/kill_dpi.bat";
    QFile bat_file(bat_path);
    if (bat_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&bat_file);
        out << "@echo off\n";
        out << "taskkill /F /IM goodbyedpi.exe >nul 2>&1\n";
        out << "sc stop \"GoodbyeDPI\" >nul 2>&1\n";
        out << "sc delete \"GoodbyeDPI\" >nul 2>&1\n";
        out << "sc stop \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc delete \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc stop \"WinDivert\" >nul 2>&1\n";
        out << "sc stop \"WinDivert1.4\" >nul 2>&1\n";
        out << "ipconfig /flushdns >nul 2>&1\n";
        bat_file.close();
    }

    QProcess* kill_process = new QProcess(this);
    kill_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
    kill_process->start("cmd.exe", QStringList() << "/c" << bat_path);
}

void PowerToysDashboard::start_autonomous_system() {
    QString pending_color = is_dark_mode ? "#ff9800" : "#d84315";
    QString fail_color = is_dark_mode ? "#ff5252" : "#d32f2f";

    btn_start->setEnabled(false);
    btn_start->setText("SİSTEM ÇALIŞIYOR...");
    btn_start->setStyleSheet("QPushButton { background-color: #444444; color: #888888; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; }");

    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");

    log_console->appendPlainText(QString("\n[%1] [SİSTEM] Ön Bağlantı Kontrolü (Pre-flight Check) yapılıyor...").arg(zaman));
    QCoreApplication::processEvents();

    QProcess pre_ping;
    pre_ping.start("cmd.exe", QStringList() << "/c" << "ping -n 1 -w 1500 8.8.8.8");
    pre_ping.waitForFinished(2000);

    if (pre_ping.exitCode() != 0) {
        progress_bar->setValue(100);
        progress_bar->setStyleSheet("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: #ff5252; border-radius: 3px; }");

        lbl_status->setText("⚠️ BAĞLANTI HATASI: Lütfen internet bağlantınızı kontrol edin!");
        lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(fail_color));

        lbl_active_mod->setText("Durum: İNTERNET BAĞLANTISI YOK");
        lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(fail_color));

        log_console->appendPlainText(QString("[%1] [KRİTİK HATA] İnternet bağlantısı algılanamadı. İşlem iptal edildi.").arg(zaman));

        QProcess* silent_kill = new QProcess(this);
        silent_kill->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
        silent_kill->start("cmd.exe", QStringList() << "/c" << "taskkill /F /IM goodbyedpi.exe >nul 2>&1 & sc stop \"GoodbyeDPI_Elite\" >nul 2>&1");

        btn_start->setEnabled(true);
        btn_start->setText("YENİDEN DENE");
        btn_start->setStyleSheet(
            "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; } "
            "QPushButton:hover { background-color: #8e24aa; } "
            "QPushButton:pressed { background-color: #4a148c; }"
        );
        return;
    }

    int selected_iss = combo_iss->currentIndex();
    is_auto_detect_mode = (selected_iss == 0 || selected_iss == 4 || selected_iss == 5);

    if (is_auto_detect_mode) {
        auto_test_params.clear();
        auto_test_names.clear();

        // 🚀 BUM! İŞTE BİZİM YENİLMEZ 5'Lİ MERMİ DİZİLİMİMİZ!
        QString p_turknet = "-5 --set-ttl 5 --dns-addr 1.1.1.1 --dns-port 53 --dnsv6-addr 2606:4700:4700::1111 --dnsv6-port 53";
        QString n_turknet = "TurkNet / Cloudflare Modülü";

        QString p_ttnet = "-5 --set-ttl 5 --dns-addr 77.88.8.8 --dns-port 1253 --dnsv6-addr 2a02:6b8::feed:0ff --dnsv6-port 1253";
        QString n_ttnet = "Türk Telekom / Ortak Zırh Modülü";

        // 🎯 KAPTANIN İSTİHBARATI: Yakarnet ve bilinmeyenler için Joker!
        QString p_yakarnet = "-5";
        QString n_yakarnet = "Evrensel Otonom Joker Modülü (-5)";

        QString p_alt1 = "--set-ttl 3 --dns-addr 77.88.8.8 --dns-port 1253 --dnsv6-addr 2a02:6b8::feed:0ff --dnsv6-port 1253";
        QString n_alt1 = "Orijinal Alternatif Sürüm v1 (ttl 3)";

        QString p_alt2 = "-e 2 -f 1 --reverse-frag --set-ttl 5 --dns-addr 77.88.8.8 --dns-port 1253 --dnsv6-addr 2a02:6b8::feed:0ff --dnsv6-port 1253";
        QString n_alt2 = "Elite Son Çare Ağır Silahı (-f 1)";

        // 🎭 1. CEPHE (FACADE): Kullanıcı Vodafone veya Kablonet seçtiyse!
        if (selected_iss == 4 || selected_iss == 5) {
            QString target_name = (selected_iss == 4) ? "KABLONET" : "VODAFONE";
            log_console->appendPlainText(QString("\n[%1] [%2] Özel altyapı profili analiz ediliyor...").arg(zaman, target_name));
            log_console->appendPlainText(QString("[%1] [SİSTEM] 5 Aşamalı Otonom Motor %2 için ayarlandı!").arg(zaman, target_name));

            // Senin tam olarak ekranda çizdiğin o kusursuz sıralama!
            auto_test_params << p_turknet << p_ttnet << p_yakarnet << p_alt1 << p_alt2;
            auto_test_names << n_turknet << n_ttnet << n_yakarnet << n_alt1 << n_alt2;
        }
        // 📡 2. CEPHE: Kullanıcı "Otomatik Algıla" seçtiyse (IP-API ile Radar Taraması)
        else if (selected_iss == 0) {
            log_console->appendPlainText(QString("[%1] [YAPAY ZEKA] Otonom Tarama Başlatıldı!").arg(zaman));
            log_console->appendPlainText(QString("[%1] [RADAR] ISS Tespiti için ağa sinyal gönderiliyor...").arg(zaman));
            QCoreApplication::processEvents();

            QProcess curl_process;
            curl_process.start("curl", QStringList() << "-s" << "http://ip-api.com/line/?fields=isp");
            curl_process.waitForFinished(3000);
            QString isp_name = curl_process.readAllStandardOutput().trimmed().toUpper();

            if (isp_name.isEmpty()) {
                isp_name = "BİLİNMEYEN_ISS";
                log_console->appendPlainText(QString("[%1] [RADAR] ISS Tespit edilemedi! Standart hücum matrisi yükleniyor.").arg(zaman));
                auto_test_params << p_ttnet << p_turknet << p_yakarnet << p_alt1 << p_alt2;
                auto_test_names << n_ttnet << n_turknet << n_yakarnet << n_alt1 << n_alt2;
            }
            else {
                log_console->appendPlainText(QString("[%1] [RADAR] Tespit Edilen Gerçek ISS: %2").arg(zaman).arg(isp_name));

                // Eğer internet Turknet ise, Turknet kodunu başa al, sonra diğerlerini ekle
                if (isp_name.contains("TURKNET")) {
                    auto_test_params << p_turknet << p_ttnet << p_yakarnet << p_alt1 << p_alt2;
                    auto_test_names << n_turknet << n_ttnet << n_yakarnet << n_alt1 << n_alt2;
                }
                // Superonline veya Turkcell ise TT (Ortak Zırh) kodunu başa al
                else if (isp_name.contains("SUPERONLINE") || isp_name.contains("TURKCELL")) {
                    auto_test_params << p_ttnet << p_turknet << p_yakarnet << p_alt1 << p_alt2;
                    auto_test_names << n_ttnet << n_turknet << n_yakarnet << n_alt1 << n_alt2;
                }
                // Diğer tüm yerel sağlayıcılar (Örn: Yakarnet, Netspeed) için JOKER kodu (-5) başa al!
                else {
                    auto_test_params << p_yakarnet << p_ttnet << p_turknet << p_alt1 << p_alt2;
                    auto_test_names << n_yakarnet << n_ttnet << n_turknet << n_alt1 << n_alt2;
                }
            }
        }

        current_auto_test_index = 0;
        run_auto_test_step();
        return;
    }

    progress_bar->setValue(15);
    lbl_status->setText("Adım 1: Eski servisler temizleniyor...");
    lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; background: transparent;").arg(is_dark_mode ? "#e0e0e0" : "#333333"));
    lbl_active_mod->setText("Aktif Mod: SİSTEM TEST EDİLİYOR...");
    lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(pending_color));

    QString app_dir = QCoreApplication::applicationDirPath();
    QString exe_path = QDir::toNativeSeparators(app_dir + "/goodbyedpi/x86_64/goodbyedpi.exe");
    QString list_path = QDir::toNativeSeparators(app_dir + "/goodbyedpi/turkey-blacklist.txt");

    QString params = "";
    QString active_mod_text = "";

    if (selected_iss == 1) { params = "-5 --set-ttl 5 --dns-addr 77.88.8.8 --dns-port 1253 --dnsv6-addr 2a02:6b8::feed:0ff --dnsv6-port 1253"; active_mod_text = "SUPERONLINE"; }
    else if (selected_iss == 2) { params = "-5 --set-ttl 5 --dns-addr 77.88.8.8 --dns-port 1253 --dnsv6-addr 2a02:6b8::feed:0ff --dnsv6-port 1253"; active_mod_text = "TÜRK TELEKOM"; }
    else if (selected_iss == 3) { params = "-5 --set-ttl 5 --dns-addr 1.1.1.1 --dns-port 53 --dnsv6-addr 2606:4700:4700::1111 --dnsv6-port 53"; active_mod_text = "TURKNET"; }

    lbl_active_mod->setProperty("final_text", active_mod_text);
    log_console->appendPlainText(QString("\n[%1] [PARAM] Hedef Profil: %2").arg(zaman, active_mod_text));
    log_console->appendPlainText(QString("[%1] [PARAM] Kod: %2").arg(zaman).arg(params));

    current_run_param = params;

    QString bat_path = app_dir + "/elite_engine.bat";
    QFile bat_file(bat_path);
    if (bat_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&bat_file);
        out << "@echo off\n";
        out << "taskkill /F /IM goodbyedpi.exe >nul 2>&1\n";
        out << "sc stop \"GoodbyeDPI\" >nul 2>&1\n";
        out << "sc delete \"GoodbyeDPI\" >nul 2>&1\n";
        out << "sc stop \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc delete \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc stop \"WinDivert\" >nul 2>&1\n";
        out << "sc stop \"WinDivert1.4\" >nul 2>&1\n";
        out << "ipconfig /flushdns >nul 2>&1\n";
        out << "timeout /t 2 /nobreak >nul\n";

        QString bin_cmd = "\"\\\"" + exe_path + "\\\" " + params + " --blacklist \\\"" + list_path + "\\\"\"";
        out << "sc create \"GoodbyeDPI_Elite\" binPath= " << bin_cmd << " start= auto >nul 2>&1\n";
        out << "sc start \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        bat_file.close();
    }

    if (motor_process) { motor_process->kill(); delete motor_process; }
    motor_process = new QProcess(this);
    motor_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });

    QTimer::singleShot(1500, this, [this]() {
        progress_bar->setValue(45);
        lbl_status->setText("Adım 2: Zombi süreçler katlediliyor ve ağ sıfırlanıyor...");
        });

    QTimer::singleShot(3500, this, [this]() {
        progress_bar->setValue(75);
        lbl_status->setText("Adım 3: Seçilen profile göre Elite Motoru kuruluyor...");
        });

    QTimer::singleShot(6000, this, [this]() {
        on_service_started(0);
        });

    motor_process->start("cmd.exe", QStringList() << "/c" << bat_path);
}

void PowerToysDashboard::run_auto_test_step() {
    QString current_param = auto_test_params[current_auto_test_index];
    QString current_name = auto_test_names[current_auto_test_index];
    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString pending_color = is_dark_mode ? "#ff9800" : "#d84315";

    current_run_param = current_param;

    progress_bar->setValue(30);
    progress_bar->setStyleSheet("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: #ff9800; border-radius: 3px; }");
    lbl_status->setText(QString("Otonom Tarama (%1/%2): %3 deneniyor...").arg(current_auto_test_index + 1).arg(auto_test_params.size()).arg(current_name));
    lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(pending_color));

    lbl_active_mod->setText(QString("Tarama Sürüyor... (%1/%2)").arg(current_auto_test_index + 1).arg(auto_test_params.size()));
    lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(pending_color));

    log_console->appendPlainText(QString("\n[%1] [TEST %2] %3 modülüne geçildi.").arg(zaman).arg(current_auto_test_index + 1).arg(current_name));
    log_console->appendPlainText(QString("[%1] [PARAM] Kod: %2").arg(zaman).arg(current_param));

    QString app_dir = QCoreApplication::applicationDirPath();
    QString exe_path = QDir::toNativeSeparators(app_dir + "/goodbyedpi/x86_64/goodbyedpi.exe");
    QString list_path = QDir::toNativeSeparators(app_dir + "/goodbyedpi/turkey-blacklist.txt");
    QString bat_path = app_dir + "/elite_engine.bat";

    QFile bat_file(bat_path);
    if (bat_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&bat_file);
        out << "@echo off\n";
        out << "taskkill /F /IM goodbyedpi.exe >nul 2>&1\n";
        out << "sc stop \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc delete \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        out << "sc stop \"WinDivert\" >nul 2>&1\n";
        out << "ipconfig /flushdns >nul 2>&1\n";
        out << "timeout /t 1 /nobreak >nul\n";

        QString bin_cmd = "\"\\\"" + exe_path + "\\\" " + current_param + " --blacklist \\\"" + list_path + "\\\"\"";
        out << "sc create \"GoodbyeDPI_Elite\" binPath= " << bin_cmd << " start= auto >nul 2>&1\n";
        out << "sc start \"GoodbyeDPI_Elite\" >nul 2>&1\n";
        bat_file.close();
    }

    if (motor_process) { motor_process->kill(); delete motor_process; }
    motor_process = new QProcess(this);
    motor_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });

    connect(motor_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this]() {
        QTimer::singleShot(1500, this, [this]() {
            on_service_started(0);
            });
        });

    motor_process->start("cmd.exe", QStringList() << "/c" << bat_path);
}

void PowerToysDashboard::on_service_removed(int exitCode) {}
void PowerToysDashboard::on_service_installed(int exitCode) {}

void PowerToysDashboard::on_service_started(int exitCode) {
    progress_bar->setValue(85);
    lbl_status->setText("Aşama 2: DPI Duvarı Delinme Testi (Discord.com)...");

    log_console->appendPlainText(QString("[%1] [RADAR] Ağ çıkışı doğrulandı. Şimdi DPI sansür duvarı (Discord) test ediliyor...").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));

    if (ping_process) { ping_process->kill(); delete ping_process; }
    ping_process = new QProcess(this);
    ping_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
    connect(ping_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &PowerToysDashboard::verify_dpi_connection);

    ping_process->start("cmd.exe", QStringList() << "/c" << "ping -n 1 -w 2000 discord.com");
}

void PowerToysDashboard::verify_connection(int exitCode) {}

void PowerToysDashboard::verify_dpi_connection(int exitCode) {
    disconnect(ping_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &PowerToysDashboard::verify_dpi_connection);

    QString zaman = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString success_color = is_dark_mode ? "#00E676" : "#008B00";
    QString fail_color = is_dark_mode ? "#ff5252" : "#d32f2f";

    if (exitCode == 0) {
        QString success_text = is_auto_detect_mode ? auto_test_names[current_auto_test_index] : lbl_active_mod->property("final_text").toString();

        progress_bar->setValue(100);
        progress_bar->setStyleSheet("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: #00E676; border-radius: 3px; }");

        lbl_status->setText("Sistem başarıyla aktif edildi! Özgür internetin tadını çıkarın.");
        lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(success_color));

        lbl_active_mod->setText("Aktif Mod: " + success_text);
        lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(success_color));

        log_console->appendPlainText(QString("[%1] [BAŞARI] Duvar Kırıldı! İnternet Sansürü Aşıldı. İyi uçuşlar Kaptan!").arg(zaman));

        QSettings settings("TheTakanosu", "EliteEngine");
        settings.setValue("last_iss_index", combo_iss->currentIndex());
        settings.setValue("last_active_mod", success_text);
        settings.setValue("last_working_param", current_run_param);

        int total = settings.value("total_launches", 0).toInt();
        settings.setValue("total_launches", total + 1);

        btn_start->setEnabled(true);
        btn_start->setText("YENİDEN BAŞLAT / GÜNCELLE");
        btn_start->setStyleSheet(
            "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; } "
            "QPushButton:hover { background-color: #8e24aa; } "
            "QPushButton:pressed { background-color: #4a148c; }"
        );
    }
    else {
        if (is_auto_detect_mode) {
            log_console->appendPlainText(QString("[%1] [HATA] %2 duvara çarptı. Bir sonraki silah çekiliyor...").arg(zaman).arg(auto_test_names[current_auto_test_index]));
            current_auto_test_index++;

            if (current_auto_test_index < auto_test_params.size()) {
                run_auto_test_step();
            }
            else {
                progress_bar->setValue(100);
                progress_bar->setStyleSheet("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: #ff5252; border-radius: 3px; }");

                lbl_status->setText("⚠️ Bütün sürümler başarısız! Lütfen GitHub üzerinden bizimle iletişime geçiniz.");
                lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(fail_color));

                lbl_active_mod->setText("Durum: AĞIR SANSÜR TESPİT EDİLDİ");
                lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(fail_color));

                log_console->appendPlainText(QString("[%1] [KRİTİK HATA] Ağır DPI sansürü aşılamadı! Hiçbir sürüm çalışmadı.").arg(zaman));

                QProcess* silent_kill = new QProcess(this);
                silent_kill->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
                silent_kill->start("cmd.exe", QStringList() << "/c" << "taskkill /F /IM goodbyedpi.exe >nul 2>&1 & sc stop \"GoodbyeDPI_Elite\" >nul 2>&1");

                btn_start->setEnabled(true);
                btn_start->setText("YENİDEN DENE");
                btn_start->setStyleSheet(
                    "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; } "
                    "QPushButton:hover { background-color: #8e24aa; } "
                    "QPushButton:pressed { background-color: #4a148c; }"
                );
            }
        }
        else {
            progress_bar->setValue(100);
            progress_bar->setStyleSheet("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: #ff5252; border-radius: 3px; }");

            lbl_status->setText("⚠️ BAĞLANTI HATASI: Sansür duvarı aşılamadı (Discord Ping Başarısız)!");
            lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; font-weight: bold; background: transparent;").arg(fail_color));

            lbl_active_mod->setText("Durum: BAĞLANTI ENGELLENDİ");
            lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(fail_color));

            log_console->appendPlainText(QString("[%1] [HATA] Timeout! Discord pingi engellendi. Farklı profil deneyin!").arg(zaman));

            QProcess* silent_kill = new QProcess(this);
            silent_kill->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
            silent_kill->start("cmd.exe", QStringList() << "/c" << "taskkill /F /IM goodbyedpi.exe >nul 2>&1 & sc stop \"GoodbyeDPI_Elite\" >nul 2>&1");

            btn_start->setEnabled(true);
            btn_start->setText("YENİDEN BAŞLAT / GÜNCELLE");
            btn_start->setStyleSheet(
                "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; } "
                "QPushButton:hover { background-color: #8e24aa; } "
                "QPushButton:pressed { background-color: #4a148c; }"
            );
        }
    }
}

void PowerToysDashboard::resizeEvent(QResizeEvent* event) {
    if (this->width() < 650) {
        if (flex_layout->direction() != QBoxLayout::TopToBottom)
            flex_layout->setDirection(QBoxLayout::TopToBottom);
    }
    else {
        if (flex_layout->direction() != QBoxLayout::LeftToRight)
            flex_layout->setDirection(QBoxLayout::LeftToRight);
    }
    QWidget::resizeEvent(event);
}

TheTakanosu_Elite::TheTakanosu_Elite(QWidget* parent)
    : QMainWindow(parent), m_is_overlay_open(false), m_is_manually_collapsed(false)
{
    this->setWindowTitle("The Takanosu Elite");

    QString app_dir = QCoreApplication::applicationDirPath();
    this->setWindowIcon(QIcon(app_dir + "/assets/icon-transparant.png"));

    // 🚀 BUM! İLK KURULUM (FRESH INSTALL) ZIRHI!
    // Uygulama o bilgisayarda İLK DEFA açılıyorsa, kendini otomatik olarak Windows Başlangıcına yazar!
    QSettings settings("TheTakanosu", "EliteEngine");
    if (settings.value("is_first_run", true).toBool()) {
        QSettings boot_settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        QString app_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        boot_settings.setValue("TheTakanosu_Elite", "\"" + app_path + "\"");
        settings.setValue("is_first_run", false); // Bir daha bunu yapma diye hafızaya kaydeder
    }

    this->resize(1100, 750);
    this->setMinimumSize(400, 500);

    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setStyleSheet("QMainWindow { background-color: #1a1a1a; }");

    HWND hwnd = reinterpret_cast<HWND>(this->winId());
    int val = 2;
    DwmSetWindowAttribute(hwnd, 33, &val, sizeof(val));
    int dark = 1;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    SetWindowLongW(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

    setupUi();
    setup_tray_icon();

    bool is_dark = settings.value("is_dark_theme", true).toBool();
    set_theme(is_dark);
    QTimer::singleShot(1000, this, &TheTakanosu_Elite::check_updates);
}

void TheTakanosu_Elite::setupUi() {
    central_widget = new QWidget(this);
    this->setCentralWidget(central_widget);

    main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    title_bar = new QWidget();
    title_bar->setObjectName("TitleBar");
    title_bar->setFixedHeight(40);

    QHBoxLayout* title_layout = new QHBoxLayout(title_bar);
    title_layout->setContentsMargins(15, 0, 0, 0);

    QString app_dir = QCoreApplication::applicationDirPath();
    logo_label = new QLabel();
    logo_label->setFixedSize(20, 20);
    logo_label->setScaledContents(true);
    logo_label->setPixmap(QPixmap(app_dir + "/assets/icon-transparant.png"));
    title_layout->addWidget(logo_label);

    title_label = new QLabel("The Takanosu Elite");
    title_label->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; margin-left: 5px; background: transparent;");
    title_layout->addWidget(title_label);
    title_layout->addStretch();

    btn_min = new QPushButton(QString::fromUtf8("\uE921"));
    btn_max = new QPushButton(QString::fromUtf8("\uE922"));
    btn_close = new QPushButton(QString::fromUtf8("\uE8BB"));

    QString btn_style = "QPushButton { color: #e0e0e0; background-color: transparent; border: none; font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 11px; width: 45px; height: 40px; } QPushButton:hover { background-color: #333333; color: white; }";
    btn_min->setStyleSheet(btn_style);
    btn_max->setStyleSheet(btn_style);
    btn_close->setStyleSheet(btn_style + "QPushButton:hover { background-color: #c42b1c; color: white; }");

    connect(btn_min, &QPushButton::clicked, this, &TheTakanosu_Elite::showMinimized);
    connect(btn_max, &QPushButton::clicked, this, &TheTakanosu_Elite::toggle_maximize);
    connect(btn_close, &QPushButton::clicked, this, &TheTakanosu_Elite::hide);

    title_layout->addWidget(btn_min);
    title_layout->addWidget(btn_max);
    title_layout->addWidget(btn_close);
    main_layout->addWidget(title_bar);

    body_widget = new QWidget();
    body_layout = new QHBoxLayout(body_widget);
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);
    main_layout->addWidget(body_widget);

    sidebar_placeholder = new QWidget();
    sidebar_placeholder->setFixedWidth(240);
    sidebar_placeholder->setStyleSheet("background-color: transparent;");
    body_layout->addWidget(sidebar_placeholder);

    main_panel = new QWidget();
    main_panel->setObjectName("MainPanel");
    main_panel_layout = new QVBoxLayout(main_panel);
    main_panel_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->addWidget(main_panel);

    stacked_widget = new QStackedWidget();
    main_panel_layout->addWidget(stacked_widget);

    dashboard_widget = new PowerToysDashboard();
    stacked_widget->addWidget(create_powertoys_dashboard());
    stacked_widget->addWidget(create_mod_settings_page());
    stacked_widget->addWidget(create_stats_page());
    stacked_widget->addWidget(create_tools_page());

    sidebar = new QFrame(body_widget);
    sidebar->setObjectName("Sidebar");
    sidebar->setFixedWidth(240);

    sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(10, 10, 10, 20);
    sidebar_layout->setSpacing(5);

    btn_hamburger = new QPushButton(QString::fromUtf8("\uE700"));
    btn_hamburger->setFixedSize(40, 40);
    btn_hamburger->setCursor(Qt::PointingHandCursor);
    btn_hamburger->setStyleSheet("QPushButton { color: #e0e0e0; background-color: transparent; border-radius: 6px; font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 15px; border: none; } QPushButton:hover { background-color: #2d2d2d; }");
    connect(btn_hamburger, &QPushButton::clicked, this, &TheTakanosu_Elite::toggle_hamburger);
    sidebar_layout->addWidget(btn_hamburger, 0, Qt::AlignLeft);

    add_sidebar_btn(QString::fromUtf8("🏠"), "Ana Ekran", 0);
    add_sidebar_btn(QString::fromUtf8("⚙️"), "Mod Ayarları", 1);
    add_sidebar_btn(QString::fromUtf8("📊"), "İstatistikler", 2);
    add_sidebar_btn(QString::fromUtf8("🛠️"), "Araçlar", 3);
    sidebar_layout->addStretch();
}

void TheTakanosu_Elite::setup_tray_icon() {
    QString app_dir = QCoreApplication::applicationDirPath();
    QIcon tray_icon_img(app_dir + "/assets/icon.ico");
    tray_icon = new QSystemTrayIcon(this);
    tray_icon->setIcon(tray_icon_img);

    tray_menu = new QMenu(this);
    tray_menu->setWindowFlags(tray_menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    tray_menu->setAttribute(Qt::WA_TranslucentBackground);

    tray_menu->setStyleSheet(
        "QMenu { background-color: #202020; border: 1px solid #333333; border-radius: 8px; padding: 5px; } "
        "QMenu::item { color: #e0e0e0; padding: 8px 25px 8px 15px; background-color: transparent; border-radius: 4px; font-family: 'Segoe UI Variable'; font-size: 12px; } "
        "QMenu::item:selected { background-color: #333333; color: white; } "
        "QMenu::separator { height: 1px; background: #333333; margin: 5px 10px; } "
    );

    QAction* act_open = new QAction("The Takanosu Elite'i Aç", this);
    QAction* act_update = new QAction("Güncellemeleri Denetle...", this);
    QAction* act_github = new QAction("GitHub Repository", this);
    QAction* act_hide = new QAction("Tepsiyi Gizle (Hide System Tray)", this);
    QAction* act_quit = new QAction("GoodbyeDPI Elite'den Çık", this);

    connect(act_open, &QAction::triggered, this, &TheTakanosu_Elite::showNormal);
    connect(act_github, &QAction::triggered, []() { QDesktopServices::openUrl(QUrl("https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition")); });
    connect(act_hide, &QAction::triggered, [this]() { tray_icon->hide(); });
    connect(act_quit, &QAction::triggered, [this]() { tray_icon->hide(); qApp->quit(); });

    tray_menu->addAction(act_open);
    tray_menu->addSeparator();
    tray_menu->addAction(act_update);
    tray_menu->addAction(act_github);
    tray_menu->addSeparator();
    tray_menu->addAction(act_hide);
    tray_menu->addAction(act_quit);

    tray_icon->setContextMenu(tray_menu);
    tray_icon->show();

    connect(tray_icon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            this->showNormal();
            this->activateWindow();
        }
        });
}

void TheTakanosu_Elite::add_sidebar_btn(const QString& icon, const QString& text, int index) {
    QPushButton* btn = new QPushButton(QString("%1  %2").arg(icon, text));
    btn->setFixedHeight(40);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("full_text", QString("%1  %2").arg(icon, text));
    btn->setProperty("icon_only", icon);
    btn->setStyleSheet("QPushButton { color: #e0e0e0; background-color: transparent; border-radius: 6px; text-align: left; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; } "
        "QPushButton:hover { background-color: #2d2d2d; } QPushButton:pressed { background-color: #3d3d3d; color: white; }");

    connect(btn, &QPushButton::clicked, [this, index]() {
        stacked_widget->setCurrentIndex(index);
        if (this->width() < 920 && m_is_overlay_open) {
            toggle_hamburger();
        }
        });
    sidebar_layout->addWidget(btn);
    sidebar_btns.append(btn);
}

QScrollArea* TheTakanosu_Elite::create_powertoys_dashboard() {
    QScrollArea* scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 🎨 BUM! ANA EKRAN İÇİN KUSURSUZ YUVARLAK UÇLU ÇUBUK!
    scroll_area->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; } "
        "QScrollBar:vertical { border: none; background: transparent; width: 14px; margin: 0px; } "
        "QScrollBar::handle:vertical { background: #444444; min-height: 30px; border-radius: 2px; margin-left: 10px; margin-right: 0px; } " /* 4px kalınlık -> 2px yarıçap */
        "QScrollBar::handle:vertical:hover { background: #777777; border-radius: 5px; margin-left: 4px; } " /* 10px şişti -> 5px yarıçap */
        "QScrollBar::handle:vertical:pressed { background: #555555; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    scroll_area->setWidget(dashboard_widget);
    return scroll_area;
}

QWidget* TheTakanosu_Elite::create_mod_settings_page() {
    QWidget* page = new QWidget();
    page->setStyleSheet("background-color: transparent;");
    QVBoxLayout* main_layout = new QVBoxLayout(page);
    main_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    main_layout->setContentsMargins(20, 30, 20, 20);

    QWidget* container = new QWidget();
    container->setMaximumWidth(800);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setSpacing(25);

    mod_settings_header = new QLabel("⚙️ Mod ve Tema Ayarları");
    mod_settings_header->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
    layout->addWidget(mod_settings_header);

    theme_block = new QFrame();
    theme_block->setObjectName("TargetBlock");
    theme_block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    theme_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 12px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* theme_layout = new QVBoxLayout(theme_block);
    theme_layout->setContentsMargins(20, 20, 20, 20);
    theme_layout->setSpacing(15);

    theme_title = new QLabel("Arayüz Görünümü (Theme Selection)");
    theme_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    theme_layout->addWidget(theme_title);

    QVBoxLayout* btn_v_layout = new QVBoxLayout();
    btn_v_layout->setSpacing(12);

    btn_dark_theme = new QPushButton("🌙 Koyu Tema (Aktif)");
    btn_dark_theme->setFixedHeight(50);
    btn_dark_theme->setCursor(Qt::PointingHandCursor);

    btn_light_theme = new QPushButton("☀️ Açık Tema");
    btn_light_theme->setFixedHeight(50);
    btn_light_theme->setCursor(Qt::PointingHandCursor);

    btn_v_layout->addWidget(btn_dark_theme);
    btn_v_layout->addWidget(btn_light_theme);

    connect(btn_dark_theme, &QPushButton::clicked, this, [this]() { set_theme(true); });
    connect(btn_light_theme, &QPushButton::clicked, this, [this]() { set_theme(false); });

    theme_layout->addLayout(btn_v_layout);
    layout->addWidget(theme_block);

    lang_block = new QFrame();
    lang_block->setObjectName("TargetBlock");
    lang_block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lang_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* lang_layout = new QVBoxLayout(lang_block);
    lang_layout->setContentsMargins(20, 20, 20, 20);
    lang_layout->setSpacing(15);

    lang_title = new QLabel("🌍 Uygulama Dili (Language)");
    lang_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    lang_layout->addWidget(lang_title);

    combo_lang = new QComboBox();
    combo_lang->addItems({ "[TR] Türkçe", "[EN] English (Coming Soon)" });
    combo_lang->setFixedHeight(38);
    combo_lang->setCursor(Qt::PointingHandCursor);

    QListView* lang_list_view = new QListView();
    lang_list_view->setCursor(Qt::PointingHandCursor);
    combo_lang->setView(lang_list_view);
    combo_lang->setItemDelegate(new QStyledItemDelegate());

    lang_layout->addWidget(combo_lang);
    layout->addWidget(lang_block);

    sys_block = new QFrame();
    sys_block->setObjectName("TargetBlock");
    sys_block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sys_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* sys_layout = new QVBoxLayout(sys_block);
    sys_layout->setContentsMargins(20, 20, 20, 20);
    sys_layout->setSpacing(15);

    sys_title = new QLabel("⚙️ Sistem Davranışı (System Behavior)");
    sys_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    sys_layout->addWidget(sys_title);

    // ---------------------------------------------------------
    // 🕵️‍♂️ SİBER İSTİHBARAT: SAF WİNDOWS API İLE DURUM OKUMA
    // ---------------------------------------------------------
    bool is_startup = false;
    HKEY hKeyRun;

    // 1. Windows'un RUN klasöründe var mıyız?
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKeyRun) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKeyRun, L"TheTakanosu_Elite", NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            is_startup = true; // Şimdilik aktif kabul et

            // 2. GÖREV YÖNETİCİSİ bizi devredışı bırakmış mı (0x03) kontrol et!
            HKEY hKeyApproved;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run", 0, KEY_READ, &hKeyApproved) == ERROR_SUCCESS) {
                DWORD type;
                BYTE data[12];
                DWORD dataSize = sizeof(data);

                if (RegQueryValueExW(hKeyApproved, L"TheTakanosu_Elite", NULL, &type, data, &dataSize) == ERROR_SUCCESS) {
                    // Eğer Görev Yöneticisinden gelen saf binary'nin ilk baytı 0x03 ise, KAPATILMIŞIZDIR!
                    if (type == REG_BINARY && dataSize > 0 && data[0] == 0x03) {
                        is_startup = false;
                    }
                }
                RegCloseKey(hKeyApproved);
            }
        }
        RegCloseKey(hKeyRun);
    }

    btn_toggle_startup = new QPushButton(is_startup ? "✔️ Başlangıçta Çalıştır (AÇIK)" : "❌ Başlangıçta Çalıştır (KAPALI)");
    btn_toggle_startup->setCheckable(true);
    btn_toggle_startup->setChecked(is_startup);
    btn_toggle_startup->setFixedHeight(45);
    btn_toggle_startup->setCursor(Qt::PointingHandCursor);

    // Açılışta butonun tasarımını (Mor veya Sönük Gri) duruma ve doğru fontla giydir!
    if (is_startup) {
        btn_toggle_startup->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; font-family: 'Segoe UI Variable', sans-serif; font-size: 14px; font-weight: bold; border-radius: 6px; border: none; }");
    }
    else {
        btn_toggle_startup->setStyleSheet("QPushButton { background-color: #2b2b2b; color: #a0a0a0; font-family: 'Segoe UI Variable', sans-serif; font-size: 14px; font-weight: bold; border-radius: 6px; border: 1px solid #444444; }");
    }

    connect(btn_toggle_startup, &QPushButton::clicked, this, &TheTakanosu_Elite::toggle_startup_setting);
    sys_layout->addWidget(btn_toggle_startup);
    // ---------------------------------------------------------

    btn_toggle_tray = new QPushButton("✔️ Sistem Tepsisinde Göster (AÇIK)");
    btn_toggle_tray->setCheckable(true);
    btn_toggle_tray->setChecked(true);
    btn_toggle_tray->setFixedHeight(45);
    btn_toggle_tray->setCursor(Qt::PointingHandCursor);

    // 🚀 BUM! TEPSİ BUTONUNA DA KUSURSUZ FONT ZIRHI!
    btn_toggle_tray->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; font-family: 'Segoe UI Variable', sans-serif; font-size: 14px; font-weight: bold; border-radius: 6px; border: none; }");

    connect(btn_toggle_tray, &QPushButton::clicked, this, &TheTakanosu_Elite::toggle_tray_setting);
    sys_layout->addWidget(btn_toggle_tray);

    layout->addWidget(sys_block);

    layout->addStretch();
    main_layout->addWidget(container);

    // ---------------------------------------------------------
    // 🚀 BUM! TOST OLMAYI ENGELLEYEN SİBER KAYDIRMA KALKANI
    // ---------------------------------------------------------
    QScrollArea* scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Bizim o efsanevi, incecik, hap şeklindeki kaydırma çubuğu!
    scroll_area->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; } "
        "QScrollBar:vertical { border: none; background: transparent; width: 14px; margin: 0px; } "
        "QScrollBar::handle:vertical { background: #444444; min-height: 30px; border-radius: 2px; margin-left: 10px; margin-right: 0px; } "
        "QScrollBar::handle:vertical:hover { background: #777777; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::handle:vertical:pressed { background: #555555; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    scroll_area->setWidget(page);
    return scroll_area;
}

QWidget* TheTakanosu_Elite::create_stats_page() {
    QWidget* page = new QWidget();
    page->setStyleSheet("background-color: transparent;");
    QVBoxLayout* main_layout = new QVBoxLayout(page);
    main_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    main_layout->setContentsMargins(20, 30, 20, 20);

    QWidget* container = new QWidget();
    container->setMaximumWidth(800);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setSpacing(25);

    stats_header = new QLabel("📊 Ağ İstihbarat Merkezi");
    stats_header->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
    layout->addWidget(stats_header);

    stats_block = new QFrame();
    stats_block->setObjectName("TargetBlock");
    stats_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 12px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* st_layout = new QVBoxLayout(stats_block);
    st_layout->setContentsMargins(30, 30, 30, 30);
    st_layout->setSpacing(25);

    stats_title = new QLabel("DPI Motoru Telemetrisi");
    stats_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    st_layout->addWidget(stats_title);

    QSettings settings("TheTakanosu", "EliteEngine");
    int total = settings.value("total_launches", 0).toInt();
    QString last_mod = settings.value("last_active_mod", "Henüz Çalıştırılmadı").toString();

    QHBoxLayout* row1 = new QHBoxLayout();
    stat_1_desc = new QLabel("Toplam Başarılı Bypass:");
    stat_1_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 14px; background: transparent;");
    stat_1_val = new QLabel(QString::number(total));
    stat_1_val->setStyleSheet("color: #00E676; font-family: 'Consolas'; font-size: 18px; font-weight: bold; background: transparent;");
    row1->addWidget(stat_1_desc);
    row1->addStretch();
    row1->addWidget(stat_1_val);
    st_layout->addLayout(row1);

    QFrame* line1 = new QFrame(); line1->setFrameShape(QFrame::HLine); line1->setStyleSheet("background-color: #444444;");
    st_layout->addWidget(line1);

    QHBoxLayout* row2 = new QHBoxLayout();
    stat_2_desc = new QLabel("Son Aktif Kalkan Profil:");
    stat_2_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 14px; background: transparent;");
    stat_2_val = new QLabel(last_mod);
    stat_2_val->setStyleSheet("color: #ff9800; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; background: transparent;");
    row2->addWidget(stat_2_desc);
    row2->addStretch();
    row2->addWidget(stat_2_val);
    st_layout->addLayout(row2);

    layout->addWidget(stats_block);

    QLabel* info_desc = new QLabel("Gizlilik (Privacy) Bildirimi: The Takanosu Elite, canlı yayın (Streaming) veya ekran paylaşımı esnasında kişisel güvenliğinizi korumak amacıyla IP, DNS veya MAC adreslerinizi ekranda göstermez. Tüm paket filtreleme işlemleri çekirdek seviyesinde (Ring 0) kapalı devre olarak yapılır.");
    info_desc->setStyleSheet("color: #777777; font-family: 'Segoe UI Variable'; font-size: 12px; margin-top: 10px; background: transparent;");
    info_desc->setWordWrap(true);
    layout->addWidget(info_desc);

    layout->addStretch();
    main_layout->addWidget(container);

    // ---------------------------------------------------------
    // 🚀 BUM! İSTATİSTİKLER İÇİN SİBER KAYDIRMA KALKANI
    // ---------------------------------------------------------
    QScrollArea* scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scroll_area->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; } "
        "QScrollBar:vertical { border: none; background: transparent; width: 14px; margin: 0px; } "
        "QScrollBar::handle:vertical { background: #444444; min-height: 30px; border-radius: 2px; margin-left: 10px; margin-right: 0px; } "
        "QScrollBar::handle:vertical:hover { background: #777777; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::handle:vertical:pressed { background: #555555; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    scroll_area->setWidget(page);
    return scroll_area;
}

QWidget* TheTakanosu_Elite::create_tools_page() {
    QWidget* page = new QWidget();
    page->setStyleSheet("background-color: transparent;");
    QVBoxLayout* main_layout = new QVBoxLayout(page);
    main_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    main_layout->setContentsMargins(20, 30, 20, 20);

    QWidget* container = new QWidget();
    container->setMaximumWidth(800);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setSpacing(25);

    tools_header = new QLabel("🛠️ Gelişmiş Araçlar");
    tools_header->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
    layout->addWidget(tools_header);

    blacklist_block = new QFrame();
    blacklist_block->setObjectName("TargetBlock");
    blacklist_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 12px; border: 1px solid #3a3a3a; }");
    QVBoxLayout* bl_layout = new QVBoxLayout(blacklist_block);
    bl_layout->setContentsMargins(20, 20, 20, 20);
    bl_layout->setSpacing(15);

    bl_title = new QLabel("📜 Özel Kara Liste Yöneticisi (Custom Blacklist)");
    bl_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
    bl_layout->addWidget(bl_title);

    bl_desc = new QLabel("DPI bypass motorunun filtreleyeceği ekstra domainleri (site adreslerini) buraya ekleyebilirsiniz.\nÖrnek: roblox.com (Sadece alan adını yazınız, http veya www eklemeyiniz.)");
    bl_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 13px; border: none; background: transparent;");
    bl_desc->setWordWrap(true);
    bl_layout->addWidget(bl_desc);

    search_domain = new QLineEdit();
    search_domain->setPlaceholderText("🔍 Listede 550+ domain içinde ara...");
    search_domain->setFixedHeight(38);
    search_domain->setStyleSheet("QLineEdit { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 13px; } QLineEdit:focus { border: 1px solid #00E676; }");
    connect(search_domain, &QLineEdit::textChanged, this, &TheTakanosu_Elite::filter_blacklist);
    bl_layout->addWidget(search_domain);

    QHBoxLayout* input_layout = new QHBoxLayout();

    input_new_domain = new QLineEdit();
    input_new_domain->setPlaceholderText("Örn: yasaklisite.com");
    input_new_domain->setFixedHeight(40);
    input_new_domain->setStyleSheet("QLineEdit { background-color: #1a1a1a; color: white; border: 1px solid #444444; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 14px; } QLineEdit:focus { border: 1px solid #6a1b9a; }");
    input_layout->addWidget(input_new_domain);

    QPushButton* btn_add = new QPushButton("Ekle");
    btn_add->setFixedHeight(40);
    btn_add->setFixedWidth(100);
    btn_add->setCursor(Qt::PointingHandCursor);
    btn_add->setStyleSheet("QPushButton { background-color: #00E676; color: #1a1a1a; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; } QPushButton:hover { background-color: #00C853; } QPushButton:pressed { background-color: #00B248; }");
    connect(btn_add, &QPushButton::clicked, this, &TheTakanosu_Elite::add_to_blacklist);
    input_layout->addWidget(btn_add);

    bl_layout->addLayout(input_layout);

    blacklist_widget = new QListWidget();
    blacklist_widget->setFixedHeight(250);
    blacklist_widget->setStyleSheet(
        "QListWidget { background-color: #1a1a1a; color: #e0e0e0; border: 1px solid #444444; border-radius: 6px; padding: 5px; font-family: 'Consolas', monospace; font-size: 13px; outline: none; } "
        "QListWidget::item { padding: 2px 5px; min-height: 22px; border-radius: 4px; } "
        "QListWidget::item:selected { background-color: #6a1b9a; color: white; } "
    );
    bl_layout->addWidget(blacklist_widget);

    QPushButton* btn_remove = new QPushButton("Seçili Domaini Sil");
    btn_remove->setFixedHeight(40);
    btn_remove->setCursor(Qt::PointingHandCursor);
    btn_remove->setStyleSheet("QPushButton { background-color: #ff5252; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; } QPushButton:hover { background-color: #e53935; } QPushButton:pressed { background-color: #c62828; }");
    connect(btn_remove, &QPushButton::clicked, this, &TheTakanosu_Elite::remove_from_blacklist);
    bl_layout->addWidget(btn_remove);

    layout->addWidget(blacklist_block);

    layout->addStretch();
    main_layout->addWidget(container);

    load_blacklist();

    // ---------------------------------------------------------
    // 🚀 BUM! ARAÇLAR İÇİN SİBER KAYDIRMA KALKANI
    // ---------------------------------------------------------
    QScrollArea* scroll_area = new QScrollArea();
    scroll_area->setWidgetResizable(true);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scroll_area->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; } "
        "QScrollBar:vertical { border: none; background: transparent; width: 14px; margin: 0px; } "
        "QScrollBar::handle:vertical { background: #444444; min-height: 30px; border-radius: 2px; margin-left: 10px; margin-right: 0px; } "
        "QScrollBar::handle:vertical:hover { background: #777777; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::handle:vertical:pressed { background: #555555; border-radius: 5px; margin-left: 4px; } "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    scroll_area->setWidget(page);
    return scroll_area;
}

void TheTakanosu_Elite::filter_blacklist(const QString& text) {
    for (int i = 0; i < blacklist_widget->count(); ++i) {
        QListWidgetItem* item = blacklist_widget->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void TheTakanosu_Elite::load_blacklist() {
    blacklist_widget->clear();
    QString app_dir = QCoreApplication::applicationDirPath();
    QString list_path = app_dir + "/goodbyedpi/turkey-blacklist.txt";

    QFile file(list_path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                blacklist_widget->addItem(line);
            }
        }
        file.close();
    }
    else {
        blacklist_widget->addItem("⚠️ Liste dosyası bulunamadı! (turkey-blacklist.txt)");
    }
}

void TheTakanosu_Elite::add_to_blacklist() {
    QString new_domain = input_new_domain->text().trimmed().toLower();

    new_domain.remove("http://");
    new_domain.remove("https://");
    new_domain.remove("www.");
    new_domain.remove("/");

    if (new_domain.isEmpty()) return;

    QList<QListWidgetItem*> items = blacklist_widget->findItems(new_domain, Qt::MatchExactly);
    if (!items.isEmpty()) {
        input_new_domain->clear();
        return;
    }

    QString app_dir = QCoreApplication::applicationDirPath();
    QString list_path = app_dir + "/goodbyedpi/turkey-blacklist.txt";

    QFile file(list_path);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "\n" << new_domain;
        file.close();

        blacklist_widget->addItem(new_domain);
        input_new_domain->clear();
        blacklist_widget->scrollToBottom();
    }
}

void TheTakanosu_Elite::remove_from_blacklist() {
    QListWidgetItem* selected_item = blacklist_widget->currentItem();
    if (!selected_item) return;

    QString domain_to_remove = selected_item->text();

    QString app_dir = QCoreApplication::applicationDirPath();
    QString list_path = app_dir + "/goodbyedpi/turkey-blacklist.txt";

    QStringList all_lines;
    QFile file_in(list_path);
    if (file_in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file_in);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty() && line != domain_to_remove) {
                all_lines.append(line);
            }
        }
        file_in.close();
    }

    QFile file_out(list_path);
    if (file_out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file_out);
        for (int i = 0; i < all_lines.size(); ++i) {
            out << all_lines[i];
            if (i < all_lines.size() - 1) out << "\n";
        }
        file_out.close();
    }

    delete blacklist_widget->takeItem(blacklist_widget->row(selected_item));
}

void TheTakanosu_Elite::toggle_startup_setting() {
    bool checked = btn_toggle_startup->isChecked();

    // Uygulama yolunu çek ve Windows'un anladığı wstring formatına çevir
    QString app_path = "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\"";
    std::wstring w_app_path = app_path.toStdWString();
    std::wstring w_key_name = L"TheTakanosu_Elite";

    HKEY hKeyRun;
    // 1. RUN KLASÖRÜNE SAF YAZMA İŞLEMİ (Windows API)
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyRun, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKeyRun, w_key_name.c_str(), 0, REG_SZ, (const BYTE*)w_app_path.c_str(), (w_app_path.size() + 1) * sizeof(wchar_t));
        RegCloseKey(hKeyRun);
    }

    HKEY hKeyApproved;
    // 2. STARTUP_APPROVED KLASÖRÜNE "DEVRE DIŞI / ETKİN" ZORLAMASI (Windows API)
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyApproved, NULL) == ERROR_SUCCESS) {
        if (checked) {
            // 🚀 ETKİN (Enabled) Kodu: İlk Bayt 0x02
            BYTE enabled_data[12] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            RegSetValueExW(hKeyApproved, w_key_name.c_str(), 0, REG_BINARY, enabled_data, sizeof(enabled_data));

            btn_toggle_startup->setText("✔️ Başlangıçta Çalıştır (AÇIK)");
            btn_toggle_startup->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; font-family: 'Segoe UI Variable', sans-serif; font-size: 14px; font-weight: bold; border-radius: 6px; border: none; }");
        }
        else {
            // 🛑 DEVRE DIŞI (Disabled) Kodu: İlk Bayt 0x03
            BYTE disabled_data[12] = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
            RegSetValueExW(hKeyApproved, w_key_name.c_str(), 0, REG_BINARY, disabled_data, sizeof(disabled_data));

            btn_toggle_startup->setText("❌ Başlangıçta Çalıştır (KAPALI)");
            btn_toggle_startup->setStyleSheet("QPushButton { background-color: #2b2b2b; color: #a0a0a0; font-family: 'Segoe UI Variable', sans-serif; font-size: 14px; font-weight: bold; border-radius: 6px; border: 1px solid #444444; }");
        }
        RegCloseKey(hKeyApproved);
    }
}

void TheTakanosu_Elite::toggle_tray_setting() {
    if (btn_toggle_tray->isChecked()) {
        tray_icon->show();
        btn_toggle_tray->setText("✔️ Sistem Tepsisinde Göster (AÇIK)");
        btn_toggle_tray->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
    }
    else {
        tray_icon->hide();
        btn_toggle_tray->setText("❌ Sistem Tepsisinde Göster (KAPALI)");
        btn_toggle_tray->setStyleSheet("QPushButton { background-color: #333333; color: #a0a0a0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; }");
    }
}

void TheTakanosu_Elite::set_theme(bool is_dark) {
    if (dashboard_widget) { dashboard_widget->update_theme(is_dark); }

    QSettings settings("TheTakanosu", "EliteEngine");
    settings.setValue("is_dark_theme", is_dark);

    if (is_dark) {
        this->setStyleSheet("QMainWindow { background-color: #1a1a1a; }");
        title_bar->setStyleSheet("QWidget#TitleBar { background-color: #1a1a1a; }");
        sidebar->setStyleSheet("QFrame#Sidebar { background-color: #1c1c1c; border-right: 1px solid #2d2d2d; }");
        main_panel->setStyleSheet("QWidget#MainPanel { background-color: #202020; border: none; }");
        title_label->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; margin-left: 5px; background: transparent;");

        if (mod_settings_header) mod_settings_header->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
        if (theme_block) theme_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        if (theme_title) theme_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (lang_block) lang_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        if (lang_title) lang_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (combo_lang) {
            combo_lang->setStyleSheet("QComboBox { background-color: #333333; color: white; border-radius: 6px; padding-left: 15px; border: 1px solid #444444; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #3a3a3a; border: 1px solid #555555; } QComboBox::drop-down { border: none; width: 30px; }");
            combo_lang->view()->setStyleSheet("QListView { background-color: #2b2b2b; color: white; border: 1px solid #444444; border-radius: 6px; outline: none; padding: 4px; } QListView::item { min-height: 32px; padding-left: 10px; border-radius: 4px; margin-bottom: 2px; } QListView::item:selected { background-color: #353535; color: #a0a0a0; } QListView::item:hover { background-color: #6a1b9a; color: white; }");
        }
        if (sys_block) sys_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        if (sys_title) sys_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");

        if (tools_header) tools_header->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
        if (blacklist_block) blacklist_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 12px; border: 1px solid #3a3a3a; }");
        if (bl_title) bl_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (bl_desc) bl_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 13px; border: none; background: transparent;");

        if (blacklist_widget) {
            blacklist_widget->setStyleSheet(
                "QListWidget { background-color: #1a1a1a; color: #e0e0e0; border: 1px solid #444444; border-radius: 6px; padding: 5px; font-family: 'Consolas', monospace; font-size: 13px; outline: none; } "
                "QListWidget::item { padding: 2px 5px; min-height: 22px; border-radius: 4px; } "
                "QListWidget::item:selected { background-color: #6a1b9a; color: white; } "
            );
        }
        if (input_new_domain) {
            input_new_domain->setStyleSheet("QLineEdit { background-color: #1a1a1a; color: white; border: 1px solid #444444; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 14px; } QLineEdit:focus { border: 1px solid #6a1b9a; }");
        }
        if (search_domain) {
            search_domain->setStyleSheet("QLineEdit { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 13px; } QLineEdit:focus { border: 1px solid #00E676; }");
        }

        if (stats_header) stats_header->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
        if (stats_block) stats_block->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 12px; border: 1px solid #3a3a3a; }");
        if (stats_title) stats_title->setStyleSheet("color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (stat_1_desc) stat_1_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 14px; background: transparent;");
        if (stat_2_desc) stat_2_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 14px; background: transparent;");

        if (btn_toggle_startup) {
            if (btn_toggle_startup->isChecked()) btn_toggle_startup->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
            else btn_toggle_startup->setStyleSheet("QPushButton { background-color: #333333; color: #a0a0a0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; }");
        }
        if (btn_toggle_tray) {
            if (btn_toggle_tray->isChecked()) btn_toggle_tray->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
            else btn_toggle_tray->setStyleSheet("QPushButton { background-color: #333333; color: #a0a0a0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; }");
        }

        for (auto btn : sidebar_btns) {
            btn->setStyleSheet("QPushButton { color: #e0e0e0; background-color: transparent; border-radius: 6px; text-align: left; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; } QPushButton:hover { background-color: #2d2d2d; } QPushButton:pressed { background-color: #3d3d3d; color: white; }");
        }
        btn_hamburger->setStyleSheet("QPushButton { color: #e0e0e0; background-color: transparent; border-radius: 6px; font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 15px; border: none; } QPushButton:hover { background-color: #2d2d2d; }");

        QString btn_style = "QPushButton { color: #e0e0e0; background-color: transparent; border: none; font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 11px; width: 45px; height: 40px; } QPushButton:hover { background-color: #333333; color: white; }";
        btn_min->setStyleSheet(btn_style);
        btn_max->setStyleSheet(btn_style);
        btn_close->setStyleSheet(btn_style + "QPushButton:hover { background-color: #c42b1c; color: white; }");

        if (btn_light_theme) {
            btn_light_theme->setText("☀️ Açık Tema");
            btn_light_theme->setStyleSheet("QPushButton { background-color: #333333; color: #a0a0a0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; } QPushButton:hover { background-color: #3e3e3e; color: white; }");
        }
        if (btn_dark_theme) {
            btn_dark_theme->setText("🌙 Koyu Tema (Aktif)");
            btn_dark_theme->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 2px solid #8e24aa; }");
        }
    }
    else {
        this->setStyleSheet("QMainWindow { background-color: #f5f5f5; }");
        title_bar->setStyleSheet("QWidget#TitleBar { background-color: #f5f5f5; }");
        sidebar->setStyleSheet("QFrame#Sidebar { background-color: #ffffff; border-right: 1px solid #aaaaaa; }");
        main_panel->setStyleSheet("QWidget#MainPanel { background-color: #f0f0f0; border: none; }");
        title_label->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; margin-left: 5px; background: transparent;");

        if (mod_settings_header) mod_settings_header->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
        if (theme_block) theme_block->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 8px; border: 1px solid #aaaaaa; }");
        if (theme_title) theme_title->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (lang_block) lang_block->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 8px; border: 1px solid #aaaaaa; }");
        if (lang_title) lang_title->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (combo_lang) {
            combo_lang->setStyleSheet("QComboBox { background-color: #f9f9f9; color: #1a1a1a; border-radius: 6px; padding-left: 15px; border: 1px solid #aaaaaa; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #e0e0e0; border: 1px solid #888888; } QComboBox::drop-down { border: none; width: 30px; }");
            combo_lang->view()->setStyleSheet("QListView { background-color: #ffffff; color: #1a1a1a; border: 1px solid #cccccc; border-radius: 6px; outline: none; padding: 4px; } QListView::item { min-height: 32px; padding-left: 10px; border-radius: 4px; margin-bottom: 2px; } QListView::item:selected { background-color: #e0e0e0; color: black; } QListView::item:hover { background-color: #6a1b9a; color: white; }");
        }
        if (sys_block) sys_block->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 8px; border: 1px solid #aaaaaa; }");
        if (sys_title) sys_title->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");

        if (tools_header) tools_header->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
        if (blacklist_block) blacklist_block->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 12px; border: 1px solid #aaaaaa; }");
        if (bl_title) bl_title->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (bl_desc) bl_desc->setStyleSheet("color: #555555; font-family: 'Segoe UI Variable'; font-size: 13px; border: none; background: transparent;");

        if (blacklist_widget) {
            blacklist_widget->setStyleSheet(
                "QListWidget { background-color: #f5f5f5; color: #333333; border: 1px solid #cccccc; border-radius: 6px; padding: 5px; font-family: 'Consolas', monospace; font-size: 13px; outline: none; } "
                "QListWidget::item { padding: 2px 5px; min-height: 22px; border-radius: 4px; } "
                "QListWidget::item:selected { background-color: #6a1b9a; color: white; } "
            );
        }
        if (input_new_domain) {
            input_new_domain->setStyleSheet("QLineEdit { background-color: #f5f5f5; color: #333333; border: 1px solid #cccccc; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 14px; } QLineEdit:focus { border: 1px solid #6a1b9a; }");
        }
        if (search_domain) {
            search_domain->setStyleSheet("QLineEdit { background-color: #e0e0e0; color: #1a1a1a; border: 1px solid #cccccc; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 13px; } QLineEdit:focus { border: 1px solid #00E676; }");
        }

        if (stats_header) stats_header->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;");
        if (stats_block) stats_block->setStyleSheet("QFrame#TargetBlock { background-color: #ffffff; border-radius: 12px; border: 1px solid #aaaaaa; }");
        if (stats_title) stats_title->setStyleSheet("color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        if (stat_1_desc) stat_1_desc->setStyleSheet("color: #555555; font-family: 'Segoe UI Variable'; font-size: 14px; background: transparent;");
        if (stat_2_desc) stat_2_desc->setStyleSheet("color: #555555; font-family: 'Segoe UI Variable'; font-size: 14px; background: transparent;");

        if (btn_toggle_startup) {
            if (btn_toggle_startup->isChecked()) btn_toggle_startup->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
            else btn_toggle_startup->setStyleSheet("QPushButton { background-color: #f5f5f5; color: #555555; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #cccccc; }");
        }
        if (btn_toggle_tray) {
            if (btn_toggle_tray->isChecked()) btn_toggle_tray->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
            else btn_toggle_tray->setStyleSheet("QPushButton { background-color: #f5f5f5; color: #555555; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #cccccc; }");
        }

        for (auto btn : sidebar_btns) {
            btn->setStyleSheet("QPushButton { color: #1a1a1a; background-color: transparent; border-radius: 6px; text-align: left; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; } QPushButton:hover { background-color: #e0e0e0; } QPushButton:pressed { background-color: #d0d0d0; color: black; }");
        }
        btn_hamburger->setStyleSheet("QPushButton { color: #1a1a1a; background-color: transparent; border-radius: 6px; font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 15px; border: none; } QPushButton:hover { background-color: #e0e0e0; }");

        QString btn_style = "QPushButton { color: #1a1a1a; background-color: transparent; border: none; font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 11px; width: 45px; height: 40px; } QPushButton:hover { background-color: #e0e0e0; color: black; }";
        btn_min->setStyleSheet(btn_style);
        btn_max->setStyleSheet(btn_style);
        btn_close->setStyleSheet(btn_style + "QPushButton:hover { background-color: #c42b1c; color: white; }");

        if (btn_light_theme) {
            btn_light_theme->setText("☀️ Açık Tema (Aktif)");
            btn_light_theme->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 2px solid #8e24aa; }");
        }
        if (btn_dark_theme) {
            btn_dark_theme->setText("🌙 Koyu Tema");
            btn_dark_theme->setStyleSheet("QPushButton { background-color: #f5f5f5; color: #555555; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #cccccc; } QPushButton:hover { background-color: #e8e8e8; color: #1a1a1a; }");
        }
    }
}

void TheTakanosu_Elite::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    HWND hwnd = reinterpret_cast<HWND>(this->winId());

    // 🚀 BUM! KAPTANIN ZIRHI: Şeffaf camı üstten sildik, sadece alta 1px verdik.
    // Artık üst tarafta o 1 piksellik masaüstü sızıntısı (bleed) asla olmayacak!
    MARGINS margins = { 0, 0, 0, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    SetWindowLongW(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    QTimer::singleShot(10, this, &TheTakanosu_Elite::sync_sidebar_height);
}

void TheTakanosu_Elite::sync_sidebar_height() {
    sidebar->setGeometry(0, 0, sidebar->width(), body_widget->height());
}

void TheTakanosu_Elite::toggle_hamburger() {
    if (this->width() < 920) {
        m_is_overlay_open = !m_is_overlay_open;
        if (m_is_overlay_open) {
            sidebar->setFixedWidth(240);
            for (auto btn : sidebar_btns) btn->setText(btn->property("full_text").toString());
            sidebar->raise();
        }
        else {
            sidebar->setFixedWidth(60);
            for (auto btn : sidebar_btns) btn->setText(btn->property("icon_only").toString());
        }
    }
    else {
        m_is_manually_collapsed = !m_is_manually_collapsed;
        if (m_is_manually_collapsed) {
            sidebar_placeholder->setFixedWidth(60);
            sidebar->setFixedWidth(60);
            for (auto btn : sidebar_btns) btn->setText(btn->property("icon_only").toString());
        }
        else {
            sidebar_placeholder->setFixedWidth(240);
            sidebar->setFixedWidth(240);
            for (auto btn : sidebar_btns) btn->setText(btn->property("full_text").toString());
        }
    }
}

void TheTakanosu_Elite::resizeEvent(QResizeEvent* event) {
    if (!this->isMaximized()) m_safe_geometry = this->geometry();
    if (this->width() < 920) {
        sidebar_placeholder->setFixedWidth(60);
        if (!m_is_overlay_open) {
            sidebar->setFixedWidth(60);
            for (auto btn : sidebar_btns) btn->setText(btn->property("icon_only").toString());
        }
    }
    else {
        m_is_overlay_open = false;
        if (m_is_manually_collapsed) {
            sidebar_placeholder->setFixedWidth(60);
            sidebar->setFixedWidth(60);
            for (auto btn : sidebar_btns) btn->setText(btn->property("icon_only").toString());
        }
        else {
            sidebar_placeholder->setFixedWidth(240);
            sidebar->setFixedWidth(240);
            for (auto btn : sidebar_btns) btn->setText(btn->property("full_text").toString());
        }
    }
    sidebar->setGeometry(0, 0, sidebar->width(), body_widget->height());
    QMainWindow::resizeEvent(event);
}

void TheTakanosu_Elite::toggle_maximize() {
    HWND hwnd = reinterpret_cast<HWND>(this->winId());

    // 🚀 BUM! KONTROLÜ WİNDOWS'A DEVRETTİK!
    // Artık butonumuz C++'ın değil, direkt Windows'un beynine komut yolluyor.
    // Bu sayede butona basmakla başlık çubuğuna çift tıklamak BİREBİR AYNI şey oldu!
    if (IsZoomed(hwnd)) {
        SendMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0); // Eski haline dön
    }
    else {
        SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0); // Tam ekran yap
    }
}

// 🚀 BUM! İŞTE KONTROLÜ WİNDOWS'A VEREN AMA SENKRONİZASYONU BİZDE TUTAN KOD!
bool TheTakanosu_Elite::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCCALCSIZE) {
        if (msg->wParam == TRUE) { *result = 0; return true; }
        return false;
    }
    if (msg->message == WM_NCHITTEST) {
        // 🚀 BUM! LAPTOP DPI (ÖLÇEKLENDİRME) ZIRHI AKTİF!
        // Windows'un kafası karışan ham pikselleri yerine, Qt'nin %100 isabetli DPI motorunu kullanıyoruz!
        QPoint pos = this->mapFromGlobal(QCursor::pos());

        if (!this->isMaximized()) {
            int w = this->width(), h = this->height(), m = 6;
            bool l = pos.x() < m, r = pos.x() > w - m, t = pos.y() < m, b = pos.y() > h - m;
            if (l && t) { *result = HTTOPLEFT; return true; }
            if (r && t) { *result = HTTOPRIGHT; return true; }
            if (l && b) { *result = HTBOTTOMLEFT; return true; }
            if (r && b) { *result = HTBOTTOMRIGHT; return true; }
            if (l) { *result = HTLEFT; return true; }
            if (r) { *result = HTRIGHT; return true; }
            if (t) { *result = HTTOP; return true; }
            if (b) { *result = HTBOTTOM; return true; }
        }

        QWidget* child = this->childAt(pos);
        if (child && (child == btn_min || child == btn_max || child == btn_close || qobject_cast<QScrollBar*>(child))) {
            return false;
        }

        // 🚀 BUM! HTCAPTION GERİ GELDİ! KONTROLÜ WİNDOWS YÖNETECEK, BİZ SADECE SEYREDECEĞİZ!
        if (pos.y() < 40) { *result = HTCAPTION; return true; }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

// ==========================================
// 🚀 UYGULAMA KAPANIRKEN TEMİZLİK (DESTRUCTOR)
// ==========================================
TheTakanosu_Elite::~TheTakanosu_Elite() {
    QProcess::execute("cmd.exe", QStringList() << "/c" << "taskkill /F /IM goodbyedpi.exe >nul 2>&1");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "sc stop \"GoodbyeDPI_Elite\" >nul 2>&1");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "sc delete \"GoodbyeDPI_Elite\" >nul 2>&1");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "sc stop \"WinDivert\" >nul 2>&1");
    QProcess::execute("cmd.exe", QStringList() << "/c" << "sc stop \"WinDivert1.4\" >nul 2>&1");
}

// ==========================================
// 🚀 PENCERE DURUMU SENKRONİZASYONU VE TAŞMA (BLEED) KORUMASI
// ==========================================
void TheTakanosu_Elite::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        if (this->isMaximized()) {
            // Pencere Tam Ekrana geçtiyse Küçült (Restore) ikonunu koy
            btn_max->setText(QString::fromUtf8("\uE923"));

            // 🚀 KAPTANIN ZIRHI: Windows 11 tam ekranda uygulamayı 8 piksel dışarı 
            // taşırıp köşeleri keser. Bunu engellemek için içeriye 8px boşluk veriyoruz!
            main_layout->setContentsMargins(8, 8, 8, 8);
        }
        else {
            // Pencere Küçüldüyse (Windowed) Büyült (Maximize) ikonunu koy
            btn_max->setText(QString::fromUtf8("\uE922"));

            // Zırhı kaldır, tam otur
            main_layout->setContentsMargins(0, 0, 0, 0);
        }
    }
    QMainWindow::changeEvent(event);
}

// ==========================================
// 🚀 ARKA PLAN OTONOM GÜNCELLEME KONTROL MERKEZİ (IN-APP OTA UPDATE)
// ==========================================
void TheTakanosu_Elite::check_updates() {
    QString current_version = "1.1.0"; // Şu anki sürümümüz

    // 🌍 DİKKAT: GitHub'a yükleyeceğin raw version.txt dosyasının linki!
    QString version_url = "https://raw.githubusercontent.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/main/version.txt";

    QProcess* curl_version = new QProcess(this);
    curl_version->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });

    connect(curl_version, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, curl_version, current_version]() {
        QString dynamic_version = curl_version->readAllStandardOutput().trimmed();
        curl_version->deleteLater();

        // 🚀 BUM! SAÇMALIK FİLTRESİ EKLENDİ! 
        // Eğer gelen yazı boş değilse, bizim sürümden farklıysa, içinde "." (nokta) varsa ve "404" YAZMIYORSA çalıştır!
        if (!dynamic_version.isEmpty() && dynamic_version != current_version && dynamic_version.contains(".") && !dynamic_version.contains("404")) {

            // 1. AŞAMA: EVET / HAYIR SORUSU
            QMessageBox msgBox(this);
            msgBox.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
            msgBox.setWindowTitle("Güncelleme Sistemi");

            QString app_dir = QCoreApplication::applicationDirPath();
            msgBox.setIconPixmap(QPixmap(app_dir + "/assets/icon-transparant.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

            msgBox.setText("🔄 Yeni Bir Güncelleme Mevcut: v" + dynamic_version);
            msgBox.setInformativeText(QString(
                "The Takanosu Elite için yeni bir sürüm yayınlandı.\n\n"
                "Mevcut Sürüm: v%1\n"
                "Yeni Sürüm: v%2\n\n"
                "En iyi performans ve güncel koruma listeleri için güncellemeyi şimdi otomatik olarak indirip kurmak ister misiniz?"
            ).arg(current_version).arg(dynamic_version));

            // Butonları Özelleştiriyoruz
            QPushButton* btn_yes = msgBox.addButton("Evet, İndir ve Kur", QMessageBox::YesRole);
            QPushButton* btn_no = msgBox.addButton("Hayır, Daha Sonra", QMessageBox::NoRole);

            // Elite Tema Giydirmesi
            msgBox.setStyleSheet(
                "QMessageBox { background-color: #1a1a1a; border: 1px solid #444444; border-radius: 12px; }"
                "QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; padding-top: 5px; }"
                "QLabel#qt_msgbox_label { color: #00E676; font-size: 14px; font-weight: bold; margin-bottom: 5px; }"
                "QPushButton { background-color: #333333; color: white; border-radius: 8px; padding: 10px 20px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: 1px solid #555555; margin-top: 15px; min-width: 120px; }"
                "QPushButton:hover { background-color: #444444; }"
                "QPushButton:pressed { background-color: #555555; }"
            );

            btn_yes->setStyleSheet("background-color: #6a1b9a; color: white; border: none;"); // Evet butonu Elite Moru

            msgBox.exec();

            // 2. AŞAMA: CEVAP "EVET" İSE İNDİRMEYİ BAŞLAT
            if (msgBox.clickedButton() == btn_yes) {

                // İndirme Ekranı Kutusu
                QMessageBox* dlBox = new QMessageBox(this);
                dlBox->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
                dlBox->setWindowTitle("İndiriliyor");
                dlBox->setIconPixmap(QPixmap(app_dir + "/assets/icon-transparant.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                dlBox->setText("⚙️ Güncelleme Paketi İndiriliyor...");
                dlBox->setInformativeText("Lütfen bekleyin. İndirme tamamlandığında kurulum otomatik olarak başlayacaktır.");
                dlBox->setStyleSheet(msgBox.styleSheet());
                dlBox->setStandardButtons(QMessageBox::NoButton); // Tıklanacak buton yok
                dlBox->show();

                // 🌍 GITHUB SETUP İNDİRME LİNKİ
                QString download_url = "https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/releases/latest/download/TheTakanosu_Elite_Setup.exe";
                QString temp_path = QDir::toNativeSeparators(QDir::tempPath() + "/TheTakanosu_Elite_Update.exe");

                QProcess* curl_dl = new QProcess(this);
                curl_dl->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });

                connect(curl_dl, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, temp_path, dlBox](int exitCode) {
                    dlBox->accept(); // İndirme kutusunu kapat
                    dlBox->deleteLater();

                    if (exitCode == 0) {
                        // Github sayfasını aç
                        QDesktopServices::openUrl(QUrl("https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition"));

                        // İndirilen Yeni Setup'ı Çalıştır
                        QProcess::startDetached(temp_path, QStringList());

                        // Programı kapat (Kendini imha et)
                        qApp->quit();
                    }
                    else {
                        QMessageBox::critical(this, "Güncelleme Hatası", "İndirme başarısız oldu. Lütfen internet bağlantınızı kontrol edin ve daha sonra tekrar deneyin.");
                    }
                    });

                // curl komutu ile GitHub'dan yeni EXE'yi indir
                curl_dl->start("cmd.exe", QStringList() << "/c" << "curl -L -o \"" + temp_path + "\" \"" + download_url + "\"");
            }
        }
        });

    curl_version->start("curl", QStringList() << "-s" << version_url);
}
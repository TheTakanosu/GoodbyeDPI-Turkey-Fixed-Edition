#pragma warning(disable : 4996)

#include "TheTakanosu_Elite.h"
#include "kara_liste.h"
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
#include <QFileInfo>
#include <QTextStream>
#include <QSettings>
#include <QMessageBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHostInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QVersionNumber>
#include <QOperatingSystemVersion>
#include <QNetworkInformation>
#include <QClipboard>
#include <QGuiApplication>
#include <QSysInfo>
#include <QMap>
#include <QSet>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QUrlQuery>
#include <QPainter>
#include <QDirIterator>
#include <QCheckBox>
#include <QGraphicsBlurEffect>
#include <tlhelp32.h>
#include <memory>
#include "qrcodegen/qrcodegen.hpp"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")

static const char* APP_VERSION = "1.3.0";
static const char* SCHEDULED_TASK_NAME = "TheTakanosu Elite";

static bool is_self_test() { return QCoreApplication::arguments().contains("--selftest"); }

// ==========================================
// 💾 AYAR DOSYASI
// Ayarlar kayıt defteri (HKCU) yerine %LOCALAPPDATA%\TheTakanosu_Elite\settings.ini dosyasında tutulur.
// Sebep: Görev Zamanlayıcı'nın açılışta başlattığı (yükseltilmiş) kopya ile kullanıcının elle açtığı kopya
// bu sistemde HKCU'da FARKLI içerik görüyordu; tema, profil, oyun modu gibi ayarlar iki kopya arasında ayrışıyordu.
// Dosya sistemi her iki kopyada da aynı olduğu için ayarlar artık tek bir yerde.
// ==========================================
static QString takanosu_settings_path() {
    static QString path;
    if (!path.isEmpty()) return path;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    path = dir + "/settings.ini";

    // İlk açılış: eski kayıt defteri ayarlarını dosyaya bir kereliğine aktar
    if (!QFileInfo::exists(path)) {
        QSettings legacy("TheTakanosu", "EliteEngine");
        QSettings ini(path, QSettings::IniFormat);
        for (const QString& key : legacy.allKeys()) {
            ini.setValue(key, legacy.value(key));
        }
        ini.sync();
    }
    return path;
}

// ==========================================
// 🧰 YARDIMCI FONKSİYONLAR
// ==========================================
static QString now_str() { return QDateTime::currentDateTime().toString("HH:mm:ss"); }

// Windows araçlarını her zaman System32'den mutlak yolla çağır.
// (Sadece "curl" yazınca Windows önce uygulama klasörüne bakar; oraya bırakılan sahte bir exe yönetici yetkisiyle çalışırdı.)
static QString sys_tool(const QString& exe) {
    wchar_t buf[MAX_PATH];
    UINT n = GetSystemDirectoryW(buf, MAX_PATH);
    return QString::fromWCharArray(buf, n) + "\\" + exe;
}

static void hide_console(QProcess* p) {
    p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
}

// Konsol araçları (ping, ipconfig...) Türkçe Windows'ta OEM (CP857) kod sayfasıyla yazar; düzgün Türkçe karakter için çeviriyoruz.
static QString oem_text(const QByteArray& bytes) {
    if (bytes.isEmpty()) return QString();
    int n = MultiByteToWideChar(CP_OEMCP, 0, bytes.constData(), bytes.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_OEMCP, 0, bytes.constData(), bytes.size(), w.data(), n);
    return QString::fromStdWString(w);
}

// Web sayfalarını NORMAL kullanıcı olarak aç. Uygulama yönetici olarak çalıştığı için QDesktopServices tarayıcıyı da
// yönetici olarak açmaya çalışıyordu: tarayıcı zaten açıksa "zaten çalışıyor" hatası veriyor ya da yönetici yetkisiyle açılıyordu.
// explorer.exe'ye verilen adres, kullanıcının mevcut (yönetici olmayan) masaüstü oturumunda açılır.
static void open_url_unelevated(const QString& url) {
    if (!url.startsWith("https://")) return;
    wchar_t buf[MAX_PATH];
    UINT n = GetWindowsDirectoryW(buf, MAX_PATH);
    if (!QProcess::startDetached(QString::fromWCharArray(buf, n) + "\\explorer.exe", { url })) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

// sc / taskkill gibi kısa komutları siyah pencere açmadan, senkron çalıştırır.
static int run_hidden_sync(const QString& exe, const QStringList& args, int timeout_ms = 5000) {
    QProcess p;
    hide_console(&p);
    p.start(sys_tool(exe), args);
    if (!p.waitForFinished(timeout_ms)) { p.kill(); p.waitForFinished(1000); return -1; }
    return p.exitCode();
}

// 📜 Kara listeler: ayrıntı kara_liste.h içinde
static QString list_dir() { return QCoreApplication::applicationDirPath() + "/goodbyedpi"; }
static QString shipped_list_path() { return kara_liste::shipped_path(list_dir()); }
static QString custom_list_path() { return kara_liste::custom_path(list_dir()); }
static bool custom_list_has_entries() { return kara_liste::custom_has_entries(list_dir()); }
static int migrate_legacy_blacklist() { return kara_liste::migrate_legacy(list_dir()); }
using kara_liste::valid_domain;
static QStringList read_list(const QString& path, bool* ok = nullptr) { return kara_liste::read(path, ok); }
static bool write_list(const QString& path, const QStringList& lines) { return kara_liste::write(path, lines); }

// ==========================================
// 🛡️ TÜNEL (GİZLİLİK) MODU: ortak yardımcılar
// Seçilen alan adları sing-box + WireGuard tünelinden yurt dışına çıkar, gerisi doğrudan bağlanır.
// Dosyalar (özel anahtarlı yapılandırma dahil) yalnız yöneticinin yazabildiği uygulama klasöründe durur.
// ==========================================
static QString tunnel_dir() { return QCoreApplication::applicationDirPath() + "/tunnel"; }
static QString tunnel_conf_path() { return tunnel_dir() + "/baglanti.conf"; }
static QString tunnel_vless_path() { return tunnel_dir() + "/baglanti.vless"; } // kendi sunucusu (VLESS + Reality)
static const char* TUNNEL_CHECK_DOMAIN = "ifconfig.co"; // çıkış ülkesini ölçmek için hep tünelden geçer
static const char* TUNNEL_NRPT_COMMENT = "TheTakanosu Elite Tunel";

static QStringList default_tunnel_domains() {
    return {
        "instagram.com", "cdninstagram.com", "facebook.com", "fbcdn.net", "fbsbx.com",
        "tiktok.com", "tiktokcdn.com", "tiktokcdn-eu.com", "tiktokcdn-us.com", "tiktokv.com", "ttwstatic.com", "ibytedtos.com", "ibyteimg.com", "byteoversea.com",
        // TikTok'un güvenlik/ölçüm sunucuları: listede olmazsa gerçek IP'yi bunlar görür (2026-10-12'de bulundu)
        "tiktokw.eu", "tiktokv.eu", "tiktokw.us", "tiktokv.us", "byteoversea.net",
        "youtube.com", "youtu.be", "youtube-nocookie.com", "googlevideo.com", "ytimg.com", "ggpht.com", "doubleclick.net",
        "x.com", "twitter.com", "twimg.com", "t.co",
        // 7578 sayılı kanunun kapsadığı diğer sosyal ağlar ve oyun platformları (2026-10-13)
        "snapchat.com", "sc-cdn.net", "sc-static.net", "snapkit.com",
        // Reddit bilerek yok: veri merkezi IP'lerini (Oracle vb.) "blocked by network security" ile engelliyor, TR'de de engelli değil
        "twitch.tv", "ttvnw.net", "jtvnw.net", "twitchcdn.net",
        "kick.com",
        "threads.net", "threads.com",
        "pinterest.com", "pinimg.com",
        "bsky.app", "bsky.social",
        // Roblox: yalnız site ve hesap trafiği; oyun sunucularına IP ile bağlanıldığı için oyun ping'i değişmez
        "roblox.com", "rbxcdn.com",
    };
}

static QStringList tunnel_domains() {
    QSettings s(takanosu_settings_path(), QSettings::IniFormat);
    const QStringList saved = s.contains("tunnel_domains") ? s.value("tunnel_domains").toStringList() : default_tunnel_domains();
    QStringList out;
    for (QString d : saved) {
        d = d.trimmed().toLower();
        if (valid_domain(d) && !out.contains(d)) out << d; // ayar dosyası kullanıcı yazabilir: yalnız geçerli alan adları
    }
    return out;
}

static QStringList tunnel_domains_with_check() {
    QStringList d = tunnel_domains();
    if (!d.contains(TUNNEL_CHECK_DOMAIN)) d << TUNNEL_CHECK_DOMAIN;
    return d;
}

static bool tunnel_enabled() {
    return QSettings(takanosu_settings_path(), QSettings::IniFormat).value("tunnel_enabled", false).toBool();
}

static bool domain_in_tunnel(const QString& entry, const QStringList& domains) {
    const QString e = entry.trimmed().toLower();
    for (const QString& d : domains) {
        if (e == d || e.endsWith("." + d)) return true;
    }
    return false;
}

// Tünel açıkken tünele giden siteler GoodbyeDPI/Zapret listesinden çıkarılır:
// motorun sahte el sıkışma paketi tünelin içine de düşüp bağlantıyı bozuyor (2026-10-12 testinde kanıtlandı).
static QString tunnel_filtered_list(const QString& src, const QString& name) {
    if (!tunnel_enabled()) return src;
    const QStringList domains = tunnel_domains_with_check();
    QStringList kept;
    for (const QString& line : read_list(src)) {
        if (!domain_in_tunnel(line, domains)) kept << line;
    }
    QDir().mkpath(tunnel_dir());
    const QString out = tunnel_dir() + "/" + name;
    return write_list(out, kept) ? out : src;
}

// ==========================================
// 🎯 DPI PROFİLLERİ (GoodbyeDPI + Zapret)
// ==========================================
namespace {

const QStringList DNS_YANDEX = { "--dns-addr", "77.88.8.8", "--dns-port", "1253", "--dnsv6-addr", "2a02:6b8::feed:0ff", "--dnsv6-port", "1253" };
const QStringList DNS_CLOUDFLARE = { "--dns-addr", "1.1.1.1", "--dns-port", "53", "--dnsv6-addr", "2606:4700:4700::1111", "--dnsv6-port", "53" };

// Zapret (winws) ortak katmanı: QUIC, Discord sesli sohbet (UDP) ve düz HTTP. Sadece HTTPS (tcp/443) stratejisi profile göre değişir.
// {LIST} = kara liste dosyası, {FAKE} = zapret\fake\ klasörü (çalışma anında gerçek yollarla değiştirilir)
QStringList zapret_args(const QStringList& tls_strategy) {
    return QStringList{
        "--wf-tcp=80,443", "--wf-udp=443,50000-50100",
        "--filter-udp=443", "--hostlist={LIST}", "--dpi-desync=fake", "--dpi-desync-repeats=6",
        "--dpi-desync-fake-quic={FAKE}quic_initial_www_google_com.bin",
        "--new",
        "--filter-udp=50000-50100", "--filter-l7=discord,stun", "--dpi-desync=fake", "--dpi-desync-repeats=6",
        "--new",
        "--filter-tcp=80", "--hostlist={LIST}", "--dpi-desync=fake,multisplit", "--dpi-desync-autottl=2", "--dpi-desync-fooling=md5sig",
        "--new",
        "--filter-tcp=443", "--hostlist={LIST}"
    } + tls_strategy;
}

const QList<DpiProfile>& dpi_profiles() {
    static const QList<DpiProfile> list = {
        // ── GoodbyeDPI (eski sürümlerden gelen, sahada kanıtlanmış profiller) ──
        // ── Sürücüsüz ──
        { "dns_doh", "Şifreli DNS Modu (sürücüsüz)", false, QStringList{}, "Windows 11'in şifreli DNS'i (Cloudflare DoH). Sürücü yüklemez, anti-cheat'e takılmaz, uygulama kapalıyken de çalışır.", true },
        { "gd_turknet", "TurkNet / Cloudflare Modülü", false, QStringList{ "-5", "--set-ttl", "5" } + DNS_CLOUDFLARE, "-5 + TTL 5, Cloudflare DNS. TurkNet için ideal." },
        { "gd_ttnet", "Türk Telekom / Ortak Zırh Modülü", false, QStringList{ "-5", "--set-ttl", "5" } + DNS_YANDEX, "-5 + TTL 5, Yandex DNS (1253 portu). DNS'i zehirlenen ağlar için." },
        { "gd_joker", "Evrensel Otonom Joker Modülü (-5)", false, QStringList{ "-5" }, "Sadece -5. DNS'e dokunmaz; Türk Telekom ve Turkcell'de çoğunlukla yeterli." },
        { "gd_ttl3", "Orijinal Alternatif Sürüm v1 (ttl 3)", false, QStringList{ "--set-ttl", "3" } + DNS_YANDEX, "TTL 3 sahte paket + Yandex DNS. Eski alternatif sürüm." },
        { "gd_heavy", "Elite Son Çare Ağır Silahı (-f 1)", false, QStringList{ "-e", "2", "-f", "1", "--reverse-frag", "--set-ttl", "5" } + DNS_YANDEX, "Ters parçalama + TTL 5 + Yandex DNS. Ağır sansür için son çare." },
        // ── GoodbyeDPI (yeni: Vodafone / Kablonet gibi inatçı altyapılar için) ──
        { "gd_full9", "Tam Zırh Modülü (-9, QUIC Engelli)", false, QStringList{ "-9" } + DNS_YANDEX, "-9: yanlış sıra/checksum sahte paketler, QUIC engelli + Yandex DNS." },
        { "gd_fakegen", "Sahte Paket Yağmuru (-9 fake-gen)", false, QStringList{ "-9", "--fake-gen", "5", "--fake-resend", "2" } + DNS_YANDEX, "-9 + 5 rastgele sahte paket (2 tekrar). İnatçı DPI için." },
        { "gd_sni", "SNI Cerrahı (native-frag)", false, QStringList{ "-5", "--native-frag", "--frag-by-sni" } + DNS_YANDEX, "Paketi tam SNI noktasından böler (native-frag). Hafif ve hızlı." },
        // ── Zapret (winws) ──
        { "zp_seqovl", "Zapret: Seqovl Bölücü", true, zapret_args({
            "--dpi-desync=multisplit", "--dpi-desync-split-seqovl=681", "--dpi-desync-split-pos=1",
            "--dpi-desync-split-seqovl-pattern={FAKE}tls_clienthello_www_google_com.bin" }), "TLS el sıkışmasını sıra çakışmalı (seqovl) böler. Vodafone/Kablonet adayı." },
        { "zp_fake_multisplit", "Zapret: Sahte + Çoklu Bölme", true, zapret_args({
            "--dpi-desync=fake,multisplit", "--dpi-desync-split-pos=1,midsld", "--dpi-desync-repeats=6",
            "--dpi-desync-fooling=badseq", "--dpi-desync-fake-tls={FAKE}tls_clienthello_www_google_com.bin" }), "Sahte ClientHello + çoklu bölme (badseq)." },
        { "zp_fakedsplit", "Zapret: Auto-TTL Sahte Bölme", true, zapret_args({
            "--dpi-desync=fake,fakedsplit", "--dpi-desync-autottl=2", "--dpi-desync-repeats=6",
            "--dpi-desync-fooling=md5sig", "--dpi-desync-fake-tls={FAKE}tls_clienthello_www_google_com.bin" }), "Otomatik TTL'li sahte bölme (md5sig)." },
        { "zp_disorder", "Zapret: Ters Sıra (multidisorder)", true, zapret_args({
            "--dpi-desync=multidisorder", "--dpi-desync-split-pos=1,midsld" }), "Parçaları ters sırada gönderir (multidisorder)." },
    };
    return list;
}

const DpiProfile* find_profile(const QString& id) {
    for (const DpiProfile& p : dpi_profiles()) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

// ISS indeksleri combo_iss ile aynı: 0 Bilinmeyen/Otomatik, 1 Superonline, 2 Türk Telekom, 3 TurkNet, 4 Kablonet, 5 Vodafone
QStringList profile_order_for_isp(int isp) {
    switch (isp) {
    case 3: return { "gd_turknet", "gd_ttnet", "gd_joker", "zp_seqovl", "zp_fake_multisplit", "gd_full9", "gd_ttl3", "gd_heavy", "zp_fakedsplit", "zp_disorder", "gd_fakegen", "gd_sni", "dns_doh" };
    case 1:
    case 2: return { "gd_joker", "gd_ttnet", "gd_turknet", "gd_ttl3", "zp_seqovl", "zp_fake_multisplit", "gd_full9", "gd_heavy", "zp_fakedsplit", "zp_disorder", "gd_fakegen", "gd_sni", "dns_doh" };
    case 4:
    case 5: return { "zp_fakedsplit", "gd_ttnet", "gd_sni", "zp_fake_multisplit", "gd_ttl3", "gd_heavy", "zp_seqovl", "gd_fakegen", "gd_full9", "zp_disorder", "gd_turknet", "gd_joker", "dns_doh" };
    default: return { "gd_joker", "gd_ttnet", "gd_turknet", "zp_seqovl", "zp_fake_multisplit", "gd_full9", "gd_ttl3", "gd_heavy", "zp_fakedsplit", "zp_disorder", "gd_fakegen", "gd_sni", "dns_doh" };
    }
}

bool zapret_installed() {
    return QFileInfo::exists(QCoreApplication::applicationDirPath() + "/zapret/winws.exe");
}

} // namespace

PowerToysDashboard::PowerToysDashboard(QWidget* parent) : QWidget(parent) {
    this->setStyleSheet("background-color: transparent;");

    is_dark_mode = true;
    is_busy = false;
    restoring = false;
    single_try = false;
    legacy_cleaned = false;
    ui_state = UiState::Idle;
    scan_generation = 0;
    current_auto_test_index = 0;
    restore_retry = 0;
    st_index = 0;
    st_in_app = false;
    game_running = false;
    game_dns_temp = false;
    health_fail = 0;

    // 🛡️ Motor süreçleri bir "Job Object" içine konur: uygulama çökse bile Windows motorları otomatik kapatır.
    engine_job = nullptr;
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
        engine_job = job;
    }

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
    set_start_button(true, "OTONOM SİSTEMİ BAŞLAT");
    layout_quick->addWidget(btn_start);

    connect(btn_start, &QPushButton::clicked, this, &PowerToysDashboard::start_autonomous_system);

    progress_bar = new QProgressBar();
    progress_bar->setFixedHeight(6);
    progress_bar->setTextVisible(false);
    set_progress(0, "#8a2be2");
    layout_quick->addWidget(progress_bar);

    lbl_status = new QLabel();
    lbl_status->setWordWrap(true);
    lbl_status->setAlignment(Qt::AlignCenter);
    layout_quick->addWidget(lbl_status);

    lbl_active_mod = new QLabel();
    lbl_active_mod->setWordWrap(true);
    lbl_active_mod->setAlignment(Qt::AlignCenter);
    layout_quick->addWidget(lbl_active_mod);

    log_console = new QPlainTextEdit();
    log_console->setReadOnly(true);
    log_console->setMaximumBlockCount(3000);
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
    log_console->appendPlainText(QString("[SİSTEM] The Takanosu Elite v%1 Başlatıldı...").arg(APP_VERSION));
    log_console->appendPlainText("[SİSTEM] Kullanıcı emirleri bekleniyor...");
    if (!zapret_installed()) {
        log_console->appendPlainText("[UYARI] Zapret motoru bulunamadı, sadece GoodbyeDPI profilleri kullanılacak.");
    }
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

    // Motor başlamadan önce: eski sürümden kalan kullanıcı sitelerini özel listeye taşı
    if (!is_self_test()) {
        const int moved = migrate_legacy_blacklist();
        if (moved > 0) log("KARA LİSTE", QString("Önceki sürümde eklediğiniz %1 site özel listenize taşındı (güncellemelerde artık silinmez).").arg(moved));
    }

    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    combo_iss->setCurrentIndex(qBound(0, settings.value("last_iss_index", 0).toInt(), combo_iss->count() - 1));

    QString last_mode = settings.value("last_active_mod").toString();
    bool has_saved_profile = settings.contains("last_profile_id") || settings.contains("last_working_param");
    if (has_saved_profile && !last_mode.isEmpty() && !is_self_test()) {
        set_ui_state(UiState::Idle, "Sistem Beklemede. Kayıtlı profil uyandırılıyor...", "Son Çalışan Mod: " + last_mode);
        QTimer::singleShot(500, this, &PowerToysDashboard::restore_last_session);
    }
    else {
        set_ui_state(UiState::Idle, "Sistem Beklemede. Hedef ISS'yi seçin ve başlatın.", "Aktif Mod: BEKLENİYOR...");
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
        "🎮 DPI'ı Durdur / Oyun Modu (Kill DPI)",
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

    QPushButton* btn_v6 = new QPushButton("🌐 IPv6 DNS Kontrolü (Discord gecikmesi)");
    btn_v6->setFixedHeight(44);
    btn_v6->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn_v6->setCursor(Qt::PointingHandCursor);
    connect(btn_v6, &QPushButton::clicked, this, &PowerToysDashboard::util_ipv6_dns_check);
    layout_utils->addWidget(btn_v6);
    utility_btns.append(btn_v6);

    QPushButton* btn_diag = new QPushButton("🩺 Tanı Raporu Oluştur (Sorun Bildir)");
    btn_diag->setFixedHeight(44);
    btn_diag->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn_diag->setCursor(Qt::PointingHandCursor);
    connect(btn_diag, &QPushButton::clicked, this, &PowerToysDashboard::util_diagnostic_report);
    layout_utils->addWidget(btn_diag);
    utility_btns.append(btn_diag);

    QPushButton* btn_github = new QPushButton("🐙 TheTakanosu GitHub Profili");
    btn_github->setFixedHeight(44);
    btn_github->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn_github->setCursor(Qt::PointingHandCursor);
    connect(btn_github, &QPushButton::clicked, this, []() { open_url_unelevated("https://github.com/TheTakanosu"); });

    layout_utils->addWidget(btn_github);
    utility_btns.append(btn_github);

    col_right_layout->addWidget(block_utils);
    col_right_layout->addStretch();

    flex_layout->addWidget(col_left);
    flex_layout->addWidget(col_right);
    outer_layout->addWidget(inner_container);

    // 🎮 Otomatik Oyun Modu: anti-cheat başlatıcısı açılır açılmaz yakalamak için saniyede bir kontrol et
    // (süreç listesini okumak ~1 ms sürer, performansa etkisi yoktur)
    QTimer* game_timer = new QTimer(this);
    connect(game_timer, &QTimer::timeout, this, &PowerToysDashboard::game_check);
    game_timer->start(1000);

    // 🛰️ Nöbetçi: 5 dakikada bir ve ağ değiştiğinde aktif profilin hâlâ çalıştığını doğrula
    QTimer* health_timer = new QTimer(this);
    connect(health_timer, &QTimer::timeout, this, &PowerToysDashboard::health_check);
    health_timer->start(5 * 60 * 1000);
    if (QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this, [this](QNetworkInformation::Reachability r) {
            if (r == QNetworkInformation::Reachability::Online) QTimer::singleShot(8000, this, &PowerToysDashboard::health_check);
        });
    }
}

PowerToysDashboard::~PowerToysDashboard() {
    stop_engine(false);
    if (engine_job) CloseHandle(static_cast<HANDLE>(engine_job));
}

// ==========================================
// 🖥️ ARAYÜZ DURUM YARDIMCILARI
// ==========================================
void PowerToysDashboard::log(const QString& tag, const QString& text) {
    const QString line = QString("[%1] [%2] %3").arg(now_str(), tag, text);
    log_console->appendPlainText(line);

    // Kalıcı log: uygulama tepsideyken veya kapandıktan sonra da sorunları inceleyebilmek için (512 KB'de döner)
    static const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    static const bool dir_ok = QDir().mkpath(dir);
    if (!dir_ok) return;
    const QString path = dir + "/takanosu_log.txt";
    if (QFileInfo(path).size() > 512 * 1024) {
        QFile::remove(path + ".old");
        QFile::rename(path, path + ".old");
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write((QDate::currentDate().toString("yyyy-MM-dd ") + line + "\n").toUtf8());
    }
}

void PowerToysDashboard::set_ui_state(UiState state, const QString& status, const QString& mod) {
    ui_state = state;
    const QString success = is_dark_mode ? "#00E676" : "#008B00";
    const QString fail = is_dark_mode ? "#ff5252" : "#d32f2f";
    const QString pending = is_dark_mode ? "#ff9800" : "#d84315";
    const QString dim = is_dark_mode ? "#777777" : "#555555";

    QString status_color = dim;
    QString mod_color = fail;
    bool status_bold = true;
    switch (state) {
    case UiState::Idle: status_bold = false; mod_color = mod.startsWith("Son Çalışan") ? success : fail; break;
    case UiState::Busy: status_color = pending; mod_color = pending; break;
    case UiState::Success: status_color = success; mod_color = success; break;
    case UiState::Fail: status_color = fail; mod_color = fail; break;
    case UiState::Stopped: status_color = pending; mod_color = fail; break;
    }

    lbl_status->setText(status);
    lbl_status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 11px; margin-top: 8px; border: none; %2 background: transparent;").arg(status_color, status_bold ? "font-weight: bold;" : ""));
    lbl_active_mod->setText(mod);
    lbl_active_mod->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; margin-top: 15px; border: none; background: transparent;").arg(mod_color));
}

void PowerToysDashboard::set_progress(int value, const QString& color) {
    progress_bar->setValue(value);
    progress_bar->setStyleSheet(QString("QProgressBar { background-color: #202020; border-radius: 3px; border: none; margin-top: 15px; } QProgressBar::chunk { background-color: %1; border-radius: 3px; }").arg(color));
}

void PowerToysDashboard::set_start_button(bool enabled, const QString& text) {
    btn_start->setEnabled(enabled);
    btn_start->setText(text);
    if (enabled) {
        btn_start->setStyleSheet(
            "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; } "
            "QPushButton:hover { background-color: #8e24aa; } "
            "QPushButton:pressed { background-color: #4a148c; }"
        );
    }
    else {
        btn_start->setStyleSheet("QPushButton { background-color: #444444; color: #888888; border-radius: 8px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; margin-top: 20px; }");
    }
}

void PowerToysDashboard::apply_combo_style(QComboBox* combo) {
    if (is_dark_mode) {
        combo->setStyleSheet("QComboBox { background-color: #333333; color: white; border-radius: 6px; padding-left: 15px; border: 1px solid #444444; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #3a3a3a; border: 1px solid #555555; } QComboBox::drop-down { border: none; width: 30px; }");
        combo->view()->setStyleSheet(
            "QListView { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; outline: none; padding: 4px; } "
            "QListView::item { min-height: 32px; padding-left: 11px; border-radius: 4px; margin-bottom: 2px; } "
            "QListView::item:selected { background-color: #444444; color: white; } "
            "QListView::item:hover { background-color: #6a1b9a; color: white; }"
        );
    }
    else {
        combo->setStyleSheet("QComboBox { background-color: #f9f9f9; color: #1a1a1a; border-radius: 6px; padding-left: 15px; border: 1px solid #aaaaaa; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #e0e0e0; border: 1px solid #888888; } QComboBox::drop-down { border: none; width: 30px; }");
        combo->view()->setStyleSheet(
            "QListView { background-color: #ffffff; color: #1a1a1a; border: 1px solid #cccccc; border-radius: 6px; outline: none; padding: 4px; } "
            "QListView::item { min-height: 32px; padding-left: 11px; border-radius: 4px; margin-bottom: 2px; } "
            "QListView::item:selected { background-color: #e0e0e0; color: black; } "
            "QListView::item:hover { background-color: #6a1b9a; color: white; }"
        );
    }
}

// ==========================================
// ⚙️ ARKA PLAN İŞLEM YARDIMCILARI (Arayüzü dondurmadan çalışır)
// ==========================================
void PowerToysDashboard::run_async(const QString& program, const QStringList& args, int timeout_ms,
                                   std::function<void(int exit_code, const QString& output)> done) {
    QProcess* p = new QProcess(this);
    hide_console(p);
    p->setProcessChannelMode(QProcess::MergedChannels);

    auto finished = std::make_shared<bool>(false);
    auto finish = [p, done, finished](int code) {
        if (*finished) return;
        *finished = true;
        QString out = oem_text(p->readAll());
        p->deleteLater();
        if (done) done(code, out);
    };

    connect(p, &QProcess::finished, this, [finish](int code, QProcess::ExitStatus status) {
        finish(status == QProcess::NormalExit ? code : -1);
    });
    connect(p, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finish(-1);
    });
    QTimer::singleShot(timeout_ms, p, [p]() {
        if (p->state() != QProcess::NotRunning) p->kill();
    });

    p->start(program, args);
}

// İnternet var mı? (ICMP ping yerine HTTPS: bazı modemler/ağlar ping'i engelliyor ve yanlış "internet yok" veriyordu)
void PowerToysDashboard::check_internet(std::function<void(bool online)> done) {
    run_async(sys_tool("curl.exe"),
        { "-s", "-o", "NUL", "-w", "%{http_code}", "--ssl-no-revoke", "--connect-timeout", "4", "--max-time", "6",
          "https://www.google.com/generate_204" },
        9000, [done](int, const QString& out) {
            int code = out.trimmed().toInt();
            done(code >= 100 && code < 600);
        });
}

// 🎯 GERÇEK SANSÜR TESTİ: Discord'a HTTPS (TLS/SNI) bağlantısı kurulabiliyor mu?
// Eski sürüm "ping discord.com" yapıyordu; ping (ICMP) DPI'a takılmadığı için sansür aşılmasa bile "BAŞARILI" diyordu.
// Vodafone/Kablonet'te "bazen çalışıyor" sorununun asıl sebebi buydu.
void PowerToysDashboard::test_dpi_bypass(std::function<void(bool passed, const QString& detail)> done) {
    // ISS'ler engeli aralıklı uygulayabiliyor (TT'de şifreli DNS tek başına 20 denemede 5 kez geçti).
    // Bu yüzden tek başarı yetmez: 3 tur boyunca iki adresin de HER denemede açılması gerekir.
    QStringList targets;
    for (int round = 0; round < 3; ++round) targets << "https://discord.com/api/v9/gateway" << "https://gateway.discord.gg";
    auto index = std::make_shared<int>(0);
    auto step = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weak_step = step;

    *step = [this, targets, index, weak_step, done]() {
        if (*index >= targets.size()) { done(true, "Discord HTTPS erişimi doğrulandı"); return; }
        const QString url = targets[*index];
        auto self = weak_step.lock();
        run_async(sys_tool("curl.exe"),
            { "-s", "-o", "NUL", "-w", "%{http_code}", "--ssl-no-revoke", "--connect-timeout", "5", "--max-time", "8", url },
            11000, [index, self, url, done](int, const QString& out) {
                int code = out.trimmed().toInt();
                if (code < 100 || code >= 600) {
                    done(false, QString("%1 → bağlantı kesildi/zaman aşımı").arg(QUrl(url).host()));
                    return;
                }
                ++(*index);
                if (self) (*self)();
            });
    };
    (*step)();
}

// Discord'a N kez ayrı ayrı bağlanmayı dener ve kaçının başarılı olduğunu döndürür (güvenilirlik ölçümü)
void PowerToysDashboard::probe_discord(int attempts, std::function<void(int ok, int total)> done) {
    auto ok = std::make_shared<int>(0);
    auto index = std::make_shared<int>(0);
    auto step = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> weak_step = step;
    *step = [this, attempts, ok, index, weak_step, done]() {
        if (*index >= attempts) { done(*ok, attempts); return; }
        auto self = weak_step.lock();
        run_async(sys_tool("curl.exe"),
            { "-s", "-o", "NUL", "-w", "%{http_code}", "--ssl-no-revoke", "--connect-timeout", "5", "--max-time", "8", "https://discord.com/api/v9/gateway" },
            11000, [ok, index, self](int, const QString& out) {
                const int code = out.trimmed().toInt();
                if (code >= 100 && code < 600) ++(*ok);
                ++(*index);
                if (self) (*self)();
            });
    };
    (*step)();
}

// ISS tespiti (HTTPS üzerinden, AS numarasıyla: isim değişse bile doğru eşleşir)
void PowerToysDashboard::detect_isp(std::function<void(int isp_index, const QString& isp_text)> done) {
    run_async(sys_tool("curl.exe"), { "-s", "--ssl-no-revoke", "--max-time", "5", "https://ipinfo.io/org" }, 8000,
        [done](int, const QString& out) {
            QString text = out.trimmed();
            if (text.contains('<') || text.contains('{') || text.size() > 120) text.clear();
            const QString t = text.toUpper();
            int isp = 0;
            if (t.contains("AS12735") || t.contains("TURKNET")) isp = 3;
            else if (t.contains("AS9121") || t.contains("TELEKOM")) isp = 2;
            else if (t.contains("AS34984") || t.contains("SUPERONLINE") || t.contains("TURKCELL")) isp = 1;
            else if (t.contains("AS47524") || t.contains("TURKSAT") || t.contains("KABLONET")) isp = 4;
            else if (t.contains("AS15897") || t.contains("VODAFONE")) isp = 5;
            done(isp, text);
        });
}

// ==========================================
// 🔐 ŞİFRELİ DNS MODU (sürücüsüz, anti-cheat dostu)
// Türk Telekom / Turkcell gibi ISS'ler Discord'u çoğunlukla DNS zehirlemesiyle engelliyor.
// Windows 11'in kendi DNS-over-HTTPS özelliğiyle (Cloudflare) DNS sorguları şifrelenir: WinDivert sürücüsü
// yüklenmez, uygulama kapalıyken ve bilgisayar açılışında da çalışmaya devam eder.
// Önceki DNS ayarları sadece yöneticinin yazabildiği uygulama klasörüne yedeklenir ve birebir geri yüklenir.
// ==========================================
static QString dns_backup_path() {
    return QCoreApplication::applicationDirPath() + "/dns_backup.json";
}

bool takanosu_dns_mode_active() {
    return QFileInfo::exists(dns_backup_path());
}

static bool dns_mode_supported() {
    return QOperatingSystemVersion::current().microVersion() >= 22000; // Windows 11+
}

static QString powershell_exe() {
    return sys_tool("WindowsPowerShell\\v1.0\\powershell.exe");
}

static QStringList powershell_args(const QString& script) {
    // -EncodedCommand: betik tırnak/kaçış sorunları olmadan, birebir iletilir
    const QByteArray utf16(reinterpret_cast<const char*>(script.utf16()), script.size() * 2);
    return { "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", QString::fromLatin1(utf16.toBase64()) };
}

static const char* DNS_APPLY_SCRIPT = R"PS(
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$backupPath = '__BACKUP_PATH__'
$v4 = @('1.1.1.1', '1.0.0.1')
$v6 = @('2606:4700:4700::1111', '2606:4700:4700::1001')
$tpl = 'https://cloudflare-dns.com/dns-query'
$backup = [ordered]@{ doh = @(); adapters = @(); error = '' }
try {
    foreach ($ip in $v4 + $v6) {
        $e = Get-DnsClientDohServerAddress -ServerAddress $ip -ErrorAction SilentlyContinue
        if ($e) {
            $backup.doh += [ordered]@{ ip = $ip; exists = $true; tpl = $e.DohTemplate; auto = [bool]$e.AutoUpgrade; udp = [bool]$e.AllowFallbackToUdp }
            Set-DnsClientDohServerAddress -ServerAddress $ip -DohTemplate $tpl -AutoUpgrade $true -AllowFallbackToUdp $false | Out-Null
        } else {
            $backup.doh += [ordered]@{ ip = $ip; exists = $false; tpl = ''; auto = $false; udp = $false }
            Add-DnsClientDohServerAddress -ServerAddress $ip -DohTemplate $tpl -AutoUpgrade $true -AllowFallbackToUdp $false | Out-Null
        }
    }
    foreach ($c in Get-NetIPConfiguration | Where-Object { $_.IPv4DefaultGateway -or $_.IPv6DefaultGateway }) {
        $idx = $c.InterfaceIndex
        $guid = (Get-NetAdapter -InterfaceIndex $idx).InterfaceGuid
        $ns4 = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces\$guid" -Name NameServer -ErrorAction SilentlyContinue).NameServer
        $ns6 = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip6\Parameters\Interfaces\$guid" -Name NameServer -ErrorAction SilentlyContinue).NameServer
        $base = "HKLM:\SYSTEM\CurrentControlSet\Services\Dnscache\InterfaceSpecificParameters\$guid\DohInterfaceSettings"
        $keys = @()
        foreach ($k in @($v4 | ForEach-Object { "Doh\$_" }) + @($v6 | ForEach-Object { "Doh6\$_" })) {
            $prev = Get-ItemProperty -Path "$base\$k" -Name DohFlags -ErrorAction SilentlyContinue
            $keys += [ordered]@{ key = $k; existed = [bool](Test-Path "$base\$k"); flags = $(if ($prev) { [int64]$prev.DohFlags } else { -1 }) }
        }
        $backup.adapters += [ordered]@{ guid = "$guid"; ns4 = "$ns4"; ns6 = "$ns6"; keys = $keys }

        netsh interface ipv4 set dnsservers name=$idx static $($v4[0]) primary validate=no | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "IPv4 DNS ayarlanamadi (arayuz $idx)" }
        netsh interface ipv4 add dnsservers name=$idx $($v4[1]) index=2 validate=no | Out-Null
        netsh interface ipv6 set dnsservers name=$idx static $($v6[0]) primary validate=no | Out-Null
        netsh interface ipv6 add dnsservers name=$idx $($v6[1]) index=2 validate=no | Out-Null
        foreach ($k in $keys) {
            New-Item -Path "$base\$($k.key)" -Force | Out-Null
            New-ItemProperty -Path "$base\$($k.key)" -Name DohFlags -PropertyType QWord -Value 1 -Force | Out-Null
        }
    }
    Clear-DnsClientCache
} catch {
    $backup.error = $_.Exception.Message
} finally {
    # Hata olsa bile yedek yazılır: uygulama yarım kalan değişiklikleri her zaman geri alabilir
    [System.IO.File]::WriteAllText($backupPath, ($backup | ConvertTo-Json -Depth 5 -Compress))
}
)PS";

// Yedekteki değerler PowerShell'e gömülmeden önce sıkı biçimde doğrulanır (komut enjeksiyonuna karşı)
static bool safe_ip(const QString& s) {
    static const QRegularExpression re("^[0-9A-Fa-f:.]{2,45}$");
    return re.match(s).hasMatch();
}

static QStringList safe_ip_list(const QString& s) {
    QStringList out;
    for (const QString& part : s.split(QRegularExpression("[ ,;]+"), Qt::SkipEmptyParts)) {
        if (safe_ip(part)) out << part;
    }
    return out;
}

bool takanosu_restore_dns() {
    QFile f(dns_backup_path());
    if (!f.open(QIODevice::ReadOnly)) return true; // geri alınacak bir şey yok
    const QJsonObject backup = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    static const QRegularExpression guid_re("^\\{[0-9A-Fa-f-]{36}\\}$");
    static const QRegularExpression tpl_re("^https://[A-Za-z0-9./-]+$");

    QString script = "$ErrorActionPreference = 'Continue'\n";
    for (const QJsonValue& v : backup.value("doh").toArray()) {
        const QJsonObject d = v.toObject();
        const QString ip = d.value("ip").toString();
        if (!safe_ip(ip)) continue;
        const QString tpl = d.value("tpl").toString();
        if (d.value("exists").toBool() && tpl_re.match(tpl).hasMatch()) {
            script += QString("Set-DnsClientDohServerAddress -ServerAddress '%1' -DohTemplate '%2' -AutoUpgrade $%3 -AllowFallbackToUdp $%4 | Out-Null\n")
                .arg(ip, tpl, d.value("auto").toBool() ? "true" : "false", d.value("udp").toBool() ? "true" : "false");
        }
        else {
            script += QString("Remove-DnsClientDohServerAddress -ServerAddress '%1' -ErrorAction SilentlyContinue | Out-Null\n").arg(ip);
        }
    }
    for (const QJsonValue& v : backup.value("adapters").toArray()) {
        const QJsonObject a = v.toObject();
        const QString guid = a.value("guid").toString();
        if (!guid_re.match(guid).hasMatch()) continue;
        const QStringList ns4 = safe_ip_list(a.value("ns4").toString());
        const QStringList ns6 = safe_ip_list(a.value("ns6").toString());

        script += QString("$idx = (Get-NetAdapter | Where-Object { $_.InterfaceGuid -eq '%1' }).ifIndex\n").arg(guid);
        script += "if ($idx) {\n";
        static const QRegularExpression key_re("^Doh6?\\\\[0-9A-Fa-f:.]{2,45}$");
        const QString base = QString("HKLM:\\SYSTEM\\CurrentControlSet\\Services\\Dnscache\\InterfaceSpecificParameters\\%1\\DohInterfaceSettings").arg(guid);
        for (const QJsonValue& kv : a.value("keys").toArray()) {
            const QJsonObject k = kv.toObject();
            const QString key = k.value("key").toString();
            if (!key_re.match(key).hasMatch()) continue;
            const qint64 flags = static_cast<qint64>(k.value("flags").toDouble(-1));
            if (k.value("existed").toBool() && flags >= 0)
                script += QString("  Set-ItemProperty -Path '%1\\%2' -Name DohFlags -Value %3 -Type QWord -ErrorAction SilentlyContinue\n").arg(base, key).arg(flags);
            else if (!k.value("existed").toBool())
                script += QString("  Remove-Item -Path '%1\\%2' -Recurse -Force -ErrorAction SilentlyContinue\n").arg(base, key);
        }
        // Her adres ailesi ayrı ayrı: önceden otomatikse (DHCP) otomatiğe, elle girilmişse aynı adreslere döner
        if (ns4.isEmpty()) script += "  netsh interface ipv4 set dnsservers name=$idx source=dhcp | Out-Null\n";
        else {
            script += QString("  netsh interface ipv4 set dnsservers name=$idx static %1 primary validate=no | Out-Null\n").arg(ns4[0]);
            for (int i = 1; i < ns4.size(); ++i) script += QString("  netsh interface ipv4 add dnsservers name=$idx %1 index=%2 validate=no | Out-Null\n").arg(ns4[i]).arg(i + 1);
        }
        if (ns6.isEmpty()) script += "  netsh interface ipv6 set dnsservers name=$idx source=dhcp | Out-Null\n";
        else {
            script += QString("  netsh interface ipv6 set dnsservers name=$idx static %1 primary validate=no | Out-Null\n").arg(ns6[0]);
            for (int i = 1; i < ns6.size(); ++i) script += QString("  netsh interface ipv6 add dnsservers name=$idx %1 index=%2 validate=no | Out-Null\n").arg(ns6[i]).arg(i + 1);
        }
        script += "}\n";
    }
    script += "Clear-DnsClientCache\n";

    QProcess p;
    hide_console(&p);
    p.start(powershell_exe(), powershell_args(script));
    const bool ok = p.waitForFinished(30000) && p.exitCode() == 0;
    if (ok) QFile::remove(dns_backup_path());
    return ok;
}

void PowerToysDashboard::apply_dns_mode(std::function<void(bool ok, const QString& error)> done) {
    if (takanosu_dns_mode_active()) { done(true, QString()); return; } // zaten açık; yedeğin üzerine yazma
    if (!dns_mode_supported()) { done(false, "Şifreli DNS modu Windows 11 gerektirir."); return; }

    QFile::remove(dns_backup_path());
    QString script = QString::fromUtf8(DNS_APPLY_SCRIPT);
    script.replace("__BACKUP_PATH__", QDir::toNativeSeparators(dns_backup_path()).replace("'", "''"));

    run_async(powershell_exe(), powershell_args(script), 45000, [done](int, const QString&) {
        QFile f(dns_backup_path());
        if (!f.open(QIODevice::ReadOnly)) { done(false, "DNS ayarları uygulanamadı (yedek oluşmadı)."); return; }
        const QJsonObject backup = QJsonDocument::fromJson(f.readAll()).object();
        f.close();

        const QString error = backup.isEmpty() ? QString("yedek okunamadı") : backup.value("error").toString();
        if (!error.isEmpty()) {
            takanosu_restore_dns(); // yarım kalan değişiklikleri hemen geri al
            done(false, "DNS ayarlanamadı: " + error);
            return;
        }
        done(true, QString());
    });
}

void PowerToysDashboard::revert_dns_mode() {
    if (!takanosu_dns_mode_active()) return;
    log("DNS", "Şifreli DNS modu kapatılıyor, önceki DNS ayarlarınız geri yükleniyor...");
    if (takanosu_restore_dns()) log("DNS", "Önceki DNS ayarları geri yüklendi.");
    else log("HATA", "DNS ayarları geri yüklenemedi. Ayarlar > Ağ > DNS bölümünden 'Otomatik (DHCP)' seçebilirsiniz.");
}

// Bir profili hazırlar: DNS profili ise şifreli DNS'i açar, motor profili ise (DNS modunu kapatıp) motoru başlatır.
void PowerToysDashboard::begin_profile(const DpiProfile& profile, std::function<void(bool ok, const QString& error)> ready) {
    if (profile.is_dns) {
        stop_engine(false);
        log("DNS", "Sistem DNS'i geçici olarak Cloudflare Şifreli DNS'e (DoH) alınıyor. İstediğiniz an tamamen geri alınabilir.");
        apply_dns_mode(ready);
        return;
    }
    revert_dns_mode();
    QString error;
    const bool ok = start_engine(profile, &error);
    ready(ok, error);
}

// ==========================================
// 🚀 MOTOR YÖNETİMİ
// Eskiden her denemede .bat dosyası yazılıp Windows servisi (start=auto) kuruluyordu.
// Artık motor doğrudan uygulamanın alt süreci: uygulama kapanınca motor ve WinDivert de kapanır,
// bilgisayar açılışında kendiliğinden servis olarak çalışıp anti-cheat'lere takılmaz.
// ==========================================
void PowerToysDashboard::reapply_engine() {
    if (is_busy || game_running || active_profile_id.isEmpty()) return;
    const DpiProfile* p = find_profile(active_profile_id);
    if (!p || p->is_dns) return; // Şifreli DNS modu listeleri kullanmaz
    log("TÜNEL", "Liste değişti, motor aynı profille yeniden başlatılıyor...");
    try_single_profile(active_profile_id);
}

bool PowerToysDashboard::is_engine_running() const {
    for (QProcess* p : engine_procs) {
        if (p->state() == QProcess::Running) return true;
    }
    return takanosu_dns_mode_active();
}

void PowerToysDashboard::stop_engine(bool unload_driver) {
    for (QProcess* p : engine_procs) {
        p->disconnect(this);
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(3000);
        }
        p->deleteLater();
    }
    engine_procs.clear();
    active_profile_id.clear();

    if (!legacy_cleaned) {
        // v1.1.0 ve öncesinden kalan otomatik başlayan servisleri ve yetim motorları bir kereliğine temizle
        legacy_cleaned = true;
        for (const QString& svc : { QString("GoodbyeDPI_Elite"), QString("GoodbyeDPI") }) {
            run_hidden_sync("sc.exe", { "stop", svc });
            run_hidden_sync("sc.exe", { "delete", svc });
        }
        run_hidden_sync("taskkill.exe", { "/F", "/IM", "goodbyedpi.exe" });
        run_hidden_sync("taskkill.exe", { "/F", "/IM", "winws.exe" });
    }

    if (unload_driver) {
        // WinDivert çekirdek sürücüsünü bellekten at (anti-cheat'ler bu sürücüye takılıyor)
        run_hidden_sync("sc.exe", { "stop", "WinDivert" });
    }
}

bool PowerToysDashboard::start_engine(const DpiProfile& profile, QString* error) {
    stop_engine(false);
    engine_output.clear();

    const QString app_dir = QCoreApplication::applicationDirPath();
    const QString gdpi_dir = QDir::toNativeSeparators(app_dir + "/goodbyedpi/x86_64");
    const QString gdpi_exe = gdpi_dir + "\\goodbyedpi.exe";
    // Tünel açıksa tünele giden siteler listeden çıkarılmış kopyalar kullanılır
    const QString list_path = QDir::toNativeSeparators(tunnel_filtered_list(shipped_list_path(), "motor-liste.txt"));
    // Kullanıcının eklediği siteler ayrı dosyada; boşsa motora hiç verilmez
    const QString custom_path = custom_list_has_entries() ? QDir::toNativeSeparators(tunnel_filtered_list(custom_list_path(), "motor-ozel-liste.txt")) : QString();
    const QString zapret_dir = QDir::toNativeSeparators(app_dir + "/zapret");

    struct Launch { QString exe; QStringList args; QString workdir; };
    QList<Launch> launches;

    if (!QFileInfo::exists(gdpi_exe)) { *error = "GoodbyeDPI motoru bulunamadı (goodbyedpi\\x86_64\\goodbyedpi.exe)."; return false; }
    if (!QFileInfo::exists(list_path)) { *error = "Kara liste bulunamadı (goodbyedpi\\turkey-blacklist.txt)."; return false; }

    if (profile.is_zapret) {
        const QString winws_exe = zapret_dir + "\\winws.exe";
        if (!QFileInfo::exists(winws_exe)) { *error = "Zapret motoru bulunamadı (zapret\\winws.exe)."; return false; }
        QStringList args;
        for (QString a : profile.args) {
            a.replace("{FAKE}", zapret_dir + "\\fake\\");
            if (a.contains("{LIST}")) {
                // winws --hostlist birden çok kez verilebilir: önce hazır liste, sonra kullanıcının listesi
                args << QString(a).replace("{LIST}", list_path);
                if (!custom_path.isEmpty()) args << QString(a).replace("{LIST}", custom_path);
                continue;
            }
            args << a;
        }
        launches << Launch{ winws_exe, args, zapret_dir };
        // Zapret DNS yönlendirmesi yapmaz. ISS'nin DNS zehirlemesine karşı GoodbyeDPI'ı SADECE DNS kalkanı olarak yanında çalıştırıyoruz.
        launches << Launch{ gdpi_exe, DNS_YANDEX, gdpi_dir };
    }
    else {
        // GoodbyeDPI --blacklist birden çok kez verilebilir
        QStringList args = profile.args + QStringList{ "--blacklist", list_path };
        if (!custom_path.isEmpty()) args << "--blacklist" << custom_path;
        launches << Launch{ gdpi_exe, args, gdpi_dir };
    }

    for (const Launch& l : launches) {
        QProcess* p = new QProcess(this);
        hide_console(p);
        p->setProcessChannelMode(QProcess::MergedChannels);
        p->setWorkingDirectory(l.workdir);
        connect(p, &QProcess::readyReadStandardOutput, this, [this, p]() {
            engine_output += oem_text(p->readAllStandardOutput());
            if (engine_output.size() > 8000) engine_output = engine_output.right(8000);
        });
        // Motor, sistem aktifken beklenmedik şekilde kapanırsa kullanıcıyı uyar
        connect(p, &QProcess::finished, this, [this]() {
            if (active_profile_id.isEmpty() || is_busy) return;
            active_profile_id.clear();
            set_progress(100, "#ff5252");
            set_ui_state(UiState::Fail, "⚠️ DPI motoru beklenmedik şekilde kapandı! Sistemi yeniden başlatın.", "Durum: MOTOR DURDU");
            log("HATA", "DPI motoru beklenmedik şekilde kapandı.");
            if (!engine_output.trimmed().isEmpty()) log("MOTOR", engine_output.trimmed().right(600));
            set_start_button(true, "YENİDEN BAŞLAT / GÜNCELLE");
            emit engine_state_changed(false, QString());
        });

        p->start(l.exe, l.args);
        if (!p->waitForStarted(4000)) {
            *error = QString("%1 başlatılamadı: %2").arg(QFileInfo(l.exe).fileName(), p->errorString());
            p->disconnect(this);
            delete p;
            stop_engine(false);
            return false;
        }
        engine_procs << p;

        if (engine_job) {
            HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(p->processId()));
            if (h) {
                AssignProcessToJobObject(static_cast<HANDLE>(engine_job), h);
                CloseHandle(h);
            }
        }
    }
    return true;
}

// ==========================================
// 🛰️ OTONOM TARAMA
// ==========================================
void PowerToysDashboard::start_autonomous_system() {
    if (is_busy) return;
    is_busy = true;
    restoring = false;
    single_try = false;
    reset_game_state();
    const int gen = ++scan_generation;

    set_start_button(false, "SİSTEM ÇALIŞIYOR...");
    set_progress(5, "#ff9800");
    set_ui_state(UiState::Busy, "Adım 1: Eski motorlar temizleniyor ve bağlantı kontrol ediliyor...", "Aktif Mod: SİSTEM TEST EDİLİYOR...");

    log_console->appendPlainText("");
    log("SİSTEM", "Ön Bağlantı Kontrolü (Pre-flight Check) yapılıyor...");

    stop_engine(false);
    revert_dns_mode(); // ölçüm temiz olsun: önceki şifreli DNS ayarı açıksa kapat
    emit engine_state_changed(false, QString());

    check_internet([this, gen](bool online) {
        if (gen != scan_generation) return;

        if (!online) {
            fail_no_internet("İnternet bağlantısı algılanamadı. İşlem iptal edildi.");
            return;
        }

        const int selected_iss = combo_iss->currentIndex();
        if (selected_iss == 0) {
            log("YAPAY ZEKA", "Otonom Tarama Başlatıldı!");
            log("RADAR", "ISS Tespiti için ağa sinyal gönderiliyor...");
            detect_isp([this, gen](int isp, const QString& text) {
                if (gen != scan_generation) return;
                if (text.isEmpty()) log("RADAR", "ISS Tespit edilemedi! Standart hücum matrisi yükleniyor.");
                else if (isp == 0) log("RADAR", QString("Tanınmayan ISS (%1). Evrensel hücum matrisi yükleniyor.").arg(text));
                else log("RADAR", QString("Tespit Edilen Gerçek ISS: %1").arg(text));
                start_scan(isp);
            });
        }
        else {
            log(combo_iss->currentText().toUpper(), "Özel altyapı profili yükleniyor...");
            start_scan(selected_iss);
        }
    });
}

void PowerToysDashboard::start_scan(int isp_index) {
    const int gen = scan_generation;
    log("RADAR", "Motor kapalıyken sansür durumu ölçülüyor...");
    run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 5000, [this, gen, isp_index](int, const QString&) {
        if (gen != scan_generation) return;
        test_dpi_bypass([this, gen, isp_index](bool open, const QString&) {
            if (gen != scan_generation) return;
            if (open) log("RADAR", "Motor kapalıyken Discord'a erişilebiliyor (özel DNS/VPN kullanıyor olabilirsiniz). Yine de en uygun profil seçiliyor.");
            else log("RADAR", "Sansür doğrulandı: motor kapalıyken Discord'a erişilemiyor.");
            build_and_run_scan(isp_index);
        });
    });
}

void PowerToysDashboard::build_and_run_scan(int isp_index) {
    candidates.clear();
    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    const int engine_mode = settings.value("engine_mode", settings.value("last_engine_index", 0)).toInt(); // 0 = ikisi de, 1 = sadece GoodbyeDPI, 2 = sadece Zapret
    const QStringList disabled = settings.value("disabled_profiles").toStringList(); // DPI Modları sayfasında işareti kaldırılanlar
    const bool has_zapret = zapret_installed();

    for (const QString& id : profile_order_for_isp(isp_index)) {
        const DpiProfile* p = find_profile(id);
        if (!p) continue;
        if (engine_mode == 1 && (p->is_zapret || p->is_dns)) continue;
        if (engine_mode == 2 && !p->is_zapret) continue;
        if (p->is_dns && !dns_mode_supported()) continue;
        if (p->is_zapret && !has_zapret) continue;
        if (disabled.contains(p->id)) continue;
        candidates << *p;
    }

    if (engine_mode == 2 && !has_zapret) {
        log("HATA", "Zapret motoru kurulu değil (zapret\\winws.exe bulunamadı). Motor seçimini 'Otomatik' yapın.");
    }
    log("SİSTEM", QString("%1 aşamalı otonom motor hazırlandı.").arg(candidates.size()));

    current_auto_test_index = 0;
    run_auto_test_step();
}

void PowerToysDashboard::run_auto_test_step() {
    if (!is_busy) return; // Kill DPI ile iptal edildiyse taramayı sürdürme
    if (current_auto_test_index >= candidates.size()) {
        on_scan_failed();
        return;
    }

    const DpiProfile& profile = candidates[current_auto_test_index];
    const int step = current_auto_test_index + 1;
    const int total = candidates.size();

    set_progress(10 + (85 * current_auto_test_index) / total, "#ff9800");
    set_ui_state(UiState::Busy,
        QString("Otonom Tarama (%1/%2): %3 deneniyor...").arg(step).arg(total).arg(profile.name),
        QString("Tarama Sürüyor... (%1/%2)").arg(step).arg(total));

    log_console->appendPlainText("");
    log(QString("TEST %1").arg(step), QString("%1 modülüne geçildi.").arg(profile.name));
    if (profile.is_dns) log("PARAM", "Windows DoH → 1.1.1.1 / 1.0.0.1 (cloudflare-dns.com)");
    else log("PARAM", QString("%1 %2").arg(profile.is_zapret ? "winws" : "goodbyedpi", profile.args.join(' ')));

    const int gen = scan_generation;
    begin_profile(profile, [this, gen](bool ok, const QString& error) {
        if (gen != scan_generation) { revert_dns_mode(); return; } // bu arada iptal edildiyse yarım kalan DNS değişikliğini geri al
        if (!ok) {
            log("HATA", error);
            current_auto_test_index++;
            QTimer::singleShot(0, this, &PowerToysDashboard::run_auto_test_step);
            return;
        }
        // WinDivert filtresinin / yeni DNS ayarının oturması için kısa bir bekleme
        QTimer::singleShot(1500, this, &PowerToysDashboard::verify_current_step);
    });
}

void PowerToysDashboard::verify_current_step() {
    const int gen = scan_generation;
    if (!is_busy || current_auto_test_index >= candidates.size()) return;
    const DpiProfile profile = candidates[current_auto_test_index];

    if (!is_engine_running()) {
        log("HATA", QString("%1 motoru açılır açılmaz kapandı.").arg(profile.name));
        if (!engine_output.trimmed().isEmpty()) log("MOTOR", engine_output.trimmed().right(600));
        current_auto_test_index++;
        run_auto_test_step();
        return;
    }

    // Zehirlenmiş DNS kayıtları önbellekte kalmasın diye testten ÖNCE temizle
    run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 5000, [this, gen, profile](int, const QString&) {
        if (gen != scan_generation) return;
        log("RADAR", "Ağ çıkışı hazır. DPI sansür duvarı (Discord HTTPS) test ediliyor...");

        test_dpi_bypass([this, gen, profile](bool passed, const QString& detail) {
            if (gen != scan_generation) return;
            if (passed && !is_engine_running()) {
                // Ağ zaten açıksa test geçebilir; motor bu arada kapandıysa bu bir başarı DEĞİLDİR
                log("HATA", QString("%1 motoru test sırasında kapandı.").arg(profile.name));
                if (!engine_output.trimmed().isEmpty()) log("MOTOR", engine_output.trimmed().right(600));
                current_auto_test_index++;
                run_auto_test_step();
                return;
            }
            if (passed) {
                on_bypass_success(profile);
                return;
            }
            log("HATA", QString("%1 duvara çarptı (%2). Bir sonraki silah çekiliyor...").arg(profile.name, detail));
            current_auto_test_index++;
            run_auto_test_step();
        });
    });
}

void PowerToysDashboard::on_bypass_success(const DpiProfile& profile) {
    const bool was_restore = restoring;
    active_profile_id = profile.id;
    restoring = false;
    single_try = false;
    is_busy = false;

    set_progress(100, "#00E676");
    set_ui_state(UiState::Success,
        was_restore ? "Kayıtlı profil doğrulandı. Sistem aktif! Özgür internetin tadını çıkarın." : "Sistem başarıyla aktif edildi! Özgür internetin tadını çıkarın.",
        "Aktif Mod: " + profile.name);
    log("BAŞARI", QString("Duvar Kırıldı! İnternet Sansürü Aşıldı (%1). İyi uçuşlar Kaptan!").arg(profile.name));

    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    settings.setValue("last_iss_index", combo_iss->currentIndex());
    settings.setValue("last_profile_id", profile.id);
    settings.setValue("last_active_mod", profile.name);
    settings.remove("last_working_param");
    if (!was_restore) {
        settings.setValue("total_launches", settings.value("total_launches", 0).toInt() + 1);
    }

    set_start_button(true, "YENİDEN BAŞLAT / GÜNCELLE");
    emit engine_state_changed(true, profile.name);
}

void PowerToysDashboard::on_scan_failed() {
    if (restoring) {
        restoring = false;
        is_busy = false;
        log("SİSTEM", "Kayıtlı profil bu ağda artık çalışmıyor. Tam otonom tarama başlatılıyor...");
        start_autonomous_system();
        return;
    }

    stop_engine(true);
    revert_dns_mode();
    set_progress(100, "#ff5252");
    if (single_try) {
        single_try = false;
        set_ui_state(UiState::Fail, "⚠️ Seçilen profil bu ağda çalışmadı. Başka bir profil deneyin veya otonom taramayı başlatın.", "Durum: PROFİL ÇALIŞMADI");
        log("SONUÇ", "Elle denenen profil Discord'a ulaşamadı.");
    }
    else if (candidates.isEmpty()) {
        set_ui_state(UiState::Fail, "⚠️ Denenecek profil yok! DPI Modları sayfasında motor seçimini ve profil işaretlerini kontrol edin.", "Durum: PROFİL BULUNAMADI");
    }
    else {
        set_ui_state(UiState::Fail, "⚠️ Bütün sürümler başarısız! Lütfen GitHub üzerinden bizimle iletişime geçiniz.", "Durum: AĞIR SANSÜR TESPİT EDİLDİ");
        log("KRİTİK HATA", "Ağır DPI sansürü aşılamadı! Hiçbir sürüm çalışmadı. (Logları GitHub'da paylaşabilirsiniz)");
    }
    is_busy = false;
    set_start_button(true, "YENİDEN DENE");
    emit engine_state_changed(false, QString());
}

void PowerToysDashboard::fail_no_internet(const QString& log_text) {
    stop_engine(true);
    revert_dns_mode();
    restoring = false;
    single_try = false;
    is_busy = false;
    set_progress(100, "#ff5252");
    set_ui_state(UiState::Fail, "⚠️ BAĞLANTI HATASI: Lütfen internet bağlantınızı kontrol edin!", "Durum: İNTERNET BAĞLANTISI YOK");
    log("KRİTİK HATA", log_text);
    set_start_button(true, "YENİDEN DENE");
    emit engine_state_changed(false, QString());
}

// DPI Modları sayfasındaki "Şimdi Dene": tek bir profili başlatıp gerçek Discord testi yapar
void PowerToysDashboard::try_single_profile(const QString& id) {
    const DpiProfile* profile = find_profile(id);
    if (!profile) return;
    if (is_busy) {
        log("UYARI", "Devam eden bir tarama var. Bitmesini bekleyin veya 'Oyun Modu' ile durdurun.");
        return;
    }

    is_busy = true;
    restoring = false;
    single_try = true;
    reset_game_state();
    const int gen = ++scan_generation;
    const DpiProfile chosen = *profile;

    set_start_button(false, "SİSTEM ÇALIŞIYOR...");
    set_progress(5, "#ff9800");
    set_ui_state(UiState::Busy, "Seçilen profil hazırlanıyor...", "Aktif Mod: SİSTEM TEST EDİLİYOR...");
    log_console->appendPlainText("");
    log("MANUEL", QString("%1 profili elle deneniyor...").arg(chosen.name));

    stop_engine(false);
    revert_dns_mode();
    emit engine_state_changed(false, QString());

    check_internet([this, gen, chosen](bool online) {
        if (gen != scan_generation) return;
        if (!online) {
            fail_no_internet("İnternet bağlantısı algılanamadı. İşlem iptal edildi.");
            return;
        }
        candidates = { chosen };
        current_auto_test_index = 0;
        run_auto_test_step();
    });
}

// ==========================================
// 💾 AÇILIŞTA SON ÇALIŞAN PROFİLİ GERİ YÜKLE
// ==========================================
void PowerToysDashboard::restore_last_session() {
    if (is_busy) return;

    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    QString id = settings.value("last_profile_id").toString();

    if (id.isEmpty()) {
        // v1.1.0 ve öncesi ham parametreyi kayıt defterinde saklıyordu. Bilinen bir profile denk geliyorsa
        // o profile çevir, gelmiyorsa ASLA çalıştırma (kayıt defterine yazılan komutlar yönetici olarak çalışabiliyordu).
        const QString legacy = settings.value("last_working_param").toString().trimmed();
        for (const DpiProfile& p : dpi_profiles()) {
            if (!p.is_zapret && p.args.join(' ') == legacy) id = p.id;
        }
        settings.remove("last_working_param");
        if (!id.isEmpty()) settings.setValue("last_profile_id", id);
    }

    const DpiProfile* profile = find_profile(id);
    if (!profile || (profile->is_zapret && !zapret_installed())) {
        set_ui_state(UiState::Idle, "Sistem Beklemede. Hedef ISS'yi seçin ve başlatın.", "Aktif Mod: BEKLENİYOR...");
        return;
    }

    is_busy = true;
    restoring = true;
    restore_retry = 0;
    pending_restore = *profile;
    ++scan_generation;

    set_start_button(false, "SİSTEM ÇALIŞIYOR...");
    set_ui_state(UiState::Busy, "Kayıtlı profil uyandırılıyor...", "Son Çalışan Mod: " + profile->name);
    log_console->appendPlainText("");
    log("SİSTEM", QString("Önceki başarılı oturum tespit edildi (%1). Bağlantı bekleniyor...").arg(profile->name));
    restore_wait_step();
}

// Bilgisayar açılışında ağ hemen hazır olmayabilir: 60 saniyeye kadar internetin gelmesini bekle
void PowerToysDashboard::restore_wait_step() {
    const int gen = scan_generation;
    check_internet([this, gen](bool online) {
        if (gen != scan_generation) return;
        if (!online) {
            if (++restore_retry < 12) {
                QTimer::singleShot(5000, this, [this, gen]() { if (gen == scan_generation) restore_wait_step(); });
                return;
            }
            fail_no_internet("60 saniye boyunca internet bağlantısı gelmedi. Kayıtlı profil başlatılamadı.");
            return;
        }
        candidates = { pending_restore };
        current_auto_test_index = 0;
        run_auto_test_step();
    });
}

void PowerToysDashboard::kill_dpi_service() {
    ++scan_generation; // devam eden tarama varsa iptal et
    reset_game_state();
    is_busy = false;
    restoring = false;
    single_try = false;

    log_console->appendPlainText("");
    log("KILL DPI", "Kırmızı Alarm! Tüm DPI motorları durduruluyor...");
    stop_engine(true);
    revert_dns_mode();
    run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 5000, nullptr);

    set_start_button(true, "OTONOM SİSTEMİ BAŞLAT");
    set_progress(0, "#8a2be2");
    set_ui_state(UiState::Stopped, "⚠️ Sistem Durduruldu! Motorlar ve WinDivert sürücüsü kapatıldı (Oyun Modu).", "Aktif Mod: BEKLENİYOR...");
    log("KILL DPI", "Motorlar ve WinDivert sürücüsü kapatıldı. Anti-cheat kullanan oyunlara girebilirsiniz.");
    emit engine_state_changed(false, QString());
}

// ==========================================
// 🛠️ SİSTEM ARAÇLARI
// ==========================================
void PowerToysDashboard::util_flush_dns() {
    log_console->appendPlainText("");
    log("ARAÇLAR", "DNS Önbelleği (Flush DNS) temizleniyor...");
    run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 10000, [this](int code, const QString&) {
        if (code == 0) log("BAŞARI", "DNS Önbelleği başarıyla temizlendi!");
        else log("HATA", "DNS önbelleği temizlenemedi.");
    });
}

// ==========================================
// 🌐 IPv6 DNS KONTROLÜ
// Bazı modemlerin IPv6 DNS'i (fe80::1) belirli alan adlarının AAAA sorgularını cevapsız bırakıyor: IPv4 DNS'i elle
// 8.8.8.8 yapılmış olsa bile Windows IPv6 sunucusunu da soruyor ve Discord gibi uygulamalar her bağlantıda ~12 sn
// bekliyor (2026-10-13'te ölçüldü). Burada her ağ kartının IPv6 DNS'leri Google ile kıyaslanır.
// ==========================================
static const char* IPV6_DNS_CHECK_SCRIPT = R"PS(
$ProgressPreference = 'SilentlyContinue'
# Tek ölçüm UDP kaybına takılabiliyor: iki kez denenir, iyisi alınır
function T1($srv, $name) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    try { Resolve-DnsName $name -Type AAAA -Server $srv -DnsOnly -NoHostsFile -QuickTimeout -ErrorAction Stop | Out-Null; $ok = $true } catch { $ok = $false }
    [ordered]@{ ms = [int]$sw.ElapsedMilliseconds; ok = $ok }
}
function T($srv) {
    $a = T1 $srv 'discord.com'
    if (-not $a.ok -or $a.ms -ge 2500) { $b = T1 $srv 'gateway.discord.gg'; if ($b.ok -and (-not $a.ok -or $b.ms -lt $a.ms)) { $a = $b } }
    [ordered]@{ s = "$srv"; ms = $a.ms; ok = $a.ok }
}
$res = @()
foreach ($c in Get-NetIPConfiguration | Where-Object { $_.IPv4DefaultGateway -or $_.IPv6DefaultGateway }) {
    $idx = $c.InterfaceIndex
    if ($c.InterfaceAlias -eq 'Elite Tunel') { continue }
    $guid = (Get-NetAdapter -InterfaceIndex $idx).InterfaceGuid
    $st = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip6\Parameters\Interfaces\$guid" -Name NameServer -ErrorAction SilentlyContinue).NameServer
    $st = "$st".Trim([char]0, ' ')
    $servers = @()
    foreach ($s in (Get-DnsClientServerAddress -InterfaceIndex $idx -AddressFamily IPv6).ServerAddresses) {
        if ($s -like 'fe80*' -and $s -notlike '*%*') { $s = "$s%$idx" }
        $servers += T $s
    }
    $ref = $null
    if ($c.IPv6DefaultGateway) { $ref = T '2001:4860:4860::8888' }
    $res += [ordered]@{ alias = "$($c.InterfaceAlias)"; guid = "$guid"; idx = $idx; static6 = "$st"; gw6 = [bool]$c.IPv6DefaultGateway; servers = $servers; ref = $ref }
}
ConvertTo-Json -InputObject @($res) -Depth 5 -Compress
)PS";

static const int IPV6_DNS_SLOW_MS = 2500;
static const char* IPV6_FIX_SERVERS[] = { "2001:4860:4860::8888", "2620:fe::fe" };

// bad: IPv6 interneti olan, Google'ın hızlı cevap verdiği ama kendi IPv6 DNS'i yavaş/cevapsız kalan kart
struct Ipv6DnsIssue { QString alias, guid, static6; int idx = 0; QStringList lines; bool bad = false; };

static QList<Ipv6DnsIssue> parse_ipv6_dns_check(const QString& out) {
    QList<Ipv6DnsIssue> list;
    const int start = out.indexOf('[');
    const QJsonArray arr = QJsonDocument::fromJson(out.mid(start < 0 ? 0 : start).toUtf8()).array();
    for (const QJsonValue& v : arr) {
        const QJsonObject a = v.toObject();
        Ipv6DnsIssue r;
        r.alias = a.value("alias").toString(); r.guid = a.value("guid").toString(); r.static6 = a.value("static6").toString(); r.idx = a.value("idx").toInt();
        const QJsonObject ref = a.value("ref").toObject();
        const bool ref_ok = ref.value("ok").toBool() && ref.value("ms").toInt() < IPV6_DNS_SLOW_MS;
        if (!a.value("gw6").toBool()) { r.lines << QString("%1: IPv6 interneti yok, kontrol gerekmiyor.").arg(r.alias); list << r; continue; }
        for (const QJsonValue& sv : a.value("servers").toArray()) {
            const QJsonObject sr = sv.toObject();
            const bool ok = sr.value("ok").toBool();
            const int ms = sr.value("ms").toInt();
            const bool slow = !ok || ms >= IPV6_DNS_SLOW_MS;
            r.lines << QString("%1 %2: IPv6 DNS %3 → %4").arg(slow ? "⚠️" : "✅", r.alias, sr.value("s").toString(),
                                                            ok ? QString("%1 ms").arg(ms) : QString("cevap yok (%1 ms)").arg(ms));
            if (slow && ref_ok) r.bad = true;
        }
        if (!ref.isEmpty()) r.lines << QString("   Karşılaştırma: Google IPv6 DNS → %1").arg(ref.value("ok").toBool() ? QString("%1 ms").arg(ref.value("ms").toInt()) : QString("cevap yok"));
        list << r;
    }
    return list;
}

// "Eski Haline Döndür"de ve kaldırmada çağrılır: yalnız Elite'in değiştirdiği kartların IPv6 DNS'i önceki haline döner
bool takanosu_restore_ipv6_dns() {
    QSettings s(takanosu_settings_path(), QSettings::IniFormat);
    const QStringList guids = s.value("ipv6_dns_fix/guids").toStringList();
    if (guids.isEmpty()) return true;
    static const QRegularExpression guid_re("^\\{[0-9A-Fa-f-]{36}\\}$");
    QString script = "$ErrorActionPreference = 'Continue'\n";
    for (const QString& g : guids) {
        if (!guid_re.match(g).hasMatch()) continue;
        const QStringList ns6 = safe_ip_list(s.value("ipv6_dns_fix/" + QString(g).remove('{').remove('}')).toString());
        script += QString("$idx = (Get-NetAdapter | Where-Object { $_.InterfaceGuid -eq '%1' }).ifIndex\nif ($idx) {\n").arg(g);
        if (ns6.isEmpty()) script += "  netsh interface ipv6 set dnsservers name=$idx source=dhcp | Out-Null\n";
        else {
            script += QString("  netsh interface ipv6 set dnsservers name=$idx static %1 primary validate=no | Out-Null\n").arg(ns6[0]);
            for (int i = 1; i < ns6.size(); ++i) script += QString("  netsh interface ipv6 add dnsservers name=$idx %1 index=%2 validate=no | Out-Null\n").arg(ns6[i]).arg(i + 1);
        }
        script += "}\n";
    }
    script += "Clear-DnsClientCache\n";
    QProcess p;
    hide_console(&p);
    p.start(powershell_exe(), powershell_args(script));
    const bool ok = p.waitForFinished(30000) && p.exitCode() == 0;
    if (ok) {
        for (const QString& g : guids) s.remove("ipv6_dns_fix/" + QString(g).remove('{').remove('}'));
        s.remove("ipv6_dns_fix/guids");
    }
    return ok;
}

void PowerToysDashboard::util_ipv6_dns_check() {
    log_console->appendPlainText("");
    // Şifreli DNS modunda (ör. Oyun Modu sırasında Discord için) DNS Cloudflare'e şifreli gidiyor; düz sorguyla ölçüm
    // yanlış "cevap yok" veriyordu (Batın, 2026-10-14). Bu modda ölçülmez.
    if (takanosu_dns_mode_active()) {
        log("ARAÇLAR", "Şifreli DNS modu açık (Oyun Modu sırasında Discord için): IPv6 DNS bu modda ölçülmez. Oyunu kapatınca tekrar dene.");
        QMessageBox box(this);
        box.setWindowTitle("IPv6 DNS Kontrolü");
        box.setStyleSheet("QMessageBox { background-color: #1a1a1a; } QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
                          "QPushButton { background-color: #333333; color: white; border-radius: 8px; padding: 8px 18px; font-family: 'Segoe UI Variable'; font-weight: bold; border: 1px solid #555555; min-width: 110px; }");
        box.setText("ℹ️ Şu an Şifreli DNS modu açık.");
        box.setInformativeText("Oyun Modu sırasında DNS sorguları şifreli olarak Cloudflare'e gidiyor; bu sırada ölçüm doğru sonuç vermez. Oyunu kapattıktan sonra tekrar dene.");
        box.addButton("Tamam", QMessageBox::AcceptRole);
        box.exec();
        return;
    }
    log("ARAÇLAR", "IPv6 DNS kontrolü başladı (yaklaşık 10-30 sn)...");
    run_async(powershell_exe(), powershell_args(IPV6_DNS_CHECK_SCRIPT), 90000, [this](int, const QString& out) {
        const QList<Ipv6DnsIssue> res = parse_ipv6_dns_check(out);
        if (res.isEmpty()) { log("HATA", "IPv6 DNS kontrolü sonuç vermedi."); return; }
        QStringList bad_aliases;
        for (const Ipv6DnsIssue& r : res) {
            for (const QString& l : r.lines) log_console->appendPlainText(l);
            if (r.bad) bad_aliases << r.alias;
        }
        QSettings s(takanosu_settings_path(), QSettings::IniFormat);
        const bool has_backup = !s.value("ipv6_dns_fix/guids").toStringList().isEmpty();

        QMessageBox box(this);
        box.setWindowTitle("IPv6 DNS Kontrolü");
        box.setStyleSheet(
            "QMessageBox { background-color: #1a1a1a; } QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
            "QPushButton { background-color: #333333; color: white; border-radius: 8px; padding: 8px 18px; font-family: 'Segoe UI Variable'; font-weight: bold; border: 1px solid #555555; min-width: 110px; } "
            "QPushButton:hover { background-color: #444444; }");
        QPushButton* btn_fix = nullptr;
        QPushButton* btn_undo = nullptr;
        if (!bad_aliases.isEmpty()) {
            box.setText("⚠️ IPv6 DNS sunucun yavaş ya da bazı sorgulara cevap vermiyor.");
            QString info = QString("Etkilenen bağlantı: %1\n\nBu, Discord gibi uygulamalarda mesajların ve fotoğrafların saniyelerce geç gelmesine yol açar. "
                                   "Düzelt dersen bu bağlantının IPv6 DNS'i Google (2001:4860:4860::8888) ve Quad9 (2620:fe::fe) yapılır; IPv4 ayarına dokunulmaz. "
                                   "İstediğin zaman buradan geri alabilirsin.").arg(bad_aliases.join(", "));
            if (takanosu_dns_mode_active()) info += "\n\nNot: Şifreli DNS modu açıkken DNS'i o mod yönetir; önce onu kapat.";
            else { btn_fix = box.addButton("Düzelt", QMessageBox::YesRole); btn_fix->setStyleSheet("background-color: #6a1b9a; color: white; border: none;"); }
            box.setInformativeText(info);
        }
        else {
            box.setText("✅ IPv6 DNS sorunsuz.");
            box.setInformativeText("Ayrıntılar kayıt penceresinde.");
        }
        if (has_backup) btn_undo = box.addButton("Eski Haline Döndür", QMessageBox::ActionRole);
        box.addButton("Tamam", QMessageBox::NoRole);
        box.exec();

        if (btn_fix && box.clickedButton() == btn_fix) {
            static const QRegularExpression guid_re("^\\{[0-9A-Fa-f-]{36}\\}$");
            QString script = "$ErrorActionPreference = 'Continue'\n";
            QStringList guids = s.value("ipv6_dns_fix/guids").toStringList();
            for (const Ipv6DnsIssue& r : res) {
                if (!r.bad || !guid_re.match(r.guid).hasMatch()) continue;
                if (!guids.contains(r.guid)) {
                    guids << r.guid;
                    // Önceki elle girilmiş IPv6 DNS yedeklenir (boş = otomatik/DHCP)
                    s.setValue("ipv6_dns_fix/" + QString(r.guid).remove('{').remove('}'), safe_ip_list(r.static6).join(','));
                }
                script += QString("$idx = (Get-NetAdapter | Where-Object { $_.InterfaceGuid -eq '%1' }).ifIndex\n").arg(r.guid);
                script += QString("if ($idx) { netsh interface ipv6 set dnsservers name=$idx static %1 primary validate=no | Out-Null; "
                                  "netsh interface ipv6 add dnsservers name=$idx %2 index=2 validate=no | Out-Null }\n").arg(IPV6_FIX_SERVERS[0], IPV6_FIX_SERVERS[1]);
            }
            script += "Clear-DnsClientCache\n";
            s.setValue("ipv6_dns_fix/guids", guids);
            s.sync();
            run_async(powershell_exe(), powershell_args(script), 30000, [this](int code, const QString&) {
                if (code == 0) log("BAŞARI", "IPv6 DNS Google + Quad9 yapıldı. Discord açıksa Ctrl+R ile yenile.");
                else log("HATA", "IPv6 DNS değiştirilemedi.");
            });
        }
        else if (btn_undo && box.clickedButton() == btn_undo) {
            if (takanosu_restore_ipv6_dns()) log("BAŞARI", "IPv6 DNS eski haline döndürüldü.");
            else log("HATA", "IPv6 DNS geri alınamadı.");
        }
    });
}

void PowerToysDashboard::util_fast_dns() {
    log_console->appendPlainText("");
    log("ARAÇLAR", "Hızlı DNS Optimizasyonu başlatıldı...");
    run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 10000, [this](int, const QString&) {
        run_async(sys_tool("ipconfig.exe"), { "/registerdns" }, 15000, [this](int code, const QString&) {
            if (code == 0) log("BAŞARI", "DNS kayıtları sisteme yeniden kaydedildi!");
            else log("HATA", "DNS kayıtları yenilenemedi.");
        });
    });
}

void PowerToysDashboard::util_restart_adapter() {
    log_console->appendPlainText("");
    log("ARAÇLAR", "Ağ Adaptörünüz yenileniyor (İnternetiniz anlık kopabilir)...");
    run_async(sys_tool("ipconfig.exe"), { "/release" }, 30000, [this](int, const QString&) {
        run_async(sys_tool("ipconfig.exe"), { "/renew" }, 60000, [this](int code, const QString&) {
            if (code == 0) log("BAŞARI", "IP Adresi DHCP üzerinden başarıyla yenilendi!");
            else log("HATA", "IP adresi yenilenemedi. Kablonuzu/Wi-Fi bağlantınızı kontrol edin.");
        });
    });
}

void PowerToysDashboard::util_ping_test() {
    QString target_ip = "8.8.8.8";
    if (const DpiProfile* p = find_profile(active_profile_id)) {
        if (p->is_dns || p->args.contains("1.1.1.1")) target_ip = "1.1.1.1";
        else if (p->is_zapret || p->args.contains("77.88.8.8")) target_ip = "77.88.8.8";
    }

    log_console->appendPlainText("");
    log("ARAÇLAR", QString("Gecikme (Ping) testi başlatıldı (Hedef: %1)...").arg(target_ip));
    run_async(sys_tool("ping.exe"), { "-n", "4", target_ip }, 20000, [this](int, const QString& out) {
        const QString text = out.trimmed();
        if (!text.isEmpty()) log_console->appendPlainText(text);
    });
}

void PowerToysDashboard::util_export_logs() {
    // Loglar artık Program Files yerine kullanıcının kendi AppData klasörüne yazılıyor
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    const QString log_path = dir + "/takanosu_logs.txt";

    QFile log_file(log_path);
    if (log_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&log_file);
        out << log_console->toPlainText();
        log_file.close();

        log("ARAÇLAR", QString("Loglar başarıyla txt dosyasına aktarıldı: %1").arg(QDir::toNativeSeparators(log_path)));
        QDesktopServices::openUrl(QUrl::fromLocalFile(log_path));
    }
    else {
        log("HATA", "Log dosyası yazılamadı.");
    }
}

void PowerToysDashboard::util_safe_net_check() {
    log_console->appendPlainText("");
    log("ARAÇLAR", "Güvenli İnternet (Aile Profili) ağda aranıyor...");

    // 1) DNS cevabı ISS'nin uyarı sunucularına mı yönleniyor?  2) Siteye HTTPS ile gerçekten ulaşılabiliyor mu?
    // (Eski sürüm İngilizce ping çıktısını arıyordu; Türkçe Windows'ta bu metinler hiç çıkmadığı için yanlış sonuç veriyordu.)
    QHostInfo::lookupHost("pastebin.com", this, [this](const QHostInfo& info) {
        bool isp_redirect = false;
        for (const QHostAddress& addr : info.addresses()) {
            const QString ip = addr.toString();
            if (ip.startsWith("195.175.") || ip.startsWith("212.156.")) isp_redirect = true;
        }

        run_async(sys_tool("curl.exe"),
            { "-s", "-o", "NUL", "-w", "%{http_code}", "--ssl-no-revoke", "--connect-timeout", "5", "--max-time", "8", "https://pastebin.com" },
            11000, [this, isp_redirect](int, const QString& out) {
                const int code = out.trimmed().toInt();
                const bool reachable = code >= 100 && code < 600;
                const bool is_safe_net_on = isp_redirect || !reachable;

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

                const QString title_color = is_dark_mode ? (is_safe_net_on ? "#ff5252" : "#00E676") : (is_safe_net_on ? "#d32f2f" : "#008B00");
                if (is_dark_mode) {
                    msgBox.setStyleSheet(
                        "QMessageBox { background-color: #1a1a1a; border: 1px solid #444444; border-radius: 12px; }"
                        "QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; padding-top: 5px; }"
                        "QLabel#qt_msgbox_label { color: " + title_color + "; font-size: 14px; font-weight: bold; margin-bottom: 5px; }"
                        "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; padding: 10px 30px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: none; margin-top: 15px; }"
                        "QPushButton:hover { background-color: #8e24aa; }"
                        "QPushButton:pressed { background-color: #4a148c; }"
                    );
                }
                else {
                    msgBox.setStyleSheet(
                        "QMessageBox { background-color: #f5f5f5; border: 1px solid #aaaaaa; border-radius: 12px; }"
                        "QLabel { color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; padding-top: 5px; }"
                        "QLabel#qt_msgbox_label { color: " + title_color + "; font-size: 14px; font-weight: bold; margin-bottom: 5px; }"
                        "QPushButton { background-color: #6a1b9a; color: white; border-radius: 8px; padding: 10px 30px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: none; margin-top: 15px; }"
                        "QPushButton:hover { background-color: #8e24aa; }"
                        "QPushButton:pressed { background-color: #4a148c; }"
                    );
                }

                msgBox.exec();
                log("BİLGİ", "Kontrol sonucu ekranda gösterildi.");
            });
    });
}

void PowerToysDashboard::util_winsock_reset() {
    log_console->appendPlainText("");
    log("ARAÇLAR", "Winsock (Ağ Katalogu) sıfırlanıyor...");
    run_async(sys_tool("netsh.exe"), { "winsock", "reset" }, 30000, [this](int, const QString&) {
        run_async(sys_tool("netsh.exe"), { "int", "ip", "reset" }, 30000, [this](int, const QString&) {
            log("BAŞARI", "Ağ ayarları sıfırlandı! Değişikliklerin tamamen uygulanması için bilgisayarınızı YENİDEN BAŞLATMANIZ gerekebilir.");
        });
    });
}

// ==========================================
// 🧪 OTOMATİK ÖZ-TEST (--selftest)
// Tüm profilleri TEK TEK dener (ilk başarılıda durmaz), Oyun Modu sonrası WinDivert'in kapandığını doğrular
// ve sonuçları rapor dosyasına yazar. Geliştirme / saha testi içindir.
// ==========================================
static QString run_capture_sync(const QString& exe, const QStringList& args) {
    QProcess p;
    hide_console(&p);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(sys_tool(exe), args);
    p.waitForFinished(8000);
    return oem_text(p.readAll());
}

void PowerToysDashboard::run_self_test(const QString& report_path) {
    st_path = report_path;
    st_profiles = dpi_profiles();
    st_index = 0;
    st_report.clear();
    st_report << QString("THE TAKANOSU ELITE v%1 — OTOMATİK ÖZ-TEST").arg(APP_VERSION)
              << QString("Tarih: %1").arg(QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss"))
              << QString("Windows: %1 (çekirdek %2)").arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion())
              << QString("Zapret kurulu: %1").arg(zapret_installed() ? "EVET" : "HAYIR")
              << QString("Şifreli DNS desteği: %1").arg(dns_mode_supported() ? "EVET (Windows 11)" : "HAYIR")
              << "";

    is_busy = true; // geri yükleme / motor çökme uyarıları araya girmesin
    ++scan_generation;
    stop_engine(false);
    revert_dns_mode();

    check_internet([this](bool online) {
        st_report << QString("İnternet (google generate_204): %1").arg(online ? "VAR" : "YOK");
        detect_isp([this](int isp, const QString& text) {
            st_report << QString("ISS tespiti: %1 (indeks %2)").arg(text.isEmpty() ? "tespit edilemedi" : text).arg(isp);
            run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 5000, [this](int, const QString&) {
                QElapsedTimer timer;
                timer.start();
                probe_discord(10, [this, timer](int ok, int total) {
                    st_report << QString("Motor KAPALIYKEN Discord: %1/%2 başarılı (%3) (%4 ms)")
                        .arg(ok).arg(total).arg(ok == 0 ? "ENGELLİ" : (ok == total ? "ENGEL YOK" : "ARALIKLI ENGEL")).arg(timer.elapsed());
                    st_report << "" << "── PROFİLLER ──";
                    self_test_next();
                });
            });
        });
    });
}

void PowerToysDashboard::self_test_next() {
    if (st_index >= st_profiles.size()) {
        // Oyun Modu testi: motorları kapat + WinDivert sürücüsünü bellekten at
        stop_engine(true);
        revert_dns_mode();
        st_report << "" << QString("DNS modu kapatıldıktan sonra yedek dosyası: %1").arg(takanosu_dns_mode_active() ? "HÂLÂ VAR (HATA)" : "SİLİNDİ (geri yüklendi)");
        QTimer::singleShot(1500, this, [this]() {
            const QString sc_out = run_capture_sync("sc.exe", { "query", "WinDivert" });
            QString state = "KAYITLI DEĞİL (sürücü tamamen kaldırıldı)";
            if (sc_out.contains("RUNNING")) state = "HÂLÂ ÇALIŞIYOR (HATA)";
            else if (sc_out.contains("STOPPED")) state = "DURDURULDU";
            const QString tasks = run_capture_sync("tasklist.exe", { "/FO", "CSV", "/NH" }).toLower();
            const bool leftovers = tasks.contains("goodbyedpi.exe") || tasks.contains("winws.exe");

            int passed = 0;
            for (const QString& line : st_report) if (line.startsWith("✅")) passed++;
            st_report << "" << "── OYUN MODU ──"
                      << QString("WinDivert sürücüsü: %1").arg(state)
                      << QString("Arkada kalan motor süreci: %1").arg(leftovers ? "VAR (HATA)" : "YOK")
                      << "" << QString("ÖZET: %1 / %2 profil Discord'a 10/10 ulaştı (⚠️ = aralıklı, güvenilmez).").arg(passed).arg(st_profiles.size());

            QFile f(st_path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                out << st_report.join('\n') << '\n';
            }
            if (st_in_app) finish_diagnostic_report();
            else QCoreApplication::exit(0);
        });
        return;
    }

    const DpiProfile p = st_profiles[st_index];
    if (st_in_app) {
        set_progress(5 + (90 * st_index) / st_profiles.size(), "#ff9800");
        set_ui_state(UiState::Busy, QString("🩺 Tanı testi (%1/%2): %3").arg(st_index + 1).arg(st_profiles.size()).arg(p.name), "Aktif Mod: TANI TESTİ");
    }
    const QString prefix = QString("[%1/%2] %3 (%4)").arg(st_index + 1).arg(st_profiles.size()).arg(p.name, p.id);

    begin_profile(p, [this, p, prefix](bool ok, const QString& error) {
    if (!ok) {
        st_report << QString("❌ %1 — BAŞLATILAMADI: %2").arg(prefix, error);
        st_index++;
        self_test_next();
        return;
    }

    QTimer::singleShot(1500, this, [this, p, prefix]() {
        if (!is_engine_running()) {
            st_report << QString("❌ %1 — MOTOR AÇILIR AÇILMAZ KAPANDI:\n      %2").arg(prefix, engine_output.trimmed().right(500).replace('\n', "\n      "));
            st_index++;
            self_test_next();
            return;
        }
        run_async(sys_tool("ipconfig.exe"), { "/flushdns" }, 5000, [this, prefix](int, const QString&) {
            QElapsedTimer timer;
            timer.start();
            probe_discord(10, [this, prefix, timer](int ok, int total) {
                const bool alive = is_engine_running();
                const QString mark = ok == total ? "✅" : (ok == 0 ? "❌" : "⚠️");
                st_report << QString("%1 %2 — Discord %3/%4 başarılı (%5 ms)%6").arg(mark, prefix).arg(ok).arg(total).arg(timer.elapsed())
                    .arg(alive ? "" : " [motor test sırasında kapandı]");
                st_index++;
                self_test_next();
            });
        });
    });
    });
}

// ==========================================
// 🎮 OTOMATİK OYUN MODU
// Anti-cheat kullanan bir oyun açılınca WinDivert'i kapatır (EA / EasyAntiCheat / BattlEye bu sürücüye takılıyor),
// oyun süresince Discord'u sürücüsüz Şifreli DNS ile ayakta tutmaya çalışır, oyun kapanınca eski profili geri açar.
// ==========================================
// 🎮 OYUNLARIM: kullanıcının elle eklediği oyunlar ve her oyun için davranış
// action: "off" = WinDivert'i kapat (anti-cheat'li oyunlar), "profile" = oyun boyunca başka profile geç,
//         "keep" = hiçbir şeye dokunma (ör. kalkanın açık kalmasını istediği oyunlar)
struct GameRule { QString name; QStringList exes; QString action; QString profile; };

static QList<GameRule> load_game_rules() {
    QSettings s(takanosu_settings_path(), QSettings::IniFormat);
    // İlk açılışta listeye örnek olarak Apex eklenir (kullanıcı silebilir, bir daha eklenmez)
    if (!s.value("my_games_seeded", false).toBool()) {
        s.setValue("my_games_seeded", true);
        if (s.value("my_games/size", 0).toInt() == 0) {
            s.beginWriteArray("my_games", 1);
            s.setArrayIndex(0);
            s.setValue("name", "Apex Legends");
            s.setValue("exes", QStringList{ "r5apex.exe", "r5apex_dx12.exe" });
            s.setValue("action", "off");
            s.setValue("profile", QString());
            s.endArray();
        }
    }
    QList<GameRule> rules;
    const int n = s.beginReadArray("my_games");
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        GameRule r{ s.value("name").toString(), s.value("exes").toStringList(), s.value("action", "off").toString(), s.value("profile").toString() };
        for (QString& e : r.exes) e = e.trimmed().toLower();
        r.exes.removeAll(QString());
        if (r.action != "off" && r.action != "profile" && r.action != "keep") r.action = "off";
        if (!r.exes.isEmpty()) rules << r;
    }
    s.endArray();
    return rules;
}

static void save_game_rules(const QList<GameRule>& rules) {
    QSettings s(takanosu_settings_path(), QSettings::IniFormat);
    s.remove("my_games");
    s.beginWriteArray("my_games", rules.size());
    for (int i = 0; i < rules.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name", rules[i].name);
        s.setValue("exes", rules[i].exes);
        s.setValue("action", rules[i].action);
        s.setValue("profile", rules[i].profile);
    }
    s.endArray();
    s.setValue("my_games_seeded", true);
}

// ==========================================
// 🔍 KURULU OYUNLARI BUL (Steam, Epic, Ubisoft)
// Kütüphane kayıtlarından oyun klasörleri bulunur; klasörde EAC / EA Anti-Cheat / BattlEye izi varsa "Kalkanı kapat" önerilir.
// Hiçbir şey kullanıcı onaylamadan eklenmez; "yeni oyunları otomatik ekle" seçeneği varsayılan KAPALI.
// ==========================================
struct FoundGame { QString name, dir; QStringList exes; bool anticheat = false; };

static bool junk_exe(const QString& lower) {
    static const QRegularExpression re("(unins|setup|install|redist|vcredist|dxsetup|dotnet|crash|report|helper|launcher|updater|"
                                       "easyanticheat|eaanticheat|beservice|battleye|start_protected_game|ue4prereq|uecrash|unitycrash|"
                                       "cefprocess|webhelper|overlay|benchmark|compiler|server|dedicated|editor|tool|vconsole|hammer)");
    return re.match(lower).hasMatch();
}

static QString game_key(const QString& s) {
    QString k = s.toLower();
    k.remove(QRegularExpression("[^a-z0-9]"));
    return k;
}

// Klasörü sınırlı derinlikte gezer: anti-cheat izi ve ana exe adayları
static void inspect_game_dir(FoundGame& g, const QString& preferred_exe = QString()) {
    struct Cand { QString name; qint64 size; int score; };
    QList<Cand> cands;
    const QString base = QDir::cleanPath(g.dir);
    const int base_depth = base.count('/');
    const QString k_name = game_key(g.name), k_dir = game_key(QFileInfo(base).fileName());
    // Kısaltma: "Counter-Strike 2" → cs2, "Half-Life" → hl (exe adları çoğu zaman böyle)
    QString acro;
    for (const QString& w : g.name.split(QRegularExpression("[^A-Za-z0-9]+"), Qt::SkipEmptyParts))
        acro += w.at(0).isDigit() ? w : w.left(1).toLower();
    static const QRegularExpression ac_re("(easyanticheat|eaanticheat|battleye|beservice|start_protected_game|_eac\\.exe$|_be\\.exe$)");
    QDirIterator it(base, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    int visited = 0;
    while (it.hasNext() && ++visited < 20000) {
        const QString path = it.next();
        if (path.count('/') - base_depth > 4) continue;
        const QFileInfo fi = it.fileInfo();
        const QString lower = fi.fileName().toLower();
        if (ac_re.match(lower).hasMatch()) g.anticheat = true;
        if (!fi.isFile() || !lower.endsWith(".exe") || junk_exe(lower)) continue;
        const QString stem = game_key(fi.completeBaseName());
        int score = 0;
        if (!stem.isEmpty() && (k_name.startsWith(stem) || k_dir.startsWith(stem) || stem.startsWith(k_dir) || stem.startsWith(k_name))) score += 100;
        else if (acro.size() >= 2 && stem == acro) score += 90;
        if (lower.contains("win64") || fi.path().toLower().contains("win64")) score += 5;
        cands << Cand{ lower, fi.size(), score };
    }
    if (!preferred_exe.isEmpty()) g.exes << QFileInfo(preferred_exe).fileName().toLower();
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.score != b.score ? a.score > b.score : a.size > b.size; });
    if (g.exes.isEmpty() && !cands.isEmpty() && (cands.first().score > 0 || cands.first().size > 5 * 1024 * 1024)) g.exes << cands.first().name;
    // Aynı oyunun _dx12 / _vulkan / _be / _eac gibi kardeş exe'leri de eklenir
    if (!g.exes.isEmpty()) {
        const QString stem = QFileInfo(g.exes.first()).completeBaseName();
        for (const Cand& c : cands)
            if (c.name != g.exes.first() && c.name.startsWith(stem + "_") && !g.exes.contains(c.name) && g.exes.size() < 4) g.exes << c.name;
    }
}

static QList<FoundGame> scan_installed_games() {
    QList<FoundGame> out;
    QSet<QString> seen_dirs;
    auto add = [&](FoundGame g, const QString& preferred_exe = QString()) {
        const QString key = QDir::cleanPath(g.dir).toLower();
        if (g.name.isEmpty() || seen_dirs.contains(key) || !QFileInfo(g.dir).isDir()) return;
        static const QRegularExpression skip("(redistributable|steamworks|proton|steam linux runtime|steamvr|wallpaper engine|soundtrack|dedicated server)", QRegularExpression::CaseInsensitiveOption);
        if (skip.match(g.name).hasMatch()) return;
        seen_dirs.insert(key);
        inspect_game_dir(g, preferred_exe);
        if (!g.exes.isEmpty()) out << g;
    };

    // Steam: libraryfolders.vdf → her kütüphanede appmanifest_*.acf (name, installdir)
    const QString steam = QSettings("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat).value("SteamPath").toString();
    if (!steam.isEmpty()) {
        QStringList libs{ QDir::cleanPath(steam) };
        QFile vdf(steam + "/steamapps/libraryfolders.vdf");
        if (vdf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            static const QRegularExpression path_re("\"path\"\\s+\"([^\"]+)\"");
            auto m = path_re.globalMatch(QString::fromUtf8(vdf.readAll()));
            while (m.hasNext()) { const QString l = QDir::cleanPath(m.next().captured(1).replace("\\\\", "\\")); if (!libs.contains(l, Qt::CaseInsensitive)) libs << l; }
        }
        static const QRegularExpression name_re("\"name\"\\s+\"([^\"]+)\""), dir_re("\"installdir\"\\s+\"([^\"]+)\"");
        for (const QString& lib : libs) {
            const QDir apps(lib + "/steamapps");
            for (const QString& acf : apps.entryList({ "appmanifest_*.acf" }, QDir::Files)) {
                QFile f(apps.filePath(acf));
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                const QString text = QString::fromUtf8(f.readAll());
                const QString name = name_re.match(text).captured(1), dir = dir_re.match(text).captured(1);
                if (!name.isEmpty() && !dir.isEmpty()) add(FoundGame{ name, lib + "/steamapps/common/" + dir });
            }
        }
    }
    // Epic: ProgramData\Epic\EpicGamesLauncher\Data\Manifests\*.item (JSON)
    const QDir epic("C:/ProgramData/Epic/EpicGamesLauncher/Data/Manifests");
    for (const QString& item : epic.entryList({ "*.item" }, QDir::Files)) {
        QFile f(epic.filePath(item));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        add(FoundGame{ o.value("DisplayName").toString(), QDir::fromNativeSeparators(o.value("InstallLocation").toString()) }, o.value("LaunchExecutable").toString());
    }
    // Ubisoft Connect: HKLM\SOFTWARE\WOW6432Node\Ubisoft\Launcher\Installs\<id>\InstallDir
    QSettings ubi("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Ubisoft\\Launcher\\Installs", QSettings::NativeFormat);
    for (const QString& id : ubi.childGroups()) {
        const QString dir = QDir::cleanPath(QDir::fromNativeSeparators(ubi.value(id + "/InstallDir").toString()));
        if (!dir.isEmpty()) add(FoundGame{ QFileInfo(dir).fileName(), dir });
    }
    std::sort(out.begin(), out.end(), [](const FoundGame& a, const FoundGame& b) { return a.anticheat != b.anticheat ? a.anticheat : a.name.toLower() < b.name.toLower(); });
    return out;
}

// Tanı için: "--scan-games <dosya>" bulunan oyunları dosyaya yazar (arayüz açılmaz)
QString takanosu_scan_games_report() {
    QStringList lines;
    for (const FoundGame& g : scan_installed_games())
        lines << QString("%1 | %2 | %3 | %4").arg(g.anticheat ? "AC" : "--", g.name, g.exes.join(","), QDir::toNativeSeparators(g.dir));
    return lines.join('\n');
}

// Oyunlarım'da zaten olan (exe'lerinden biri eşleşen) oyunlar listede gösterilmez
static QList<FoundGame> new_found_games(const QList<FoundGame>& found, const QList<GameRule>& rules) {
    QSet<QString> known;
    for (const GameRule& r : rules) for (const QString& e : r.exes) known.insert(e);
    QList<FoundGame> out;
    for (const FoundGame& g : found) {
        bool dup = false;
        for (const QString& e : g.exes) if (known.contains(e)) dup = true;
        if (!dup) out << g;
    }
    return out;
}

static QString game_action_text(const GameRule& r) {
    if (r.action == "keep") return "Dokunma";
    if (r.action == "profile") {
        const DpiProfile* p = find_profile(r.profile);
        return "Profil: " + (p ? p->name : QString("(bulunamadı)"));
    }
    return "Kalkanı kapat";
}

static QSet<QString> running_processes() {
    QSet<QString> procs;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return procs;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do { procs.insert(QString::fromWCharArray(entry.szExeFile).toLower()); } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return procs;
}

static QString find_anticheat_game(const QSet<QString>& procs) {
    // Sadece oyun çalışırken açık olan süreçler.
    // Valorant (Vanguard) bilerek YOK: test edildi, Vanguard WinDivert'i engellemiyor; kalkan oyunda açık kalabilir.
    static const QMap<QString, QString> games = {
        // Oyunun anti-cheat başlatıcıları: sürücü kontrolü oyun exe'sinden ÖNCE burada yapılıyor, o yüzden en önemlileri bunlar
        { "eaanticheat.gameservicelauncher.exe", "Apex Legends / EA oyunu" }, { "rocketleague_eac.exe", "Rocket League" },
        { "rainbowsix_be.exe", "Rainbow Six Siege" }, { "sen_launcher.exe", "Rainbow Six Siege" }, { "start_protected_game.exe", "Easy Anti-Cheat korumalı oyun" },
        { "r5apex.exe", "Apex Legends" }, { "r5apex_dx12.exe", "Apex Legends" },
        { "fortnitebootstrapper.exe", "Fortnite" }, { "fortnitelauncher.exe", "Fortnite" }, { "fortniteclient-win64-shipping_eac_eos.exe", "Fortnite" },
        { "fortniteclient-win64-shipping_be.exe", "Fortnite" }, { "fortniteclient-win64-shipping.exe", "Fortnite" },
        { "rainbowsix.exe", "Rainbow Six Siege" }, { "rainbowsix_vulkan.exe", "Rainbow Six Siege" },
        { "rocketleague.exe", "Rocket League" },
        { "huntgame.exe", "Hunt: Showdown" },
        { "eaanticheat.gameservice.exe", "EA Anti-Cheat korumalı oyun" },
        { "easyanticheat.exe", "Easy Anti-Cheat korumalı oyun" }, { "easyanticheat_eos.exe", "Easy Anti-Cheat korumalı oyun" },
        { "beservice.exe", "BattlEye korumalı oyun" }, { "beservice_x64.exe", "BattlEye korumalı oyun" },
    };

    QString found;
    const bool faceit_running = procs.contains("faceitclient.exe") || procs.contains("faceitservice.exe");
    const bool cs2_running = procs.contains("cs2.exe");
    for (const QString& exe : procs) {
        // Belirli oyun adı, genel anti-cheat servisinden önceliklidir (bildirimde "Apex Legends" yazsın)
        if (games.contains(exe) && (found.isEmpty() || (!exe.contains("anticheat") && !exe.startsWith("beservice")))) {
            found = games.value(exe);
        }
    }

    // FACEIT istemcisi çoğu kişide sürekli tepside açık; sadece ona bakarsak kalkan hiç açılmaz.
    // FACEIT AC'nin sürücü denetimi CS2 açıldığında yapıldığı için ikisi birlikte çalışıyorsa Oyun Modu'na geç.
    // (Normal CS2 / VAC WinDivert'e takılmaz.)
    if (found.isEmpty() && faceit_running && cs2_running) found = "CS2 (FACEIT AC)";
    return found;
}

void PowerToysDashboard::game_check() {
    if (is_self_test() || st_in_app) return;
    const QSet<QString> procs = running_processes();

    // Önce kullanıcının Oyunlarım listesi (kendi seçtiği davranışla), sonra hazır anti-cheat listesi
    QString game, action = "off", profile;
    for (const GameRule& r : load_game_rules()) {
        for (const QString& exe : r.exes) {
            if (procs.contains(exe)) { game = r.name.isEmpty() ? exe : r.name; action = r.action; profile = r.profile; break; }
        }
        if (!game.isEmpty()) break;
    }
    const bool enabled = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("auto_game_mode", true).toBool();
    if (game.isEmpty() && enabled) game = find_anticheat_game(procs);

    if (!game.isEmpty() && !game_running) on_game_started(game, action, profile);
    else if (game.isEmpty() && game_running) on_game_stopped();
}

void PowerToysDashboard::on_game_started(const QString& game, const QString& action, const QString& profile_id) {
    game_running = true;
    game_name = game;
    game_action = action;

    bool engine_active = false;
    for (QProcess* p : engine_procs) {
        if (p->state() == QProcess::Running) engine_active = true;
    }

    log_console->appendPlainText("");
    if (action == "keep") {
        log("OYUN MODU", QString("%1 açıldı. Oyunlarım'da \"Dokunma\" seçili; kalkan olduğu gibi kalıyor.").arg(game));
        return;
    }
    if (action == "profile") {
        const DpiProfile* target = find_profile(profile_id);
        if (!target) {
            log("OYUN MODU", QString("%1 açıldı ama seçili profil bulunamadı; dokunulmadı.").arg(game));
            return;
        }
        if (!is_engine_running() || is_busy) {
            log("OYUN MODU", QString("%1 açıldı. Kalkan şu an kapalı/meşgul olduğu için profil değiştirilmedi.").arg(game));
            return;
        }
        if (target->id == active_profile_id) {
            log("OYUN MODU", QString("%1 açıldı. Zaten %2 kullanılıyor.").arg(game, target->name));
            return;
        }
        // Eski profili oyun bitince geri açmak için sakla. Kayıtlı profil (last_profile_id) değişmez.
        game_paused_profile = active_profile_id;
        const DpiProfile chosen = *target;
        log("OYUN MODU", QString("%1 açıldı! Oyun boyunca profil: %2").arg(game, chosen.name));
        begin_profile(chosen, [this, game, chosen](bool ok, const QString& error) {
            if (!game_running) return; // oyun bu arada kapandıysa on_game_stopped eski profili açar
            if (!ok) {
                log("OYUN MODU", "Oyun profili açılamadı: " + error + " Eski profile dönülüyor.");
                const DpiProfile* old = find_profile(game_paused_profile);
                if (old) {
                    const DpiProfile back = *old;
                    begin_profile(back, [this, back](bool ok2, const QString&) {
                        if (ok2) { active_profile_id = back.id; emit engine_state_changed(true, back.name); }
                    });
                }
                return;
            }
            active_profile_id = chosen.id;
            log("OYUN MODU", QString("%1 açık. Oyun profili çalışıyor.").arg(chosen.name));
            set_progress(100, "#00E676");
            set_ui_state(UiState::Success, QString("🎮 %1 açık: oyun boyunca %2 kullanılıyor. Oyun kapanınca eski profile dönülür.").arg(game, chosen.name), "Aktif Mod: " + chosen.name);
            emit engine_state_changed(true, chosen.name);
            emit notify("🎮 Oyun Modu", QString("%1 açıldı. Oyun boyunca %2 kullanılıyor.").arg(game, chosen.name));
        });
        return;
    }
    if (!engine_active) {
        log("OYUN MODU", QString("%1 algılandı. WinDivert kullanılmıyor, müdahale gerekmiyor.").arg(game));
        return;
    }

    // Devam eden tarama varsa iptal et; duraklatılan profili oyun bitince geri açacağız
    game_paused_profile = !active_profile_id.isEmpty() ? active_profile_id : QSettings(takanosu_settings_path(), QSettings::IniFormat).value("last_profile_id").toString();
    ++scan_generation;
    is_busy = false;
    restoring = false;
    single_try = false;

    stop_engine(true);
    log("OYUN MODU", QString("%1 algılandı! Anti-cheat sorun çıkarmasın diye WinDivert sürücüsü kapatıldı.").arg(game));
    set_progress(0, "#8a2be2");
    set_ui_state(UiState::Stopped, QString("🎮 %1 açık: WinDivert geçici olarak kapatıldı. Oyun kapanınca kalkan otomatik geri açılır.").arg(game), "Aktif Mod: OYUN MODU");
    set_start_button(true, "OTONOM SİSTEMİ BAŞLAT");
    emit engine_state_changed(false, QString());

    const QStringList disabled = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("disabled_profiles").toStringList();
    if (!dns_mode_supported() || takanosu_dns_mode_active() || disabled.contains("dns_doh")) {
        emit notify("🎮 Oyun Modu", QString("%1 algılandı. WinDivert kapatıldı, oyun kapanınca kalkan geri açılacak.").arg(game));
        return;
    }

    // Oyun sırasında Discord kopmasın: sürücüsüz Şifreli DNS'i geçici olarak dene
    log("OYUN MODU", "Oyun sırasında Discord için sürücüsüz Şifreli DNS deneniyor...");
    apply_dns_mode([this, game](bool ok, const QString& error) {
        if (!game_running) { if (ok) revert_dns_mode(); return; } // oyun bu arada kapandıysa
        if (!ok) {
            log("OYUN MODU", "Şifreli DNS açılamadı: " + error);
            emit notify("🎮 Oyun Modu", QString("%1 algılandı. WinDivert kapatıldı; oyun sırasında Discord bağlantısı kopabilir.").arg(game));
            return;
        }
        game_dns_temp = true;
        probe_discord(5, [this, game](int ok, int total) {
            if (!game_running) return;
            log("OYUN MODU", QString("Şifreli DNS ile Discord bağlantı denemesi: %1/%2 başarılı.").arg(ok).arg(total));
            if (ok == total) {
                log("OYUN MODU", "Discord oyun boyunca Şifreli DNS ile çalışmaya devam ediyor. İyi oyunlar Kaptan!");
                emit notify("🎮 Oyun Modu", QString("%1 algılandı. Discord, sürücüsüz Şifreli DNS ile çalışmaya devam ediyor.").arg(game));
            }
            else if (ok > 0) {
                log("OYUN MODU", "ISS DNS'in yanında aralıklı DPI engeli de uyguluyor: Discord birkaç denemede bağlanabilir, bağlandıktan sonra genelde kopmaz.");
                emit notify("🎮 Oyun Modu", QString("%1 algılandı. Discord oyun sırasında gecikmeli bağlanabilir (ISS kısmi engel uyguluyor).").arg(game));
            }
            else {
                log("OYUN MODU", "Bu ağda Discord oyun sırasında erişilemeyebilir (ISS, DNS dışında DPI engeli uyguluyor).");
                emit notify("🎮 Oyun Modu", QString("%1 algılandı. WinDivert kapatıldı; bu ağda oyun sırasında Discord kopabilir.").arg(game));
            }
        });
    });
}

void PowerToysDashboard::on_game_stopped() {
    game_running = false;
    log_console->appendPlainText("");
    log("OYUN MODU", QString("%1 kapandı.").arg(game_name));
    const bool was_profile_switch = (game_action == "profile");
    game_action.clear();

    const DpiProfile* profile = find_profile(game_paused_profile);
    game_paused_profile.clear();
    if (game_dns_temp) {
        game_dns_temp = false;
        if (!profile) revert_dns_mode(); // profil varsa begin_profile zaten geri alır
    }
    if (!profile || is_busy) return;

    // Duraklatılan profili doğrulayarak geri aç (çalışmazsa tam tarama yapılır)
    log("OYUN MODU", QString(was_profile_switch ? "Önceki profile dönülüyor: %1" : "DPI kalkanı geri açılıyor: %1").arg(profile->name));
    emit notify("🎮 Oyun Modu", was_profile_switch ? QString("Oyun kapandı, %1 profiline dönülüyor.").arg(profile->name) : QString("Oyun kapandı, DPI kalkanı geri açılıyor."));
    is_busy = true;
    restoring = true;
    single_try = false;
    ++scan_generation;
    set_start_button(false, "SİSTEM ÇALIŞIYOR...");
    candidates = { *profile };
    current_auto_test_index = 0;
    run_auto_test_step();
}

// Kullanıcı elle başlattığında / durdurduğunda oyun moduna ait bekleyen işleri unut
void PowerToysDashboard::reset_game_state() {
    game_paused_profile.clear();
    game_dns_temp = false;
}

// ==========================================
// 🛰️ NÖBETÇİ: AĞ DEĞİŞİNCE VE PERİYODİK BAĞLANTI KONTROLÜ
// Aktif profil artık çalışmıyorsa (ağ/ISS değişti, laptop başka Wi-Fi'ye bağlandı) kendiliğinden yeni profil arar.
// ==========================================
void PowerToysDashboard::health_check() {
    if (is_busy || game_running || st_in_app || active_profile_id.isEmpty() || is_self_test()) return;
    const int gen = scan_generation;

    check_internet([this, gen](bool online) {
        if (gen != scan_generation || is_busy || active_profile_id.isEmpty()) return;
        if (!online) return; // internet tamamen yoksa profil suçlu değil; bağlantı gelince tekrar bakılır

        test_dpi_bypass([this, gen](bool passed, const QString& detail) {
            if (gen != scan_generation || is_busy || active_profile_id.isEmpty()) return;
            if (passed) { health_fail = 0; return; }
            if (++health_fail < 2) {
                QTimer::singleShot(30000, this, &PowerToysDashboard::health_check); // tek seferlik takılma olabilir, 30 sn sonra tekrar
                return;
            }
            health_fail = 0;
            log_console->appendPlainText("");
            log("NÖBETÇİ", QString("Aktif profil artık Discord'a ulaşamıyor (%1). Otomatik yeniden tarama başlatılıyor...").arg(detail));
            emit notify("The Takanosu Elite", "Bağlantı bozuldu, otomatik olarak yeni profil aranıyor...");
            start_autonomous_system();
        });
    });
}

// ==========================================
// 🩺 TEK TIKLA TANI RAPORU
// ==========================================
void PowerToysDashboard::util_diagnostic_report() {
    if (is_busy) {
        log("UYARI", "Devam eden bir işlem var. Bitmesini bekleyin veya 'Oyun Modu' ile durdurun.");
        return;
    }

    QMessageBox confirm(this);
    confirm.setWindowTitle("Tanı Raporu");
    confirm.setText("🩺 Tanı raporu hazırlansın mı?");
    confirm.setInformativeText("Tüm profiller tek tek denenir (yaklaşık 2 dakika). Bu sürede internetiniz birkaç kez kısa süreli kopabilir.\n\n"
                               "Rapor kişisel veri içermez (IP adresiniz yazılmaz); sadece ISS adı, Windows sürümü ve test sonuçları bulunur. "
                               "Bitince rapor panoya kopyalanır.");
    QPushButton* btn_yes = confirm.addButton("Evet, Başlat", QMessageBox::YesRole);
    confirm.addButton("Vazgeç", QMessageBox::NoRole);
    confirm.setStyleSheet(
        "QMessageBox { background-color: #1a1a1a; } QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
        "QPushButton { background-color: #333333; color: white; border-radius: 8px; padding: 8px 18px; font-family: 'Segoe UI Variable'; font-weight: bold; border: 1px solid #555555; min-width: 110px; } "
        "QPushButton:hover { background-color: #444444; }");
    btn_yes->setStyleSheet("background-color: #6a1b9a; color: white; border: none;");
    confirm.exec();
    if (confirm.clickedButton() != btn_yes) return;

    reset_game_state();
    st_in_app = true;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    set_start_button(false, "TANI TESTİ SÜRÜYOR...");
    set_progress(5, "#ff9800");
    set_ui_state(UiState::Busy, "🩺 Tanı raporu hazırlanıyor...", "Aktif Mod: TANI TESTİ");
    log_console->appendPlainText("");
    log("TANI", "Tanı testi başladı. Tüm profiller sırayla deneniyor...");
    emit engine_state_changed(false, QString());
    // Önce IPv6 DNS ölçülür (profil testleri interneti kesip açtığı için ondan önce); sonuç raporun sonuna eklenir
    if (takanosu_dns_mode_active()) { st_ipv6_lines = QStringList{ "Şifreli DNS modu açıktı, ölçülmedi." }; run_self_test(dir + "/tani_raporu.txt"); return; }
    log("TANI", "IPv6 DNS ölçülüyor...");
    run_async(powershell_exe(), powershell_args(IPV6_DNS_CHECK_SCRIPT), 90000, [this, dir](int, const QString& out) {
        st_ipv6_lines.clear();
        for (const Ipv6DnsIssue& r : parse_ipv6_dns_check(out)) {
            st_ipv6_lines << r.lines;
            if (r.bad) st_ipv6_lines << QString("   → %1: IPv6 DNS yavaş/cevapsız (Araçlar → IPv6 DNS Kontrolü ile düzeltilebilir)").arg(r.alias);
        }
        if (st_ipv6_lines.isEmpty()) st_ipv6_lines << "Ölçülemedi.";
        run_self_test(dir + "/tani_raporu.txt");
    });
}

void PowerToysDashboard::finish_diagnostic_report() {
    st_in_app = false;
    is_busy = false;
    if (!st_ipv6_lines.isEmpty()) st_report << "" << "── IPv6 DNS ──" << st_ipv6_lines;
    const QString report = st_report.join('\n');

    QGuiApplication::clipboard()->setText(report);
    set_progress(100, "#00E676");
    set_ui_state(UiState::Idle, "🩺 Tanı raporu hazır ve panoya kopyalandı.", "Aktif Mod: BEKLENİYOR...");
    set_start_button(true, "OTONOM SİSTEMİ BAŞLAT");
    log("TANI", "Rapor hazır: " + QDir::toNativeSeparators(st_path));

    QMessageBox box(this);
    box.setWindowTitle("Tanı Raporu");
    box.setText("✅ Tanı raporu hazır ve panoya kopyalandı!");
    box.setInformativeText("GitHub tartışma sayfasına yapıştırarak (Ctrl+V) bizimle paylaşabilirsiniz. Böylece sizin ağınız için en iyi profili ekleyebiliriz.\n\nKaydedildi: " + QDir::toNativeSeparators(st_path));
    QPushButton* btn_share = box.addButton("GitHub'da Paylaş", QMessageBox::YesRole);
    box.addButton("Tamam", QMessageBox::NoRole);
    box.setStyleSheet(
        "QMessageBox { background-color: #1a1a1a; } QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
        "QPushButton { background-color: #333333; color: white; border-radius: 8px; padding: 8px 18px; font-family: 'Segoe UI Variable'; font-weight: bold; border: 1px solid #555555; min-width: 110px; } "
        "QPushButton:hover { background-color: #444444; }");
    btn_share->setStyleSheet("background-color: #6a1b9a; color: white; border: none;");
    box.exec();
    if (box.clickedButton() == btn_share) {
        open_url_unelevated("https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/discussions");
    }

    // Kalkanı test öncesindeki profille geri aç
    restore_last_session();
}

void PowerToysDashboard::update_theme(bool is_dark) {
    is_dark_mode = is_dark;

    if (is_dark) {
        block_quick->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        block_utils->setStyleSheet("QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 8px; border: 1px solid #3a3a3a; }");
        title_quick->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        title_utils->setStyleSheet("color: white; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;");
        lbl_iss->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 13px; margin-top: 15px; border: none; background: transparent;");

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

        for (auto btn : utility_btns) {
            btn->setStyleSheet("QPushButton { background-color: #f5f5f5; border-radius: 6px; color: #333333; text-align: left; padding-left: 15px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: 1px solid #dddddd; } QPushButton:hover { background-color: #e8e8e8; border: 1px solid #cccccc; } QPushButton:pressed { background-color: #d5d5d5; }");
        }
    }

    apply_combo_style(combo_iss);

    // Durum yazılarını yeni temanın renkleriyle yeniden boya
    set_ui_state(ui_state, lbl_status->text(), lbl_active_mod->text());
}

// İki sütun yan yana sığmıyorsa alt alta diz. Karar panelin kendi genişliğine göre DEĞİL, kaydırma alanının
// görünür genişliğine göre verilir: panel, kutuların asgari genişliği yüzünden görünür alandan daha dar olamadığı
// için eski yöntemde eşik hiç aşılmıyor ve dar pencerede sağ sütun kesiliyordu.
void PowerToysDashboard::update_column_layout() {
    const int visible = parentWidget() ? parentWidget()->width() : width();
    const QBoxLayout::Direction wanted = visible < 680 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    if (flex_layout->direction() != wanted) flex_layout->setDirection(wanted);
}

bool PowerToysDashboard::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parentWidget() && event->type() == QEvent::Resize) update_column_layout();
    return QWidget::eventFilter(watched, event);
}

void PowerToysDashboard::resizeEvent(QResizeEvent* event) {
    update_column_layout();
    QWidget::resizeEvent(event);
}

TheTakanosu_Elite::TheTakanosu_Elite(QWidget* parent)
    : QMainWindow(parent), m_is_overlay_open(false), m_is_manually_collapsed(false)
{
    this->setWindowTitle("The Takanosu Elite");

    QString app_dir = QCoreApplication::applicationDirPath();
    this->setWindowIcon(QIcon(app_dir + "/assets/icon-transparant.png"));

    // 🚀 BUM! İLK KURULUM (FRESH INSTALL) ZIRHI!
    // İlk açılışta başlangıç görevini oluşturur; eski sürümün "Run" kaydını Görev Zamanlayıcı'ya taşır.
    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    if (!is_self_test()) migrate_legacy_startup();

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

    // Yönetici olmayan bir kopyadan gelen "pencereyi göster" sinyaline izin ver (UIPI engellemesin)
    ChangeWindowMessageFilterEx(hwnd, RegisterWindowMessageW(L"TheTakanosu_Elite_Show"), MSGFLT_ALLOW, nullptr);

    setupUi();
    setup_tray_icon();

    bool is_dark = settings.value("is_dark_theme", true).toBool();
    set_theme(is_dark);
    if (!is_self_test()) QTimer::singleShot(1000, this, [this]() { check_updates(false); });
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
    connect(dashboard_widget, &PowerToysDashboard::engine_state_changed, this, &TheTakanosu_Elite::on_engine_state_changed);
    connect(dashboard_widget, &PowerToysDashboard::notify, this, [this](const QString& title, const QString& text) {
        if (tray_icon && tray_icon->isVisible()) tray_icon->showMessage(title, text, QSystemTrayIcon::Information, 6000);
    });
    stacked_widget->addWidget(create_powertoys_dashboard());
    stacked_widget->addWidget(create_dpi_modes_page());
    stacked_widget->addWidget(create_mod_settings_page());
    stacked_widget->addWidget(create_stats_page());
    stacked_widget->addWidget(create_tools_page());
    stacked_widget->addWidget(create_tunnel_page());

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
    add_sidebar_btn(QString::fromUtf8("🧬"), "DPI Modları", 1);
    add_sidebar_btn(QString::fromUtf8("⚙️"), "Ayarlar", 2);
    add_sidebar_btn(QString::fromUtf8("📊"), "İstatistikler", 3);
    add_sidebar_btn(QString::fromUtf8("🛠️"), "Araçlar", 4);
    add_sidebar_btn(QString::fromUtf8("🛡️"), "Tünel", 5);
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
    act_toggle_engine = new QAction("🚀 DPI Kalkanını Başlat", this);
    QAction* act_update = new QAction("Güncellemeleri Denetle...", this);
    QAction* act_github = new QAction("GitHub Repository", this);
    QAction* act_hide = new QAction("Tepsiyi Gizle (Hide System Tray)", this);
    QAction* act_quit = new QAction("GoodbyeDPI Elite'den Çık", this);

    connect(act_open, &QAction::triggered, this, [this]() { this->showNormal(); this->raise(); this->activateWindow(); });
    connect(act_toggle_engine, &QAction::triggered, this, [this]() {
        if (dashboard_widget->is_engine_running()) dashboard_widget->kill_dpi_service();
        else dashboard_widget->start_autonomous_system();
    });
    connect(act_update, &QAction::triggered, this, [this]() { check_updates(true); });
    connect(act_github, &QAction::triggered, []() { open_url_unelevated("https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition"); });
    connect(act_hide, &QAction::triggered, [this]() { tray_icon->hide(); });
    connect(act_quit, &QAction::triggered, [this]() { tray_icon->hide(); qApp->quit(); });

    tray_menu->addAction(act_open);
    tray_menu->addAction(act_toggle_engine);
    tray_menu->addSeparator();
    tray_menu->addAction(act_update);
    tray_menu->addAction(act_github);
    tray_menu->addSeparator();
    tray_menu->addAction(act_hide);
    tray_menu->addAction(act_quit);

    tray_icon->setContextMenu(tray_menu);
    tray_icon->setToolTip("The Takanosu Elite — Beklemede");
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
    scroll_area->viewport()->installEventFilter(dashboard_widget); // görünür alan daralınca sütunları alt alta diz
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

    mod_settings_header = new QLabel("⚙️ Ayarlar");
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
    bool is_startup = startup_task_exists();

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

    // 🎮 Otomatik Oyun Modu (anti-cheat'li oyun açılınca WinDivert'i kapatır, kapanınca geri açar)
    const bool game_on = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("auto_game_mode", true).toBool();
    btn_toggle_game = new QPushButton(game_on ? "✔️ Otomatik Oyun Modu (AÇIK)" : "❌ Otomatik Oyun Modu (KAPALI)");
    btn_toggle_game->setCheckable(true);
    btn_toggle_game->setChecked(game_on);
    btn_toggle_game->setFixedHeight(45);
    btn_toggle_game->setCursor(Qt::PointingHandCursor);
    btn_toggle_game->setToolTip("Apex, Fortnite, Rainbow Six, Rocket League gibi anti-cheat kullanan oyunlar açılınca WinDivert'i otomatik kapatır.");
    connect(btn_toggle_game, &QPushButton::clicked, this, [this]() {
        const bool on = btn_toggle_game->isChecked();
        QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("auto_game_mode", on);
        btn_toggle_game->setText(on ? "✔️ Otomatik Oyun Modu (AÇIK)" : "❌ Otomatik Oyun Modu (KAPALI)");
        set_theme(QSettings(takanosu_settings_path(), QSettings::IniFormat).value("is_dark_theme", true).toBool());
    });
    sys_layout->addWidget(btn_toggle_game);

    layout->addWidget(sys_block);

    // 🎮 OYUNLARIM: Windows'taki grafik/performans ayarı gibi, oyunu exe'sinden elle ekle ve davranışını seç
    games_block = new QFrame();
    games_block->setObjectName("TargetBlock");
    games_block->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout* games_layout = new QVBoxLayout(games_block);
    games_layout->setContentsMargins(20, 20, 20, 20);
    games_layout->setSpacing(12);
    games_title = new QLabel("🎮 Oyunlarım");
    games_layout->addWidget(games_title);
    games_desc = new QLabel("Oyunu .exe dosyasından ekle, açılınca ne olacağını seç: anti-cheat'li oyunlarda kalkanı kapat, "
                            "diğerlerinde oyun boyunca başka bir profile geç (ör. CS2 açılınca Joker -5) ya da hiçbir şeye dokunma. "
                            "Oyun kapanınca eski profile dönülür. Buradaki ayar hazır anti-cheat listesinden önce gelir.");
    games_desc->setWordWrap(true);
    games_layout->addWidget(games_desc);
    games_list = new QListWidget();
    games_list->setFixedHeight(150);
    games_list->setCursor(Qt::PointingHandCursor);
    connect(games_list, &QListWidget::itemDoubleClicked, this, [this]() { edit_game_rule(games_list->currentRow()); });
    games_layout->addWidget(games_list);

    QHBoxLayout* games_btn_row = new QHBoxLayout();
    games_btn_row->setSpacing(10);
    QPushButton* btn_game_add = new QPushButton("➕ Oyun Ekle");
    QPushButton* btn_game_edit = new QPushButton("✏️ Düzenle");
    QPushButton* btn_game_del = new QPushButton("🗑️ Kaldır");
    QPushButton* btn_game_scan = new QPushButton("🔍 Oyunlarımı Bul");
    btn_game_scan->setToolTip("Steam, Epic ve Ubisoft kütüphanelerindeki kurulu oyunları bulur; anti-cheat'li olanlar için \"Kalkanı kapat\" önerir.");
    games_btns = { btn_game_add, btn_game_scan, btn_game_edit, btn_game_del };
    for (QPushButton* b : games_btns) { b->setFixedHeight(38); b->setCursor(Qt::PointingHandCursor); games_btn_row->addWidget(b); }
    connect(btn_game_add, &QPushButton::clicked, this, [this]() { edit_game_rule(-1); });
    connect(btn_game_scan, &QPushButton::clicked, this, [this]() { scan_games_dialog(); });
    connect(btn_game_edit, &QPushButton::clicked, this, [this]() { if (games_list->currentRow() >= 0) edit_game_rule(games_list->currentRow()); });
    connect(btn_game_del, &QPushButton::clicked, this, [this]() {
        const int row = games_list->currentRow();
        QList<GameRule> rules = load_game_rules();
        if (row < 0 || row >= rules.size()) return;
        rules.removeAt(row);
        save_game_rules(rules);
        reload_games_list();
    });
    games_layout->addLayout(games_btn_row);
    const bool autoscan = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("games_autoscan", false).toBool();
    btn_games_autoscan = new QPushButton(autoscan ? "✔️ Yeni kurulan anti-cheat'li oyunları otomatik ekle (AÇIK)" : "❌ Yeni kurulan anti-cheat'li oyunları otomatik ekle (KAPALI)");
    btn_games_autoscan->setCheckable(true);
    btn_games_autoscan->setChecked(autoscan);
    btn_games_autoscan->setFixedHeight(38);
    btn_games_autoscan->setCursor(Qt::PointingHandCursor);
    btn_games_autoscan->setToolTip("Açıksa Elite her açılışta kütüphanelere bakar; yeni kurulan anti-cheat'li oyunları \"Kalkanı kapat\" ile Oyunlarım'a ekler. Kaldırdığın oyun tekrar eklenmez.");
    connect(btn_games_autoscan, &QPushButton::clicked, this, [this]() {
        const bool on = btn_games_autoscan->isChecked();
        QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("games_autoscan", on);
        btn_games_autoscan->setText(on ? "✔️ Yeni kurulan anti-cheat'li oyunları otomatik ekle (AÇIK)" : "❌ Yeni kurulan anti-cheat'li oyunları otomatik ekle (KAPALI)");
        if (on) auto_add_games();
    });
    games_btns << btn_games_autoscan;
    games_layout->addWidget(btn_games_autoscan);
    layout->addWidget(games_block);
    reload_games_list();
    // Açılışta (seçenek açıksa) yeni oyunlar sessizce eklenir; tarama arayüzü bekletmesin diye biraz sonra
    if (autoscan && !is_self_test()) QTimer::singleShot(20000, this, [this]() { auto_add_games(); });

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

// ==========================================
// 🧬 DPI MODLARI SAYFASI
// Motor seçimi + tüm profiller: otonom taramaya dahil et/çıkar, tek tek "Şimdi Dene"
// ==========================================
QWidget* TheTakanosu_Elite::create_dpi_modes_page() {
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

    modes_header = new QLabel("🧬 DPI Modları");
    layout->addWidget(modes_header);

    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    const QStringList disabled = settings.value("disabled_profiles").toStringList();
    const bool has_zapret = zapret_installed();

    // ── Motor seçimi ──
    QFrame* engine_block = new QFrame();
    engine_block->setObjectName("TargetBlock");
    modes_blocks << engine_block;
    QVBoxLayout* engine_layout = new QVBoxLayout(engine_block);
    engine_layout->setContentsMargins(20, 20, 20, 20);
    engine_layout->setSpacing(12);

    QLabel* engine_title = new QLabel("⚙️ DPI Motoru Seçimi");
    modes_titles << engine_title;
    engine_layout->addWidget(engine_title);

    QLabel* engine_desc = new QLabel("Otonom tarama hangi yöntemleri denesin? 'Otomatik' önce ISS'nize en uygun GoodbyeDPI profillerini, sonra Zapret'i, en son sürücüsüz Şifreli DNS'i dener; Discord'a üst üste 6 kez sorunsuz bağlanan ilk yöntemde durur.");
    engine_desc->setWordWrap(true);
    modes_descs << engine_desc;
    engine_layout->addWidget(engine_desc);

    combo_engine_mode = new QComboBox();
    combo_engine_mode->addItems({ "Otomatik (GoodbyeDPI + Zapret + Şifreli DNS)", "Sadece GoodbyeDPI", "Sadece Zapret" });
    combo_engine_mode->setFixedHeight(38);
    combo_engine_mode->setCursor(Qt::PointingHandCursor);
    QListView* engine_view = new QListView();
    engine_view->setCursor(Qt::PointingHandCursor);
    combo_engine_mode->setView(engine_view);
    combo_engine_mode->setItemDelegate(new QStyledItemDelegate());
    const int saved_mode = settings.value("engine_mode", settings.value("last_engine_index", 0)).toInt();
    combo_engine_mode->setCurrentIndex(qBound(0, saved_mode, 2));
    connect(combo_engine_mode, &QComboBox::currentIndexChanged, this, [](int index) {
        QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("engine_mode", index);
    });
    engine_layout->addWidget(combo_engine_mode);
    layout->addWidget(engine_block);

    // ── Profil grupları ──
    auto add_group = [&](const QString& title, const QString& desc, int kind) { // 0 = DNS, 1 = GoodbyeDPI, 2 = Zapret
        QFrame* block = new QFrame();
        block->setObjectName("TargetBlock");
        modes_blocks << block;
        QVBoxLayout* block_layout = new QVBoxLayout(block);
        block_layout->setContentsMargins(20, 20, 20, 20);
        block_layout->setSpacing(6);

        QLabel* group_title = new QLabel(title);
        modes_titles << group_title;
        block_layout->addWidget(group_title);

        QLabel* group_desc = new QLabel(desc);
        group_desc->setWordWrap(true);
        modes_descs << group_desc;
        block_layout->addWidget(group_desc);
        block_layout->addSpacing(8);

        for (const DpiProfile& p : dpi_profiles()) {
            if ((p.is_dns ? 0 : (p.is_zapret ? 2 : 1)) != kind) continue;

            QWidget* row = new QWidget();
            QHBoxLayout* row_layout = new QHBoxLayout(row);
            row_layout->setContentsMargins(0, 6, 0, 6);
            row_layout->setSpacing(12);

            QVBoxLayout* text_col = new QVBoxLayout();
            text_col->setSpacing(2);

            QCheckBox* check = new QCheckBox(p.name);
            check->setChecked(!disabled.contains(p.id));
            check->setProperty("profile_id", p.id);
            check->setCursor(Qt::PointingHandCursor);
            check->setToolTip("İşaretliyse otonom taramada denenir");
            connect(check, &QCheckBox::toggled, this, &TheTakanosu_Elite::save_disabled_profiles);
            mode_checks << check;
            text_col->addWidget(check);

            QLabel* profile_desc = new QLabel(p.desc);
            profile_desc->setWordWrap(true);
            profile_desc->setProperty("indent", true);
            modes_descs << profile_desc;
            text_col->addWidget(profile_desc);
            row_layout->addLayout(text_col, 1);

            QLabel* status = new QLabel();
            mode_status_labels.insert(p.name, status);
            row_layout->addWidget(status);

            QPushButton* btn_try = new QPushButton("Şimdi Dene");
            btn_try->setFixedSize(110, 34);
            btn_try->setCursor(Qt::PointingHandCursor);
            mode_try_btns << btn_try;
            connect(btn_try, &QPushButton::clicked, this, [this, id = p.id]() {
                stacked_widget->setCurrentIndex(0); // sonucu loglarda görsün diye Ana Ekran'a dön
                dashboard_widget->try_single_profile(id);
            });
            row_layout->addWidget(btn_try);

            if ((kind == 2 && !has_zapret) || (kind == 0 && !dns_mode_supported())) {
                check->setEnabled(false);
                btn_try->setEnabled(false);
            }
            block_layout->addWidget(row);
        }
        layout->addWidget(block);
    };

    add_group("🛡️ GoodbyeDPI Profilleri",
        "Hafif ve hızlı. Türk Telekom, Turkcell ve TurkNet'te çoğunlukla ilk profillerde çalışır.", 1);
    add_group("⚡ Zapret Profilleri",
        has_zapret ? "Daha gelişmiş bölme ve sahte paket teknikleri; Discord sesli sohbet (UDP) desteği içerir. Vodafone ve Kablonet gibi inatçı altyapılar için."
                   : "⚠️ Zapret motoru bu kurulumda bulunamadı (zapret\\winws.exe). Uygulamayı yeniden kurmayı deneyin.", 2);
    add_group("🔐 Sürücüsüz Mod (Şifreli DNS)",
        dns_mode_supported() ? "Sürücü yüklemeden DNS'i şifreler; anti-cheat sorunu çıkarmaz. Bazı ISS'ler (ör. Türk Telekom) DNS'in yanında DPI engeli de uyguladığı için tek başına yetmeyebilir: tarama bunu ölçer ve son çare olarak dener. Otomatik Oyun Modu, oyun sırasında Discord'u ayakta tutmak için bunu kullanır. Önceki DNS ayarlarınız her zaman birebir geri yüklenir."
                             : "⚠️ Bu mod Windows 11 gerektirir. Windows 10'da GoodbyeDPI/Zapret profilleri kullanılır.", 0);

    QPushButton* btn_reset = new QPushButton("↺ Tüm Profilleri Etkinleştir");
    btn_reset->setFixedHeight(42);
    btn_reset->setCursor(Qt::PointingHandCursor);
    mode_try_btns << btn_reset;
    connect(btn_reset, &QPushButton::clicked, this, [this]() {
        for (QCheckBox* check : mode_checks) {
            if (check->isEnabled()) check->setChecked(true);
        }
    });
    layout->addWidget(btn_reset);

    QLabel* note = new QLabel("İşareti kaldırılan profiller otonom taramada atlanır. 'Şimdi Dene' seçilen profili tek başına başlatıp Discord bağlantısını test eder; çalışırsa bir sonraki açılışta da o profil kullanılır.");
    note->setWordWrap(true);
    modes_descs << note;
    layout->addWidget(note);

    layout->addStretch();
    main_layout->addWidget(container);

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

void TheTakanosu_Elite::save_disabled_profiles() {
    QStringList disabled;
    for (QCheckBox* check : mode_checks) {
        if (!check->isChecked()) disabled << check->property("profile_id").toString();
    }
    QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("disabled_profiles", disabled);
}

void TheTakanosu_Elite::apply_modes_theme(bool is_dark) {
    const QString text = is_dark ? "#e0e0e0" : "#1a1a1a";
    const QString dim = is_dark ? "#a0a0a0" : "#555555";

    modes_header->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;").arg(is_dark ? "white" : "#1a1a1a"));
    for (QFrame* block : modes_blocks) {
        block->setStyleSheet(is_dark ? "QFrame#TargetBlock { background-color: #2b2b2b; border-radius: 12px; border: 1px solid #3a3a3a; }"
                                     : "QFrame#TargetBlock { background-color: #ffffff; border-radius: 12px; border: 1px solid #aaaaaa; }");
    }
    for (QLabel* title : modes_titles) {
        title->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;").arg(text));
    }
    for (QLabel* desc : modes_descs) {
        desc->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 12px; border: none; background: transparent; %2")
            .arg(dim, desc->property("indent").toBool() ? "margin-left: 26px;" : ""));
    }
    for (QCheckBox* check : mode_checks) {
        check->setStyleSheet(QString(
            "QCheckBox { color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: 600; spacing: 10px; background: transparent; } "
            "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid %2; background: %3; } "
            "QCheckBox::indicator:checked { background: #6a1b9a; border: 1px solid #8e24aa; } "
            "QCheckBox:disabled { color: #777777; }").arg(text, is_dark ? "#666666" : "#999999", is_dark ? "#333333" : "#ffffff"));
    }
    for (QPushButton* btn : mode_try_btns) {
        btn->setStyleSheet(
            "QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: none; } "
            "QPushButton:hover { background-color: #8e24aa; } QPushButton:pressed { background-color: #4a148c; } "
            "QPushButton:disabled { background-color: #444444; color: #888888; }");
    }
    for (QLabel* status : mode_status_labels) {
        status->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 12px; font-weight: bold; background: transparent;").arg(is_dark ? "#00E676" : "#008B00"));
    }

    if (is_dark) {
        combo_engine_mode->setStyleSheet("QComboBox { background-color: #333333; color: white; border-radius: 6px; padding-left: 15px; border: 1px solid #444444; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #3a3a3a; border: 1px solid #555555; } QComboBox::drop-down { border: none; width: 30px; }");
        combo_engine_mode->view()->setStyleSheet("QListView { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; outline: none; padding: 4px; } QListView::item { min-height: 32px; padding-left: 11px; border-radius: 4px; margin-bottom: 2px; } QListView::item:selected { background-color: #444444; color: white; } QListView::item:hover { background-color: #6a1b9a; color: white; }");
    }
    else {
        combo_engine_mode->setStyleSheet("QComboBox { background-color: #f9f9f9; color: #1a1a1a; border-radius: 6px; padding-left: 15px; border: 1px solid #aaaaaa; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; } QComboBox:hover { background-color: #e0e0e0; border: 1px solid #888888; } QComboBox::drop-down { border: none; width: 30px; }");
        combo_engine_mode->view()->setStyleSheet("QListView { background-color: #ffffff; color: #1a1a1a; border: 1px solid #cccccc; border-radius: 6px; outline: none; padding: 4px; } QListView::item { min-height: 32px; padding-left: 11px; border-radius: 4px; margin-bottom: 2px; } QListView::item:selected { background-color: #e0e0e0; color: black; } QListView::item:hover { background-color: #6a1b9a; color: white; }");
    }
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

    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
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

    bl_desc = new QLabel("DPI bypass motorunun filtreleyeceği ekstra domainleri (site adreslerini) buraya ekleyebilirsiniz.\nÖrnek: roblox.com (Sadece alan adını yazınız, http veya www eklemeyiniz.)\n★ ile işaretli siteler sizin ekledikleriniz: listenin başında durur ve güncellemelerde silinmez.\nDeğişiklikler DPI motoru yeniden başlatıldığında geçerli olur.");
    bl_desc->setStyleSheet("color: #a0a0a0; font-family: 'Segoe UI Variable'; font-size: 13px; border: none; background: transparent;");
    bl_desc->setWordWrap(true);
    bl_layout->addWidget(bl_desc);

    search_domain = new QLineEdit();
    search_domain->setPlaceholderText("🔍 Listedeki 500+ alan adı içinde ara...");
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

// Liste öğesi: görünen metin "★  site.com" olabilir; gerçek alan adı ve hangi dosyadan geldiği öğe verisinde saklanır
static const int ROLE_DOMAIN = Qt::UserRole;
static const int ROLE_CUSTOM = Qt::UserRole + 1;

static void add_list_item(QListWidget* w, const QString& domain, bool custom) {
    QListWidgetItem* item = new QListWidgetItem(custom ? "★  " + domain : domain);
    item->setData(ROLE_DOMAIN, domain);
    item->setData(ROLE_CUSTOM, custom);
    item->setToolTip(custom ? "Sizin eklediğiniz site (güncellemelerde korunur)" : "Uygulamayla gelen hazır liste");
    w->addItem(item);
}

void TheTakanosu_Elite::load_blacklist() {
    blacklist_widget->clear();

    // Önce kullanıcının listesi, sonra hazır liste
    for (const QString& d : read_list(custom_list_path())) add_list_item(blacklist_widget, d, true);

    bool ok = false;
    const QStringList shipped = read_list(shipped_list_path(), &ok);
    if (!ok || !QFileInfo::exists(shipped_list_path())) {
        QListWidgetItem* warn = new QListWidgetItem("⚠️ Liste dosyası bulunamadı! (turkey-blacklist.txt)");
        warn->setFlags(Qt::NoItemFlags); // seçilip "silinemesin"
        blacklist_widget->addItem(warn);
        return;
    }
    for (const QString& d : shipped) add_list_item(blacklist_widget, d, false);
}

void TheTakanosu_Elite::add_to_blacklist() {
    QString new_domain = input_new_domain->text().trimmed().toLower();

    // "https://www.site.com/yol:443" gibi girişleri sadece "site.com" olarak temizle
    new_domain.remove(QRegularExpression("^[a-z]+://"));
    new_domain = new_domain.section('/', 0, 0).section(':', 0, 0);
    if (new_domain.startsWith("www.")) new_domain = new_domain.mid(4);
    if (new_domain.isEmpty()) return; // kutu tamamen boşsa sessizce geç
    new_domain = QString::fromLatin1(QUrl::toAce(new_domain)); // Türkçe karakterli alan adları (IDN) için

    if (!valid_domain(new_domain)) {
        input_new_domain->clear();
        input_new_domain->setPlaceholderText("⚠️ Geçersiz alan adı! Örn: yasaklisite.com");
        return;
    }

    for (int i = 0; i < blacklist_widget->count(); ++i) {
        if (blacklist_widget->item(i)->data(ROLE_DOMAIN).toString() == new_domain) {
            input_new_domain->clear();
            input_new_domain->setPlaceholderText("ℹ️ Bu site zaten listede.");
            return;
        }
    }

    // Kullanıcının siteleri ayrı dosyaya: kurulum paketi güncellemede bu dosyaya dokunmaz
    bool ok = false;
    QStringList custom = read_list(custom_list_path(), &ok);
    if (ok) {
        custom << new_domain;
        ok = write_list(custom_list_path(), custom);
    }
    if (!ok) {
        input_new_domain->setPlaceholderText("⚠️ Liste dosyasına yazılamadı! Uygulamayı yönetici olarak çalıştırın.");
        input_new_domain->clear();
        return;
    }

    load_blacklist();
    filter_blacklist(search_domain->text());
    input_new_domain->clear();
    input_new_domain->setPlaceholderText("✔️ Eklendi! Örn: yasaklisite.com");
    blacklist_widget->scrollToTop();
}

void TheTakanosu_Elite::remove_from_blacklist() {
    QListWidgetItem* selected_item = blacklist_widget->currentItem();
    if (!selected_item) return;
    const QString domain = selected_item->data(ROLE_DOMAIN).toString();
    if (domain.isEmpty()) return; // uyarı satırı gibi gerçek olmayan öğeler

    const bool custom = selected_item->data(ROLE_CUSTOM).toBool();
    const QString path = custom ? custom_list_path() : shipped_list_path();

    // Dosya okunamazsa ASLA üzerine yazma (eskiden okunamayan liste boş olarak kaydediliyordu)
    bool ok = false;
    QStringList lines = read_list(path, &ok);
    if (!ok || !QFileInfo::exists(path)) {
        input_new_domain->setPlaceholderText("⚠️ Liste dosyası okunamadı, hiçbir şey silinmedi.");
        return;
    }
    lines.removeAll(domain);
    if (!write_list(path, lines)) {
        input_new_domain->setPlaceholderText("⚠️ Liste dosyasına yazılamadı! Uygulamayı yönetici olarak çalıştırın.");
        return;
    }

    delete blacklist_widget->takeItem(blacklist_widget->row(selected_item));
}

void TheTakanosu_Elite::toggle_startup_setting() {
    const bool want = btn_toggle_startup->isChecked();
    if (want) create_startup_task();
    else delete_startup_task();

    // Gerçek durumu Windows'tan tekrar oku (işlem başarısız olduysa buton yanlış durum göstermesin)
    const bool is_on = startup_task_exists();
    const bool is_dark = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("is_dark_theme", true).toBool();
    btn_toggle_startup->setChecked(is_on);
    btn_toggle_startup->setText(is_on ? "✔️ Başlangıçta Çalıştır (AÇIK)" : "❌ Başlangıçta Çalıştır (KAPALI)");
    if (is_on) btn_toggle_startup->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
    else if (is_dark) btn_toggle_startup->setStyleSheet("QPushButton { background-color: #333333; color: #a0a0a0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; }");
    else btn_toggle_startup->setStyleSheet("QPushButton { background-color: #f5f5f5; color: #555555; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #cccccc; }");
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
    apply_games_theme(is_dark);
    apply_tunnel_theme(is_dark);
    apply_modes_theme(is_dark);

    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
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
        for (QPushButton* toggle : { btn_toggle_tray, btn_toggle_game }) {
            if (!toggle) continue;
            if (toggle->isChecked()) toggle->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
            else toggle->setStyleSheet("QPushButton { background-color: #333333; color: #a0a0a0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; }");
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
        for (QPushButton* toggle : { btn_toggle_tray, btn_toggle_game }) {
            if (!toggle) continue;
            if (toggle->isChecked()) toggle->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; }");
            else toggle->setStyleSheet("QPushButton { background-color: #f5f5f5; color: #555555; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #cccccc; }");
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
    QTimer::singleShot(0, this, &TheTakanosu_Elite::sync_sidebar_height); // düzen oturduktan sonra tekrar eşitle
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

    // İkinci kopya açılmaya çalışıldığında gönderilen "pencereyi göster" sinyali
    static const UINT wm_takanosu_show = RegisterWindowMessageW(L"TheTakanosu_Elite_Show");
    if (msg->message == wm_takanosu_show) {
        this->showNormal();
        this->raise();
        this->activateWindow();
        *result = 0;
        return true;
    }

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
    // Uygulama kapanırken motorları ve WinDivert sürücüsünü kapat (anti-cheat dostu)
    if (dashboard_widget) dashboard_widget->stop_engine(true);
    // Tünel kuralları (NRPT) bilerek kalır: tünel açık bırakıldıysa uygulama kapalıyken listedeki siteler
    // Türkiye IP'siyle açılmaz, sonraki açılışta tünel kendiliğinden geri gelir.
    if (tunnel_proc) { tunnel_stopping = true; tunnel_proc->kill(); tunnel_proc->waitForFinished(2000); }
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
        // Tam ekranda alt kenardaki 1 px DWM camı gereksiz; Windows onu sol alt köşede boş bir çerçeve olarak çiziyordu
        MARGINS margins = { 0, 0, 0, this->isMaximized() ? 0 : 1 };
        DwmExtendFrameIntoClientArea(reinterpret_cast<HWND>(this->winId()), &margins);

        // Kenar boşlukları değişince düzen yeniden hesaplanır; kenar menüsünün boyunu ondan SONRA eşitle
        // (yoksa tam ekranda menü içerikten 16 px uzun kalıp pencerenin altına taşıyordu)
        QTimer::singleShot(0, this, &TheTakanosu_Elite::sync_sidebar_height);
    }
    QMainWindow::changeEvent(event);
}

// ==========================================
// 🗓️ BAŞLANGIÇTA ÇALIŞTIRMA (Görev Zamanlayıcı)
// Uygulama yönetici yetkisi istediği için Windows, "Run" kayıt anahtarındaki girdiyi açılışta sessizce engelliyordu.
// Görev Zamanlayıcı "en yüksek yetkiyle" başlatabildiği için uygulama UAC sormadan tepside açılır.
// ==========================================
bool TheTakanosu_Elite::startup_task_exists() {
    return run_hidden_sync("schtasks.exe", { "/Query", "/TN", SCHEDULED_TASK_NAME }) == 0;
}

bool TheTakanosu_Elite::create_startup_task() {
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString user = qEnvironmentVariable("USERDOMAIN") + "\\" + qEnvironmentVariable("USERNAME");
    const QString xml = QString(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <RegistrationInfo><Description>The Takanosu Elite: oturum açılışında DPI kalkanını sistem tepsisinde başlatır.</Description></RegistrationInfo>\n"
        "  <Triggers><LogonTrigger><Enabled>true</Enabled><UserId>%1</UserId><Delay>PT5S</Delay></LogonTrigger></Triggers>\n"
        "  <Principals><Principal id=\"Author\"><UserId>%1</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>\n"
        "  <Settings>\n"
        "    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
        "    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
        "    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
        "    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
        "    <Priority>4</Priority>\n"
        "  </Settings>\n"
        "  <Actions Context=\"Author\"><Exec><Command>%2</Command><Arguments>--tray</Arguments></Exec></Actions>\n"
        "</Task>\n").arg(user.toHtmlEscaped(), exe.toHtmlEscaped());

    // XML'i Temp yerine uygulama klasörüne (Program Files, sadece yöneticiler yazabilir) yazıyoruz;
    // böylece başka bir program dosyayı araya girip değiştirerek yönetici yetkisiyle komut çalıştıramaz.
    const QString xml_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/startup_task.xml");
    QFile f(xml_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write("\xFF\xFE", 2); // UTF-16LE BOM
    f.write(reinterpret_cast<const char*>(xml.utf16()), xml.size() * 2);
    f.close();

    const bool ok = run_hidden_sync("schtasks.exe", { "/Create", "/TN", SCHEDULED_TASK_NAME, "/XML", xml_path, "/F" }, 10000) == 0;
    QFile::remove(xml_path);
    return ok;
}

bool TheTakanosu_Elite::delete_startup_task() {
    return run_hidden_sync("schtasks.exe", { "/Delete", "/TN", SCHEDULED_TASK_NAME, "/F" }, 10000) == 0;
}

void TheTakanosu_Elite::migrate_legacy_startup() {
    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    const bool first_run = settings.value("is_first_run", true).toBool();

    bool had_run_entry = false;
    bool was_disabled = false;
    HKEY hKeyRun;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_SET_VALUE, &hKeyRun) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKeyRun, L"TheTakanosu_Elite", NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            had_run_entry = true;
            RegDeleteValueW(hKeyRun, L"TheTakanosu_Elite");
        }
        RegCloseKey(hKeyRun);
    }

    HKEY hKeyApproved;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run", 0, KEY_READ | KEY_SET_VALUE, &hKeyApproved) == ERROR_SUCCESS) {
        DWORD type = 0;
        BYTE data[12] = {};
        DWORD size = sizeof(data);
        if (RegQueryValueExW(hKeyApproved, L"TheTakanosu_Elite", NULL, &type, data, &size) == ERROR_SUCCESS) {
            // Görev Yöneticisi'nden "Devre dışı" yapıldıysa (ilk bayt 0x03) kullanıcının tercihine saygı göster
            was_disabled = (type == REG_BINARY && size > 0 && data[0] == 0x03);
            RegDeleteValueW(hKeyApproved, L"TheTakanosu_Elite");
        }
        RegCloseKey(hKeyApproved);
    }

    if ((first_run || had_run_entry) && !was_disabled) {
        create_startup_task();
    }
    settings.setValue("is_first_run", false);
}

// ==========================================
// 📊 MOTOR DURUMU DEĞİŞİNCE (istatistik + tepsi)
// ==========================================
void TheTakanosu_Elite::on_engine_state_changed(bool active, const QString& profile_name) {
    QSettings settings(takanosu_settings_path(), QSettings::IniFormat);
    if (stat_1_val) stat_1_val->setText(QString::number(settings.value("total_launches", 0).toInt()));
    if (active && stat_2_val) stat_2_val->setText(profile_name);

    for (auto it = mode_status_labels.begin(); it != mode_status_labels.end(); ++it) {
        it.value()->setText(active && it.key() == profile_name ? "● Aktif" : "");
    }

    if (act_toggle_engine) act_toggle_engine->setText(active ? "🎮 DPI'ı Durdur (Oyun Modu)" : "🚀 DPI Kalkanını Başlat");
    if (tray_icon) tray_icon->setToolTip(active ? "The Takanosu Elite — Aktif: " + profile_name : "The Takanosu Elite — Beklemede");
}

// ==========================================
// 🚀 ARKA PLAN OTONOM GÜNCELLEME KONTROL MERKEZİ (IN-APP OTA UPDATE)
// GitHub API'den son sürüm okunur; indirilen kurulum dosyasının SHA-256 özeti GitHub'ın verdiği özetle
// birebir eşleşmezse ÇALIŞTIRILMAZ. (Eski sürüm yanlış dosya adını indiriyor ve hiçbir doğrulama yapmıyordu.)
// ==========================================
static void style_elite_box(QMessageBox& box) {
    box.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    box.setIconPixmap(QPixmap(QCoreApplication::applicationDirPath() + "/assets/icon-transparant.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    box.setStyleSheet(
        "QMessageBox { background-color: #1a1a1a; border: 1px solid #444444; border-radius: 12px; }"
        "QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: 500; border: none; padding-top: 5px; }"
        "QLabel#qt_msgbox_label { color: #00E676; font-size: 14px; font-weight: bold; margin-bottom: 5px; }"
        "QPushButton { background-color: #333333; color: white; border-radius: 8px; padding: 10px 20px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: 1px solid #555555; margin-top: 15px; min-width: 120px; }"
        "QPushButton:hover { background-color: #444444; }"
        "QPushButton:pressed { background-color: #555555; }"
    );
}

static void show_elite_info(QWidget* parent, const QString& title, const QString& text, const QString& info) {
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setText(text);
    box.setInformativeText(info);
    style_elite_box(box);
    box.exec();
}

void TheTakanosu_Elite::check_updates(bool manual) {
    const QString release_prefix = "https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/releases/download/";
    const QString api_url = "https://api.github.com/repos/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/releases/latest";
    const QString update_dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/update_cache");
    QDir(update_dir).removeRecursively(); // önceki güncellemeden kalan dosyaları temizle

    QProcess* curl_api = new QProcess(this);
    hide_console(curl_api);

    connect(curl_api, &QProcess::finished, this, [this, curl_api, manual, release_prefix, update_dir](int exit_code) {
        const QByteArray body = curl_api->readAllStandardOutput();
        curl_api->deleteLater();

        const QJsonObject release = QJsonDocument::fromJson(body).object();
        QString tag = release.value("tag_name").toString();
        if (tag.startsWith('v', Qt::CaseInsensitive)) tag = tag.mid(1);
        const QVersionNumber latest = QVersionNumber::fromString(tag);
        const QVersionNumber current = QVersionNumber::fromString(APP_VERSION);

        if (exit_code != 0 || latest.isNull()) {
            if (manual) show_elite_info(this, "Güncelleme Sistemi", "⚠️ Güncelleme sunucusuna ulaşılamadı.", "Lütfen internet bağlantınızı kontrol edip daha sonra tekrar deneyin.");
            return;
        }
        if (latest <= current) {
            if (manual) show_elite_info(this, "Güncelleme Sistemi", "✅ En güncel sürümü kullanıyorsunuz!", QString("Mevcut Sürüm: v%1").arg(APP_VERSION));
            return;
        }

        QString download_url, file_name, sha256;
        for (const QJsonValue& v : release.value("assets").toArray()) {
            const QJsonObject asset = v.toObject();
            const QString name = asset.value("name").toString();
            if (name.startsWith("TheTakanosu_Elite", Qt::CaseInsensitive) && name.endsWith("_Setup.exe", Qt::CaseInsensitive)) {
                file_name = name;
                download_url = asset.value("browser_download_url").toString();
                const QString digest = asset.value("digest").toString();
                if (digest.startsWith("sha256:")) sha256 = digest.mid(7).toLower();
                break;
            }
        }
        const bool can_auto_install = !download_url.isEmpty() && download_url.startsWith(release_prefix) && sha256.size() == 64;

        // 1. AŞAMA: EVET / HAYIR SORUSU
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Güncelleme Sistemi");
        msgBox.setText("🔄 Yeni Bir Güncelleme Mevcut: v" + latest.toString());
        msgBox.setInformativeText(QString(
            "The Takanosu Elite için yeni bir sürüm yayınlandı.\n\n"
            "Mevcut Sürüm: v%1\n"
            "Yeni Sürüm: v%2\n\n%3"
        ).arg(APP_VERSION, latest.toString(), can_auto_install
            ? "En iyi performans ve güncel koruma listeleri için güncellemeyi şimdi otomatik olarak indirip kurmak ister misiniz?"
            : "Bu sürüm otomatik kurulamıyor. İndirme sayfasını açmak ister misiniz?"));

        QPushButton* btn_yes = msgBox.addButton(can_auto_install ? "Evet, İndir ve Kur" : "Sayfayı Aç", QMessageBox::YesRole);
        msgBox.addButton("Hayır, Daha Sonra", QMessageBox::NoRole);
        style_elite_box(msgBox);
        btn_yes->setStyleSheet("background-color: #6a1b9a; color: white; border: none;"); // Evet butonu Elite Moru

        msgBox.exec();
        if (msgBox.clickedButton() != btn_yes) return;

        if (!can_auto_install) {
            open_url_unelevated("https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/releases/latest");
            return;
        }

        // 2. AŞAMA: İNDİRME
        QMessageBox* dlBox = new QMessageBox(this);
        dlBox->setWindowTitle("İndiriliyor");
        dlBox->setText("⚙️ Güncelleme Paketi İndiriliyor...");
        dlBox->setInformativeText("Lütfen bekleyin. İndirme ve güvenlik doğrulaması tamamlandığında kurulum otomatik olarak başlayacaktır.");
        style_elite_box(*dlBox);
        dlBox->setStandardButtons(QMessageBox::NoButton);
        dlBox->show();

        QDir().mkpath(update_dir);
        const QString setup_path = update_dir + "\\" + QFileInfo(file_name).fileName();

        QProcess* curl_dl = new QProcess(this);
        hide_console(curl_dl);
        connect(curl_dl, &QProcess::finished, this, [this, curl_dl, dlBox, setup_path, sha256](int dl_exit) {
            curl_dl->deleteLater();
            dlBox->accept();
            dlBox->deleteLater();

            // 3. AŞAMA: SHA-256 DOĞRULAMASI
            bool verified = false;
            QFile setup_file(setup_path);
            if (dl_exit == 0 && setup_file.open(QIODevice::ReadOnly)) {
                QCryptographicHash hash(QCryptographicHash::Sha256);
                hash.addData(&setup_file);
                verified = (QString::fromLatin1(hash.result().toHex()) == sha256);
                setup_file.close();
            }

            if (!verified) {
                QFile::remove(setup_path);
                show_elite_info(this, "Güncelleme Hatası", "⚠️ Güncelleme doğrulanamadı!",
                    dl_exit == 0 ? "İndirilen dosyanın güvenlik özeti (SHA-256) GitHub ile eşleşmedi, dosya silindi."
                                 : "İndirme başarısız oldu. Lütfen internet bağlantınızı kontrol edin ve daha sonra tekrar deneyin.");
                return;
            }

            // Doğrulanmış kurulumu çalıştır ve uygulamayı kapat (motorlar yıkıcıda temizce durdurulur)
            if (QProcess::startDetached(setup_path, QStringList())) {
                qApp->quit();
            }
        });

        curl_dl->start(sys_tool("curl.exe"), { "-s", "-f", "-L", "--ssl-no-revoke", "--max-time", "600", "-o", setup_path, download_url });
    });

    curl_api->start(sys_tool("curl.exe"), { "-s", "-f", "--ssl-no-revoke", "--max-time", "10",
        "-H", "Accept: application/vnd.github+json", "-H", "User-Agent: TheTakanosu-Elite", api_url });
}


// ==========================================
// 🎮 OYUNLARIM LİSTESİ (ayarlar sayfası)
// ==========================================
void TheTakanosu_Elite::reload_games_list() {
    if (!games_list) return;
    const int keep = games_list->currentRow();
    games_list->clear();
    for (const GameRule& r : load_game_rules()) {
        games_list->addItem(QString("%1   ·   %2   →   %3").arg(r.name, r.exes.join(", "), game_action_text(r)));
    }
    if (keep >= 0 && keep < games_list->count()) games_list->setCurrentRow(keep);
}

void TheTakanosu_Elite::apply_games_theme(bool is_dark) {
    if (!games_block) return;
    const QString block_bg = is_dark ? "#2b2b2b" : "#ffffff";
    const QString block_border = is_dark ? "#3a3a3a" : "#aaaaaa";
    games_block->setStyleSheet(QString("QFrame#TargetBlock { background-color: %1; border-radius: 8px; border: 1px solid %2; }").arg(block_bg, block_border));
    games_title->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;").arg(is_dark ? "#e0e0e0" : "#1a1a1a"));
    games_desc->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 13px; border: none; background: transparent;").arg(is_dark ? "#a0a0a0" : "#555555"));
    games_list->setStyleSheet(QString(
        "QListWidget { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 5px; font-family: 'Segoe UI Variable'; font-size: 13px; outline: none; } "
        "QListWidget::item { padding: 4px 6px; min-height: 24px; border-radius: 4px; } "
        "QListWidget::item:selected { background-color: #6a1b9a; color: white; } ").arg(is_dark ? "#1a1a1a" : "#f5f5f5", is_dark ? "#e0e0e0" : "#333333", is_dark ? "#444444" : "#cccccc"));
    for (QPushButton* b : games_btns) {
        b->setStyleSheet(is_dark
            ? "QPushButton { background-color: #333333; color: #e0e0e0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: 1px solid #444444; } QPushButton:hover { background-color: #6a1b9a; color: white; }"
            : "QPushButton { background-color: #f0f0f0; color: #1a1a1a; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: 1px solid #aaaaaa; } QPushButton:hover { background-color: #6a1b9a; color: white; }");
    }
}

void TheTakanosu_Elite::scan_games_dialog() {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QList<GameRule> rules = load_game_rules();
    const QList<FoundGame> found = new_found_games(scan_installed_games(), rules);
    QGuiApplication::restoreOverrideCursor();

    const bool dark = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("is_dark_theme", true).toBool();
    QDialog dlg(this);
    dlg.setWindowTitle("Oyunlarımı Bul");
    dlg.setMinimumWidth(560);
    dlg.setStyleSheet(dark
        ? "QDialog { background-color: #1f1f1f; } QLabel, QCheckBox { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QPushButton { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; padding: 6px 16px; font-family: 'Segoe UI Variable'; } QPushButton:hover { background-color: #6a1b9a; } "
          "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #777777; border-radius: 4px; background-color: #2b2b2b; } "
          "QCheckBox::indicator:checked { background-color: #6a1b9a; border: 1px solid #9c4dcc; } QCheckBox::indicator:hover { border: 1px solid #9c4dcc; }"
        : "QDialog { background-color: #f5f5f5; } QLabel, QCheckBox { color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QPushButton { background-color: #e0e0e0; color: #1a1a1a; border: 1px solid #aaaaaa; border-radius: 6px; padding: 6px 16px; font-family: 'Segoe UI Variable'; } QPushButton:hover { background-color: #6a1b9a; color: white; } "
          "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #888888; border-radius: 4px; background-color: white; } "
          "QCheckBox::indicator:checked { background-color: #6a1b9a; border: 1px solid #6a1b9a; } QCheckBox::indicator:hover { border: 1px solid #6a1b9a; }");
    QVBoxLayout* l = new QVBoxLayout(&dlg);
    QLabel* info = new QLabel(found.isEmpty()
        ? "Yeni oyun bulunamadı. (Steam, Epic ve Ubisoft kütüphanelerine bakıldı; Oyunlarım'da zaten olanlar gösterilmez.)"
        : "Eklemek istediklerini işaretle. 🛡️ olanlarda anti-cheat bulundu, onlar için \"Kalkanı kapat\" seçilir. "
          "Diğerleri \"Dokunma\" ile eklenir; sonra Düzenle'den oyun boyunca başka profile geçmesini seçebilirsin.");
    info->setWordWrap(true);
    l->addWidget(info);
    QList<QCheckBox*> boxes;
    QWidget* list_w = new QWidget();
    QVBoxLayout* list_l = new QVBoxLayout(list_w);
    for (const FoundGame& g : found) {
        QCheckBox* cb = new QCheckBox(QString("%1 %2   ·   %3").arg(g.anticheat ? "🛡️" : "🎮", g.name, g.exes.join(", ")));
        cb->setChecked(g.anticheat);
        cb->setToolTip(QDir::toNativeSeparators(g.dir));
        list_l->addWidget(cb);
        boxes << cb;
    }
    list_l->addStretch();
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidget(list_w);
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(qMin(60 + 30 * int(found.size()), 360));
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; } QWidget { background: transparent; }");
    if (!found.isEmpty()) l->addWidget(scroll);
    QDialogButtonBox* bb = new QDialogButtonBox();
    if (!found.isEmpty()) bb->addButton("Seçilenleri Ekle", QDialogButtonBox::AcceptRole);
    bb->addButton(found.isEmpty() ? "Tamam" : "Vazgeç", QDialogButtonBox::RejectRole);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    l->addWidget(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    int added = 0;
    for (int i = 0; i < found.size(); ++i) {
        if (!boxes[i]->isChecked()) continue;
        rules << GameRule{ found[i].name, found[i].exes, found[i].anticheat ? "off" : "keep", QString() };
        ++added;
    }
    if (added) { save_game_rules(rules); reload_games_list(); apply_games_theme(dark); }
}

// "Yeni kurulan anti-cheat'li oyunları otomatik ekle" açıksa: daha önce görülmemiş anti-cheat'li oyun klasörleri eklenir.
// Görülen klasörler kaydedilir; kullanıcının kaldırdığı oyun bir daha eklenmez.
void TheTakanosu_Elite::auto_add_games() {
    QSettings s(takanosu_settings_path(), QSettings::IniFormat);
    if (!s.value("games_autoscan", false).toBool()) return;
    QStringList seen = s.value("games_seen_dirs").toStringList();
    QList<GameRule> rules = load_game_rules();
    QStringList added;
    for (const FoundGame& g : new_found_games(scan_installed_games(), rules)) {
        const QString key = QDir::cleanPath(g.dir).toLower();
        if (seen.contains(key)) continue;
        seen << key;
        if (!g.anticheat) continue;
        rules << GameRule{ g.name, g.exes, "off", QString() };
        added << g.name;
    }
    s.setValue("games_seen_dirs", seen);
    if (added.isEmpty()) return;
    save_game_rules(rules);
    reload_games_list();
    apply_games_theme(s.value("is_dark_theme", true).toBool());
    if (dashboard_widget) dashboard_widget->log_event("OYUN MODU", QString("Yeni anti-cheat'li oyun bulundu, Oyunlarım'a eklendi: %1").arg(added.join(", ")));
}

// row < 0: yeni oyun (önce exe seçtirilir), row >= 0: mevcut kuralı düzenle
void TheTakanosu_Elite::edit_game_rule(int row) {
    QList<GameRule> rules = load_game_rules();
    GameRule rule{ QString(), QStringList(), "off", QString() };
    if (row >= 0) {
        if (row >= rules.size()) return;
        rule = rules[row];
    }
    else {
        const QString path = QFileDialog::getOpenFileName(this, "Oyunun .exe dosyasını seç", "C:/", "Oyun (*.exe)");
        if (path.isEmpty()) return;
        const QString exe = QFileInfo(path).fileName().toLower();
        if (exe == "thetakanosu_elite.exe" || exe == "explorer.exe") return;
        for (int i = 0; i < rules.size(); ++i) {
            if (rules[i].exes.contains(exe)) { row = i; rule = rules[i]; break; } // zaten varsa onu düzenle
        }
        if (row < 0) { rule.exes = { exe }; rule.name = QFileInfo(path).completeBaseName(); }
    }

    const bool dark = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("is_dark_theme", true).toBool();
    QDialog dlg(this);
    dlg.setWindowTitle(row < 0 ? "Oyun Ekle" : "Oyunu Düzenle");
    dlg.setMinimumWidth(460);
    dlg.setStyleSheet(dark
        ? "QDialog { background-color: #1f1f1f; } QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QLineEdit, QComboBox { background-color: #2b2b2b; color: white; border: 1px solid #444444; border-radius: 6px; padding: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QComboBox QAbstractItemView { background-color: #2b2b2b; color: white; selection-background-color: #6a1b9a; } QComboBox:disabled { color: #666666; background-color: #242424; } "
          "QPushButton { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; padding: 6px 16px; font-family: 'Segoe UI Variable'; } QPushButton:hover { background-color: #6a1b9a; }"
        : "QDialog { background-color: #f5f5f5; } QLabel { color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QLineEdit, QComboBox { background-color: white; color: #1a1a1a; border: 1px solid #aaaaaa; border-radius: 6px; padding: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; } QComboBox:disabled { color: #999999; background-color: #eeeeee; } "
          "QPushButton { background-color: #e0e0e0; color: #1a1a1a; border: 1px solid #aaaaaa; border-radius: 6px; padding: 6px 16px; font-family: 'Segoe UI Variable'; } QPushButton:hover { background-color: #6a1b9a; color: white; }");

    QFormLayout* form = new QFormLayout(&dlg);
    QLineEdit* name_edit = new QLineEdit(rule.name);
    QLineEdit* exes_edit = new QLineEdit(rule.exes.join(", "));
    exes_edit->setToolTip("Birden çok exe virgülle ayrılabilir (ör. r5apex.exe, r5apex_dx12.exe)");
    QComboBox* action_box = new QComboBox();
    action_box->addItem("🛡️ Kalkanı kapat (anti-cheat'li oyunlar)", "off");
    action_box->addItem("🔁 Oyun boyunca başka profile geç", "profile");
    action_box->addItem("✋ Dokunma (kalkan olduğu gibi kalsın)", "keep");
    action_box->setCurrentIndex(qMax(0, action_box->findData(rule.action)));
    QComboBox* profile_box = new QComboBox();
    for (const DpiProfile& p : dpi_profiles()) profile_box->addItem(p.name, p.id);
    profile_box->setCurrentIndex(qMax(0, profile_box->findData(rule.profile.isEmpty() ? QString("gd_joker") : rule.profile)));
    QLabel* warn = new QLabel("⚠️ Anti-cheat'li oyunlarda (EAC, BattlEye, FACEIT) profil değiştirmek yerine \"Kalkanı kapat\" seçin; WinDivert açıkken oyun açılmayabilir.");
    warn->setWordWrap(true);
    auto sync = [=]() { const bool p = action_box->currentData().toString() == "profile"; profile_box->setEnabled(p); warn->setVisible(p); };
    connect(action_box, &QComboBox::currentIndexChanged, &dlg, sync);
    sync();

    form->addRow("Oyun adı:", name_edit);
    form->addRow("Exe:", exes_edit);
    form->addRow("Açılınca:", action_box);
    form->addRow("Profil:", profile_box);
    form->addRow(warn);
    QDialogButtonBox* box = new QDialogButtonBox();
    box->addButton("Kaydet", QDialogButtonBox::AcceptRole);
    box->addButton("Vazgeç", QDialogButtonBox::RejectRole);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);
    if (dlg.exec() != QDialog::Accepted) return;

    QStringList exes;
    for (QString e : exes_edit->text().split(',', Qt::SkipEmptyParts)) {
        e = e.trimmed().toLower();
        if (e.isEmpty()) continue;
        if (!e.endsWith(".exe")) e += ".exe";
        if (!exes.contains(e)) exes << e;
    }
    if (exes.isEmpty()) return;
    rule.name = name_edit->text().trimmed().isEmpty() ? exes.first() : name_edit->text().trimmed();
    rule.exes = exes;
    rule.action = action_box->currentData().toString();
    rule.profile = rule.action == "profile" ? profile_box->currentData().toString() : QString();
    if (row < 0) rules << rule; else rules[row] = rule;
    save_game_rules(rules);
    reload_games_list();
    apply_games_theme(dark);
}


// ==========================================
// 🛡️ TÜNEL (GİZLİLİK) MODU
// Nasıl çalışır: Windows'un NRPT kuralları listedeki alan adlarının DNS sorgularını 127.0.0.53'teki sing-box'a yollar.
// sing-box bu adlara sahte IP (198.18.x) verir; yalnız o aralık "Elite Tunel" sanal ağ kartına yönlenir ve WireGuard ile
// yurt dışına çıkar. Oyunlar ve diğer her şey sing-box'a hiç uğramaz, ping değişmez.
// Kural kalıcıdır: sing-box çalışmıyorsa listedeki siteler hiç çözülmez, yani Türkiye IP'siyle açılmaz (kill switch).
// ==========================================
// Tünel bağlantısı: WireGuard .conf (Proton, Mullvad...) ya da vless:// (kullanıcının kendi sunucusu, Reality)
struct WgConf {
    bool ok = false; QString error, private_key, peer_key, host, name; int port = 0; QStringList addresses;
    bool vless = false; QString uuid, flow, sni, fingerprint, public_key, short_id;
};

static WgConf parse_wg_conf(const QString& path) {
    WgConf c;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { c.error = "Dosya açılamadı."; return c; }
    QString section;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith('[')) { section = line.toLower(); continue; }
        if (line.startsWith('#')) {
            // Proton gibi sağlayıcılar sunucu adını [Peer] altına yorum olarak yazar (# NL-FREE#15)
            if (section == "[peer]" && c.name.isEmpty()) c.name = line.mid(1).trimmed();
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed().toLower();
        const QString val = line.mid(eq + 1).trimmed();
        if (section == "[interface]" && key == "privatekey") c.private_key = val;
        else if (section == "[interface]" && key == "address") { for (const QString& a : val.split(',')) if (!a.trimmed().isEmpty()) c.addresses << a.trimmed(); }
        else if (section == "[peer]" && key == "publickey") c.peer_key = val;
        else if (section == "[peer]" && key == "endpoint") {
            const int colon = val.lastIndexOf(':');
            if (colon > 0) { c.host = val.left(colon).remove('[').remove(']'); c.port = val.mid(colon + 1).toInt(); }
        }
    }
    static const QRegularExpression key_re("^[A-Za-z0-9+/]{42,43}=?$");
    if (!key_re.match(c.private_key).hasMatch() || !key_re.match(c.peer_key).hasMatch()) c.error = "Anahtarlar okunamadı. Bu bir WireGuard .conf dosyası mı?";
    else if (c.host.isEmpty() || c.port <= 0 || c.port > 65535) c.error = "Sunucu adresi (Endpoint) okunamadı.";
    else if (c.addresses.isEmpty()) c.error = "Tünel adresi (Address) okunamadı.";
    else c.ok = true;
    if (c.name.isEmpty()) c.name = c.host;
    return c;
}

// vless://UUID@sunucu:443?security=reality&sni=...&pbk=...&sid=...&flow=xtls-rprx-vision&fp=chrome#ad
static WgConf parse_vless_url(const QString& text) {
    WgConf c;
    c.vless = true;
    const QUrl u(text.trimmed());
    const QUrlQuery q(u);
    static const QRegularExpression uuid_re("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    static const QRegularExpression key_re("^[A-Za-z0-9_-]{43}$");
    static const QRegularExpression sid_re("^[0-9a-fA-F]{0,16}$");
    static const QRegularExpression host_re("^[A-Za-z0-9.:-]{1,253}$");
    c.uuid = u.userName();
    c.host = u.host();
    c.port = u.port(443);
    c.sni = q.queryItemValue("sni");
    c.public_key = q.queryItemValue("pbk");
    c.short_id = q.queryItemValue("sid");
    c.flow = q.queryItemValue("flow");
    c.fingerprint = q.queryItemValue("fp").isEmpty() ? QString("chrome") : q.queryItemValue("fp");
    c.name = u.fragment(QUrl::FullyDecoded).trimmed();
    if (u.scheme() != "vless") c.error = "Bağlantı vless:// ile başlamalı.";
    else if (q.queryItemValue("security") != "reality") c.error = "Şimdilik yalnız Reality (security=reality) destekleniyor.";
    else if (!uuid_re.match(c.uuid).hasMatch()) c.error = "UUID okunamadı.";
    else if (!host_re.match(c.host).hasMatch() || c.port <= 0 || c.port > 65535) c.error = "Sunucu adresi okunamadı.";
    else if (!key_re.match(c.public_key).hasMatch() || !sid_re.match(c.short_id).hasMatch() || !valid_domain(c.sni)) c.error = "Reality bilgileri (pbk/sid/sni) okunamadı.";
    else if (!c.flow.isEmpty() && c.flow != "xtls-rprx-vision") c.error = "Desteklenmeyen flow: " + c.flow;
    else c.ok = true;
    if (c.name.isEmpty()) c.name = c.host;
    return c;
}

// Etkin bağlantı: vless dosyası varsa o, yoksa WireGuard .conf
static WgConf load_tunnel_conf() {
    QFile f(tunnel_vless_path());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) return parse_vless_url(QString::fromUtf8(f.readAll()));
    const QString conf = tunnel_conf_path();
    return parse_wg_conf(conf);
}

static QByteArray build_singbox_config(const WgConf& c, const QStringList& domains) {
    const QJsonArray dom = QJsonArray::fromStringList(domains);
    QJsonObject dns{
        { "servers", QJsonArray{
            QJsonObject{ { "type", "fakeip" }, { "tag", "fake" }, { "inet4_range", "198.18.0.0/15" }, { "inet6_range", "fc00::/18" } },
            // Gerçek adresler tünelin içinden (şifreli DNS ile) çözülür; ISS bu sorguları göremez
            QJsonObject{ { "type", "https" }, { "tag", "tunnel-dns" }, { "server", "1.1.1.1" }, { "detour", "wg" } } } },
        { "rules", QJsonArray{ QJsonObject{ { "domain_suffix", dom }, { "server", "fake" } } } },
        { "final", "tunnel-dns" },
        { "strategy", "ipv4_only" },
    };
    QJsonArray tun_addr{ "172.19.0.1/30", "fdfe:dcba:9876::1/126" };
    QJsonArray inbounds{
        QJsonObject{ { "type", "tun" }, { "tag", "tun-in" }, { "interface_name", "Elite Tunel" }, { "address", tun_addr },
                     { "auto_route", true }, { "strict_route", false },
                     { "route_address", QJsonArray{ "198.18.0.0/15", "fc00::/18" } }, { "stack", "gvisor" } },
        QJsonObject{ { "type", "direct" }, { "tag", "dns-in" }, { "listen", "127.0.0.53" }, { "listen_port", 53 } },
    };
    QJsonObject peer{ { "address", c.host }, { "port", c.port }, { "public_key", c.peer_key },
                      { "allowed_ips", QJsonArray{ "0.0.0.0/0", "::/0" } }, { "persistent_keepalive_interval", 25 } };
    QJsonObject wg{ { "type", "wireguard" }, { "tag", "wg" }, { "system", false }, { "mtu", 1420 },
                    { "address", QJsonArray::fromStringList(c.addresses) }, { "private_key", c.private_key },
                    { "peers", QJsonArray{ peer } } };
    QJsonObject route{
        { "rules", QJsonArray{
            QJsonObject{ { "inbound", "dns-in" }, { "action", "hijack-dns" } },
            // Windows tünel kartına da DNS sunucusu (172.19.0.2) atanıyor; oraya düşen sorgular cevapsız kalırsa
            // tüm siteler 12 sn bekliyordu. Kart DNS'i ayrıca siliniyor, buraya düşen olursa da cevaplanıyor.
            QJsonObject{ { "ip_cidr", QJsonArray{ "172.19.0.2/32", "fdfe:dcba:9876::2/128" } }, { "port", 53 }, { "action", "hijack-dns" } },
            // Tünelden geçmeyecek programlar: YouTube veri merkezi IP'lerinden gelen yt-dlp isteklerini "bot musun?" diye
            // engelliyor (Ghost Engine/mpv şarkıları açamıyordu, 2026-10-13). Bunlar ev IP'sinden doğrudan çıkar.
            QJsonObject{ { "process_name", QJsonArray{ "yt-dlp.exe", "mpv.exe", "ffmpeg.exe" } }, { "outbound", "direct" } },
            // sing-box 1.12+: tünele yollamadan önce alan adı açıkça çözülmeli
            QJsonObject{ { "domain_suffix", dom }, { "action", "resolve" }, { "server", "tunnel-dns" }, { "strategy", "ipv4_only" } },
            QJsonObject{ { "domain_suffix", dom }, { "outbound", "wg" } } } },
        { "final", "direct" },
        { "auto_detect_interface", true },
        { "find_process", true },
        { "default_domain_resolver", "tunnel-dns" },
    };
    // Not: tünel çıkışının etiketi her iki türde de "wg" (DNS ve yönlendirme kuralları ona bakıyor)
    QJsonArray outbounds{ QJsonObject{ { "type", "direct" }, { "tag", "direct" } } };
    QJsonArray endpoints;
    if (c.vless) {
        QJsonObject vless{ { "type", "vless" }, { "tag", "wg" }, { "server", c.host }, { "server_port", c.port }, { "uuid", c.uuid },
            { "tls", QJsonObject{ { "enabled", true }, { "server_name", c.sni },
                                  { "utls", QJsonObject{ { "enabled", true }, { "fingerprint", c.fingerprint } } },
                                  { "reality", QJsonObject{ { "enabled", true }, { "public_key", c.public_key }, { "short_id", c.short_id } } } } } };
        if (!c.flow.isEmpty()) vless["flow"] = c.flow;
        outbounds.append(vless);
    }
    else endpoints.append(wg);
    QJsonObject root{
        { "log", QJsonObject{ { "level", "warn" } } },
        { "dns", dns }, { "inbounds", inbounds }, { "outbounds", outbounds },
        { "route", route },
    };
    if (!endpoints.isEmpty()) root["endpoints"] = endpoints;
    // Sahte IP ↔ alan adı eşleşmesi diske yazılır: tünel yeniden başlayınca tarayıcının önbellekteki 198.18.x
    // adresleri boşa düşmesin (yoksa site açılmıyor; 2026-10-13'te TikTok'ta yaşandı)
    root["experimental"] = QJsonObject{ { "cache_file", QJsonObject{ { "enabled", true }, { "path", "cache.db" }, { "store_fakeip", true } } } };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

static QString nrpt_script(const QStringList& domains) {
    QString s;
    if (!domains.isEmpty()) {
        // Tünel kartının DNS sunucusunu sil (kart birkaç saniye içinde oluşur)
        s += "for ($i = 0; $i -lt 20 -and -not (Get-NetAdapter -Name 'Elite Tunel' -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 500 }; "
             "Set-DnsClientServerAddress -InterfaceAlias 'Elite Tunel' -ResetServerAddresses -ErrorAction SilentlyContinue; ";
    }
    s += QString("$c='%1'; Get-DnsClientNrptRule | Where-Object { $_.Comment -eq $c } | ForEach-Object { Remove-DnsClientNrptRule -Name $_.Name -Force }; ").arg(TUNNEL_NRPT_COMMENT);
    for (const QString& d : domains) {
        if (!valid_domain(d)) continue; // alan adı betiğe girmeden doğrulanır
        s += QString("Add-DnsClientNrptRule -Namespace '.%1','%1' -NameServers '127.0.0.53' -Comment $c | Out-Null; ").arg(d);
    }
    s += "Clear-DnsClientCache";
    return s;
}

bool takanosu_tunnel_cleanup() {
    QProcess p;
    p.start(powershell_exe(), powershell_args(nrpt_script({})));
    return p.waitForFinished(30000) && p.exitCode() == 0;
}

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

// Telefona aktarma: v2rayNG / Hiddify (vless://) ve WireGuard uygulaması (.conf metni) bu QR'ı doğrudan okur
static QImage make_qr_image(const QString& text, int scale) {
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 4, size = qr.getSize();
    QImage img((size + border * 2) * scale, (size + border * 2) * scale, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (qr.getModule(x, y)) p.fillRect((x + border) * scale, (y + border) * scale, scale, scale, Qt::black);
    return img;
}

static void style_tunnel_button(QPushButton* b, bool dark, bool primary) {
    if (primary) b->setStyleSheet("QPushButton { background-color: #6a1b9a; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; } QPushButton:hover { background-color: #7b1fa2; }");
    else b->setStyleSheet(dark
        ? "QPushButton { background-color: #333333; color: #e0e0e0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: 1px solid #444444; } QPushButton:hover { background-color: #6a1b9a; color: white; }"
        : "QPushButton { background-color: #f0f0f0; color: #1a1a1a; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 13px; font-weight: bold; border: 1px solid #aaaaaa; } QPushButton:hover { background-color: #6a1b9a; color: white; }");
}

QWidget* TheTakanosu_Elite::create_tunnel_page() {
    QWidget* page = new QWidget();
    page->setStyleSheet("background-color: transparent;");
    QVBoxLayout* main_l = new QVBoxLayout(page);
    main_l->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    main_l->setContentsMargins(20, 30, 20, 20);
    QWidget* container = new QWidget();
    container->setMaximumWidth(800);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setSpacing(25);

    tunnel_header = new QLabel("🛡️ Tünel (Gizlilik Modu)");
    layout->addWidget(tunnel_header);

    auto make_block = [this](const QString& title, const QString& desc) {
        QFrame* b = new QFrame();
        b->setObjectName("TargetBlock");
        QVBoxLayout* l = new QVBoxLayout(b);
        l->setContentsMargins(20, 20, 20, 20);
        l->setSpacing(12);
        QLabel* t = new QLabel(title);
        QLabel* d = new QLabel(desc);
        d->setWordWrap(true);
        l->addWidget(t);
        l->addWidget(d);
        tunnel_blocks << b; tunnel_titles << t; tunnel_descs << d;
        return l;
    };

    QVBoxLayout* l1 = make_block("🔐 Bağlantı",
        "Seçtiğin siteler (Instagram, TikTok, YouTube, X...) bir WireGuard tünelinden yurt dışına çıkar; platformlar seni Türkiye'den bağlanıyor görmez. "
        "Oyunlar ve diğer her şey doğrudan bağlanır, ping değişmez.\n"
        "Ücretsiz bağlantı dosyası için: Proton VPN → Downloads → WireGuard configuration (rehber GitHub sayfasında).");
    tunnel_conf_label = new QLabel();
    tunnel_conf_label->setWordWrap(true);
    l1->addWidget(tunnel_conf_label);
    QPushButton* btn_import = new QPushButton("📂 WireGuard .conf İçe Aktar");
    btn_import->setFixedHeight(40);
    btn_import->setCursor(Qt::PointingHandCursor);
    QPushButton* btn_vless = new QPushButton("🔗 Kendi Sunucum (vless:// yapıştır)");
    btn_vless->setFixedHeight(40);
    btn_vless->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* import_row = new QHBoxLayout();
    import_row->addWidget(btn_import);
    import_row->addWidget(btn_vless);
    l1->addLayout(import_row);
    QPushButton* btn_qr = new QPushButton("📱 Telefona Aktar (QR kod)");
    btn_qr->setFixedHeight(40);
    btn_qr->setCursor(Qt::PointingHandCursor);
    btn_qr->setToolTip("Bu bağlantıyı telefonuna ya da tabletine QR kodla aktar.");
    l1->addWidget(btn_qr);
    btn_tunnel_qr = btn_qr;
    btn_tunnel_toggle = new QPushButton();
    btn_tunnel_toggle->setFixedHeight(48);
    btn_tunnel_toggle->setCursor(Qt::PointingHandCursor);
    l1->addWidget(btn_tunnel_toggle);
    tunnel_status = new QLabel();
    tunnel_status->setWordWrap(true);
    l1->addWidget(tunnel_status);
    QLabel* warn = new QLabel("⚠️ Koruma: Tünel açıkken Elite kapanırsa ya da tünel koparsa listedeki siteler hiç açılmaz (Türkiye IP'n sızmasın diye). "
                              "Normale dönmek için tüneli buradan kapat. Tarayıcında \"Güvenli DNS\" açıksa kapat ya da \"Sistem varsayılanı\" seç; yoksa tarayıcı tüneli atlayabilir.");
    warn->setWordWrap(true);
    l1->addWidget(warn);
    tunnel_descs << warn;
    layout->addWidget(tunnel_blocks.last());

    QVBoxLayout* l2 = make_block("🌐 Tünelden Geçecek Siteler",
        "Alan adını yaz (www olmadan). Alt alan adları da otomatik dahil olur. Discord'u şimdilik ekleme: Elite'in DPI testi Discord'la yapılıyor, tünele girerse test yanılır.");
    tunnel_list = new QListWidget();
    tunnel_list->setFixedHeight(220);
    l2->addWidget(tunnel_list);
    QHBoxLayout* row = new QHBoxLayout();
    tunnel_input = new QLineEdit();
    tunnel_input->setPlaceholderText("Örn: reddit.com");
    tunnel_input->setFixedHeight(38);
    row->addWidget(tunnel_input);
    QPushButton* btn_add = new QPushButton("Ekle");
    QPushButton* btn_del = new QPushButton("🗑️ Kaldır");
    QPushButton* btn_reset = new QPushButton("↺ Varsayılan");
    for (QPushButton* b : { btn_add, btn_del, btn_reset }) { b->setFixedHeight(38); b->setCursor(Qt::PointingHandCursor); row->addWidget(b); }
    l2->addLayout(row);
    layout->addWidget(tunnel_blocks.last());
    tunnel_btns = { btn_import, btn_vless, btn_qr, btn_add, btn_del, btn_reset };

    connect(btn_qr, &QPushButton::clicked, this, [this]() { show_tunnel_qr(); });

    connect(btn_vless, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString text = QInputDialog::getText(this, "Kendi Sunucum", "vless:// bağlantısını yapıştır (Reality):", QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || text.isEmpty()) return;
        const WgConf c = parse_vless_url(text);
        if (!c.ok) { tunnel_status->setText("⚠️ " + c.error); return; }
        QDir().mkpath(tunnel_dir());
        QSaveFile f(tunnel_vless_path());
        if (!f.open(QIODevice::WriteOnly) || f.write(text.toUtf8()) < 0 || !f.commit()) { tunnel_status->setText("⚠️ Bağlantı kaydedilemedi."); return; }
        tunnel_status->setText("✅ Kendi sunucun eklendi: " + c.name);
        reload_tunnel_ui();
        // Tünel açık olarak ayarlıysa (ör. eski kurulumdan kalan ayar) ama bağlantı olmadığı için kurulamadıysa şimdi başlat
        if (tunnel_proc || tunnel_enabled()) start_tunnel();
    });

    auto save_domains = [this](const QStringList& d) {
        QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("tunnel_domains", d);
        reload_tunnel_ui();
        if (tunnel_proc) { stop_tunnel(false); start_tunnel(); } // yeni listeyle yeniden kur
    };
    connect(btn_add, &QPushButton::clicked, this, [this, save_domains]() {
        QString d = tunnel_input->text().trimmed().toLower();
        d.remove(QRegularExpression("^https?://")); d.remove(QRegularExpression("/.*$"));
        if (d.startsWith("www.")) d = d.mid(4);
        if (!valid_domain(d)) { tunnel_status->setText("⚠️ Geçersiz alan adı: " + tunnel_input->text()); return; }
        QStringList list = tunnel_domains();
        if (!list.contains(d)) list << d;
        tunnel_input->clear();
        save_domains(list);
    });
    connect(tunnel_input, &QLineEdit::returnPressed, btn_add, &QPushButton::click);
    connect(btn_del, &QPushButton::clicked, this, [this, save_domains]() {
        QListWidgetItem* it = tunnel_list->currentItem();
        if (!it) return;
        QStringList list = tunnel_domains();
        list.removeAll(it->text());
        save_domains(list);
    });
    connect(btn_reset, &QPushButton::clicked, this, [save_domains]() { save_domains(default_tunnel_domains()); });

    connect(btn_import, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "WireGuard yapılandırması seç",
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "WireGuard (*.conf)");
        if (path.isEmpty()) return;
        const WgConf c = parse_wg_conf(path);
        if (!c.ok) { tunnel_status->setText("⚠️ " + c.error); return; }
        QDir().mkpath(tunnel_dir());
        QFile::remove(tunnel_conf_path());
        if (!QFile::copy(path, tunnel_conf_path())) { tunnel_status->setText("⚠️ Dosya kopyalanamadı."); return; }
        QFile::remove(tunnel_vless_path()); // artık WireGuard kullanılacak
        tunnel_status->setText("✅ Bağlantı içe aktarıldı: " + c.name);
        reload_tunnel_ui();
        if (tunnel_proc || tunnel_enabled()) start_tunnel();
    });

    connect(btn_tunnel_toggle, &QPushButton::clicked, this, [this]() {
        if (tunnel_proc) stop_tunnel(true);
        else if (tunnel_enabled() && !load_tunnel_conf().ok) stop_tunnel(true); // bağlantısız "açık" ayarı kapat
        else start_tunnel();                                                    // kapalıysa ya da çalışmıyorsa (yeniden) kur
    });

    reload_tunnel_ui();
    // Açılışta: tünel açık bırakıldıysa geri kur (kurallar zaten duruyor, siteler bu arada açılmıyordu)
    // Profil geri yüklemesi (Discord testi) bitmeden kurulmaz: sanal ağ kartı ve DNS değişikliği testi bozabiliyor
    if (tunnel_enabled() && !is_self_test()) {
        tunnel_pending = true;
        reload_tunnel_ui();
        auto waited = std::make_shared<int>(0);
        QTimer* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this, t, waited]() {
            if (dashboard_widget && dashboard_widget->busy() && ++*waited < 60) return;
            t->deleteLater();
            tunnel_pending = false;
            if (!tunnel_proc && tunnel_enabled()) start_tunnel();
            reload_tunnel_ui();
        });
        t->start(1000);
    }

    main_l->addWidget(container);
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; } "
        "QScrollBar:vertical { border: none; background: transparent; width: 14px; margin: 0px; } "
        "QScrollBar::handle:vertical { background: #444444; min-height: 30px; border-radius: 2px; margin-left: 10px; margin-right: 0px; } "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }");
    scroll->setWidget(page);
    return scroll;
}

void TheTakanosu_Elite::reload_tunnel_ui() {
    if (!tunnel_list) return;
    tunnel_list->clear();
    tunnel_list->addItems(tunnel_domains());
    const WgConf c = load_tunnel_conf();
    tunnel_conf_label->setText(c.ok ? QString("Bağlantı: <b>%1</b>").arg(c.name.toHtmlEscaped()) : QString("Bağlantı: <i>henüz yok</i> — .conf içe aktar ya da kendi sunucunun vless:// bağlantısını yapıştır."));
    const bool on = tunnel_enabled();
    // Düğme ayara değil tünelin gerçek durumuna bakar: açık ayarlı ama motor çalışmıyorsa "AÇIK" yazmaz
    // (2026-10-14: temiz kurulumda eski ayar "açık" kalmıştı, bağlantı sonradan eklenince tünel hiç kurulmadı)
    const bool running = tunnel_proc != nullptr;
    const bool broken = on && !running && !tunnel_pending;
    btn_tunnel_toggle->setText(on && running ? "🛡️ Tünel AÇIK (kapatmak için tıkla)"
                               : on && tunnel_pending ? "⏳ Tünel başlatılıyor..."
                               : broken ? (c.ok ? "⚠️ Tünel çalışmıyor — yeniden başlatmak için tıkla" : "⚠️ Tünel açık ayarlı ama bağlantı yok — kapatmak için tıkla")
                               : "Tüneli Aç");
    btn_tunnel_toggle->setEnabled(on || c.ok);
    if (btn_tunnel_qr) btn_tunnel_qr->setEnabled(c.ok);
    style_tunnel_button(btn_tunnel_toggle, true, on && running);
    if (broken) {
        btn_tunnel_toggle->setStyleSheet("QPushButton { background-color: #e65100; color: white; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: none; } QPushButton:hover { background-color: #ef6c00; }");
        return;
    }
    if (!(on && running)) btn_tunnel_toggle->setStyleSheet(QSettings(takanosu_settings_path(), QSettings::IniFormat).value("is_dark_theme", true).toBool()
        ? "QPushButton { background-color: #333333; color: #e0e0e0; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #444444; } QPushButton:hover { background-color: #6a1b9a; color: white; } QPushButton:disabled { color: #666666; }"
        : "QPushButton { background-color: #f0f0f0; color: #1a1a1a; border-radius: 6px; font-family: 'Segoe UI Variable'; font-size: 14px; font-weight: bold; border: 1px solid #aaaaaa; } QPushButton:hover { background-color: #6a1b9a; color: white; } QPushButton:disabled { color: #999999; }");
}

void TheTakanosu_Elite::show_tunnel_qr() {
    const WgConf c = load_tunnel_conf();
    if (!c.ok) return;
    QFile f(c.vless ? tunnel_vless_path() : tunnel_conf_path());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { tunnel_status->setText("⚠️ Bağlantı dosyası okunamadı."); return; }
    const QString text = QString::fromUtf8(f.readAll()).trimmed();

    const bool dark = QSettings(takanosu_settings_path(), QSettings::IniFormat).value("is_dark_theme", true).toBool();
    QDialog dlg(this);
    dlg.setWindowTitle("Telefona Aktar");
    dlg.setStyleSheet(dark
        ? "QDialog { background-color: #1f1f1f; } QLabel { color: #e0e0e0; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QPushButton { background-color: #333333; color: white; border: 1px solid #444444; border-radius: 6px; padding: 8px 16px; font-family: 'Segoe UI Variable'; font-weight: bold; } QPushButton:hover { background-color: #6a1b9a; }"
        : "QDialog { background-color: #f5f5f5; } QLabel { color: #1a1a1a; font-family: 'Segoe UI Variable'; font-size: 13px; } "
          "QPushButton { background-color: #e0e0e0; color: #1a1a1a; border: 1px solid #aaaaaa; border-radius: 6px; padding: 8px 16px; font-family: 'Segoe UI Variable'; font-weight: bold; } QPushButton:hover { background-color: #6a1b9a; color: white; }");
    QVBoxLayout* l = new QVBoxLayout(&dlg);
    l->setSpacing(12);
    QLabel* how = new QLabel(c.vless
        ? QString("<b>%1</b><br><br>Telefonda <b>v2rayNG</b> (Android, GitHub: 2dust/v2rayNG) ya da <b>Hiddify</b> (Android/iOS) aç → <b>+</b> → <b>QR kodu tara</b>. "
                  "Sonra uygulama ayarlarından yalnız istediğin uygulamaları tünele al (Per-app proxy).").arg(c.name.toHtmlEscaped())
        : QString("<b>%1</b><br><br>Telefonda resmi <b>WireGuard</b> uygulamasını aç → <b>+</b> → <b>QR kodundan oluştur</b>.").arg(c.name.toHtmlEscaped()));
    how->setWordWrap(true);
    how->setMaximumWidth(420);
    l->addWidget(how);
    // QR, yayın anahtarı gibi korunur: bulanık gelir, "Göster" ile açılır
    // QR, yayın anahtarı gibi korunur: bulanık gelir, "Göster" ile açılır
    QLabel* img = new QLabel();
    img->setPixmap(QPixmap::fromImage(make_qr_image(text, 6)));
    img->setAlignment(Qt::AlignCenter);
    QGraphicsBlurEffect* blur = new QGraphicsBlurEffect(img);
    blur->setBlurRadius(28);
    blur->setBlurHints(QGraphicsBlurEffect::QualityHint);
    img->setGraphicsEffect(blur);
    l->addWidget(img);
    QLabel* warn = new QLabel("🔒 Bu kod sunucunun anahtarıdır: kimseye gösterme. Bu pencere ekran görüntüsünde, kayıtta ve yayında (OBS, Discord ekran paylaşımı) siyah görünür. "
                              "Kamera yoksa \"Kopyala\" ile panoya al, telefona kendine güvendiğin bir yoldan (ör. LocalSend) gönder; işin bitince panoyu temizle.");
    warn->setWordWrap(true);
    warn->setMaximumWidth(420);
    l->addWidget(warn);
    QHBoxLayout* row = new QHBoxLayout();
    QPushButton* btn_show = new QPushButton("👁 Göster");
    QPushButton* btn_copy = new QPushButton("📋 Kopyala");
    QPushButton* btn_close = new QPushButton("Kapat");
    row->addWidget(btn_show);
    row->addWidget(btn_copy);
    row->addWidget(btn_close);
    connect(btn_show, &QPushButton::clicked, &dlg, [blur, btn_show]() {
        const bool hidden = blur->isEnabled();
        blur->setEnabled(!hidden);
        btn_show->setText(hidden ? "🙈 Gizle" : "👁 Göster");
    });
    l->addLayout(row);
    connect(btn_copy, &QPushButton::clicked, &dlg, [text, btn_copy]() {
        QGuiApplication::clipboard()->setText(text);
        btn_copy->setText("✅ Panoya kopyalandı");
    });
    connect(btn_close, &QPushButton::clicked, &dlg, &QDialog::accept);
    // Ekran görüntüsü / kayıt / yayın bu pencereyi siyah görür (Windows 10 2004+; eskilerde yalnız bulanıklık kalır).
    // Pencere ekrana geldikten sonra uygulanır: exec()'ten önce winId() ile uygulamak, kapanınca ana pencerenin
    // sürüklenmesini kilitliyordu (2026-10-14, küçük bir test uygulamasıyla yeniden üretildi).
    QTimer::singleShot(0, &dlg, [&dlg]() { SetWindowDisplayAffinity(reinterpret_cast<HWND>(dlg.winId()), WDA_EXCLUDEFROMCAPTURE); });
    dlg.exec();
}

void TheTakanosu_Elite::apply_tunnel_theme(bool is_dark) {
    if (!tunnel_header) return;
    tunnel_header->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 24px; font-weight: bold; background: transparent;").arg(is_dark ? "white" : "#1a1a1a"));
    for (QFrame* b : tunnel_blocks) b->setStyleSheet(QString("QFrame#TargetBlock { background-color: %1; border-radius: 12px; border: 1px solid %2; }").arg(is_dark ? "#2b2b2b" : "#ffffff", is_dark ? "#3a3a3a" : "#aaaaaa"));
    for (QLabel* t : tunnel_titles) t->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 16px; font-weight: bold; border: none; background: transparent;").arg(is_dark ? "#e0e0e0" : "#1a1a1a"));
    for (QLabel* d : tunnel_descs) d->setStyleSheet(QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 13px; border: none; background: transparent;").arg(is_dark ? "#a0a0a0" : "#555555"));
    const QString strong = QString("color: %1; font-family: 'Segoe UI Variable'; font-size: 14px; border: none; background: transparent;").arg(is_dark ? "#e0e0e0" : "#1a1a1a");
    tunnel_conf_label->setStyleSheet(strong);
    tunnel_status->setStyleSheet(strong);
    tunnel_list->setStyleSheet(QString(
        "QListWidget { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 5px; font-family: 'Consolas', monospace; font-size: 13px; outline: none; } "
        "QListWidget::item { padding: 2px 5px; min-height: 22px; border-radius: 4px; } QListWidget::item:selected { background-color: #6a1b9a; color: white; }")
        .arg(is_dark ? "#1a1a1a" : "#f5f5f5", is_dark ? "#e0e0e0" : "#333333", is_dark ? "#444444" : "#cccccc"));
    tunnel_input->setStyleSheet(QString("QLineEdit { background-color: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding-left: 10px; font-family: 'Segoe UI Variable'; font-size: 14px; } QLineEdit:focus { border: 1px solid #6a1b9a; }")
        .arg(is_dark ? "#1a1a1a" : "#f5f5f5", is_dark ? "white" : "#333333", is_dark ? "#444444" : "#cccccc"));
    for (QPushButton* b : tunnel_btns) style_tunnel_button(b, is_dark, false);
    reload_tunnel_ui();
}

void TheTakanosu_Elite::start_tunnel() {
    const WgConf c = load_tunnel_conf();
    if (!c.ok) { tunnel_status->setText("⚠️ Önce bir bağlantı ekle (WireGuard .conf ya da vless://). " + c.error); return; }
    const QString exe = tunnel_dir() + "/sing-box.exe";
    if (!QFileInfo::exists(exe)) { tunnel_status->setText("⚠️ Tünel motoru bulunamadı (tunnel\\sing-box.exe). Uygulamayı yeniden kurun."); return; }
    if (tunnel_proc) stop_tunnel(false);

    const QStringList domains = tunnel_domains_with_check();
    QSaveFile cfg(tunnel_dir() + "/config.json");
    if (!cfg.open(QIODevice::WriteOnly) || cfg.write(build_singbox_config(c, domains)) < 0 || !cfg.commit()) {
        tunnel_status->setText("⚠️ Tünel ayarı yazılamadı.");
        return;
    }

    if (!tunnel_job) {
        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (job) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
            tunnel_job = job;
        }
    }

    tunnel_stopping = false;
    tunnel_proc = new QProcess(this);
    hide_console(tunnel_proc);
    tunnel_proc->setWorkingDirectory(tunnel_dir());
    tunnel_proc->setProcessChannelMode(QProcess::MergedChannels);
    QProcess* proc = tunnel_proc;
    connect(proc, &QProcess::finished, this, [this, proc]() {
        const QString tail = oem_text(proc->readAll()).trimmed().right(400);
        proc->deleteLater();
        if (tunnel_proc != proc) return;
        tunnel_proc = nullptr;
        reload_tunnel_ui();
        if (tunnel_stopping) return;
        tunnel_status->setText("⚠️ Tünel beklenmedik şekilde kapandı. Koruma açık: listedeki siteler şu an açılmıyor." + (tail.isEmpty() ? QString() : "\n" + tail));
        if (tunnel_enabled() && tunnel_restarts++ < 3) QTimer::singleShot(3000, this, [this]() { if (!tunnel_proc && tunnel_enabled()) start_tunnel(); });
    });
    tunnel_proc->start(exe, { "run", "-c", tunnel_dir() + "/config.json" });
    if (!tunnel_proc->waitForStarted(4000)) {
        tunnel_status->setText("⚠️ Tünel motoru başlatılamadı: " + tunnel_proc->errorString());
        tunnel_stopping = true;
        tunnel_proc->deleteLater();
        tunnel_proc = nullptr;
        reload_tunnel_ui();
        return;
    }
    if (tunnel_job) {
        HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(tunnel_proc->processId()));
        if (h) { AssignProcessToJobObject(static_cast<HANDLE>(tunnel_job), h); CloseHandle(h); }
    }

    const bool was_enabled = tunnel_enabled();
    QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("tunnel_enabled", true);
    reload_tunnel_ui();
    tunnel_status->setText("⏳ Tünel kuruluyor: " + c.name + "...");

    // DNS kuralları (NRPT): listedeki alan adları 127.0.0.53'teki sing-box'a sorulur
    QProcess* ps = new QProcess(this);
    hide_console(ps);
    connect(ps, &QProcess::finished, this, [this, ps, was_enabled](int code) {
        ps->deleteLater();
        if (code != 0) { tunnel_status->setText("⚠️ DNS kuralları eklenemedi (NRPT). Tünel çalışmayabilir."); return; }
        if (!was_enabled && dashboard_widget) dashboard_widget->reapply_engine(); // motor listesinden tünel sitelerini çıkar
        QTimer::singleShot(4000, this, &TheTakanosu_Elite::check_tunnel_exit);
    });
    ps->start(powershell_exe(), powershell_args(nrpt_script(domains)));
}

void TheTakanosu_Elite::stop_tunnel(bool remove_dns_rules) {
    if (tunnel_proc) {
        tunnel_stopping = true;
        tunnel_proc->kill();
        tunnel_proc->waitForFinished(3000);
    }
    if (!remove_dns_rules) return; // yeniden kurulum: kurallar ve ayar yerinde kalır
    tunnel_restarts = 0;
    QSettings(takanosu_settings_path(), QSettings::IniFormat).setValue("tunnel_enabled", false);
    reload_tunnel_ui();
    tunnel_status->setText("⏳ Tünel kapatılıyor...");
    QProcess* ps = new QProcess(this);
    hide_console(ps);
    connect(ps, &QProcess::finished, this, [this, ps](int code) {
        ps->deleteLater();
        tunnel_status->setText(code == 0 ? "Tünel kapalı. Siteler normal bağlantıyla açılıyor." : "⚠️ DNS kuralları silinemedi. Bilgisayarı yeniden başlatmadan önce tekrar deneyin.");
        if (dashboard_widget) dashboard_widget->reapply_engine(); // siteleri motor listesine geri koy
    });
    ps->start(powershell_exe(), powershell_args(nrpt_script({})));
}

void TheTakanosu_Elite::check_tunnel_exit() {
    if (!tunnel_proc) return;
    QProcess* p = new QProcess(this);
    hide_console(p);
    connect(p, &QProcess::finished, this, [this, p](int code) {
        const QString out = QString::fromUtf8(p->readAllStandardOutput()).trimmed();
        p->deleteLater();
        if (!tunnel_proc) return;
        static const QRegularExpression cc("^[A-Z]{2}$");
        const WgConf c = load_tunnel_conf();
        if (code == 0 && cc.match(out).hasMatch()) {
            tunnel_restarts = 0;
            tunnel_status->setText(out == "TR"
                ? "⚠️ Tünel çalışıyor ama çıkış hâlâ Türkiye görünüyor. Bağlantı dosyasını kontrol et."
                : QString("✅ Tünel açık · %1 · çıkış ülkesi: %2").arg(c.name, out));
        }
        else tunnel_status->setText("⚠️ Tünel kuruldu ama çıkış doğrulanamadı (sunucuya ulaşılamıyor olabilir). Koruma açık.");
    });
    p->start(sys_tool("curl.exe"), { "-s", "--ssl-no-revoke", "--max-time", "12", QString("https://%1/country-iso").arg(TUNNEL_CHECK_DOMAIN) });
}

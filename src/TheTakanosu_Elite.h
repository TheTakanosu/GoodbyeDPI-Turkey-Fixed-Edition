#pragma once
#pragma warning(disable : 4996)

#include <QtWidgets/QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QStackedWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QScrollBar>
#include <QPoint>
#include <QRect>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QComboBox>
#include <QProgressBar>
#include <QProcess>
#include <QPlainTextEdit>
#include <QStringList>
#include <QLineEdit>
#include <QListWidget>
#include <QEvent>
#include <QCheckBox>
#include <QMap>
#include <functional>

// 🚀 Tek bir DPI profilinin tanımı. Parametreler kayıt defterinde ham metin olarak DEĞİL,
// sadece "id" olarak saklanır; böylece yönetici yetkisiyle çalışan motora dışarıdan komut sokulamaz.
struct DpiProfile {
    QString id;
    QString name;
    bool is_zapret;
    QStringList args;
    QString desc;
    bool is_dns = false;
};

// Şifreli DNS modu (kaldırıcı da "--restore-dns" ile kullanır)
bool takanosu_dns_mode_active();
bool takanosu_restore_dns();

class PowerToysDashboard : public QWidget {
    Q_OBJECT
public:
    PowerToysDashboard(QWidget* parent = nullptr);
    ~PowerToysDashboard();
    void update_theme(bool is_dark);
    void stop_engine(bool unload_driver);
    void try_single_profile(const QString& id);
    void run_self_test(const QString& report_path);
    bool is_engine_running() const;

    QFrame* block_quick;
    QFrame* block_utils;
    QLabel* title_quick;
    QLabel* title_utils;
    QLabel* lbl_iss;
    QList<QPushButton*> utility_btns;

    QLabel* lbl_active_mod;
    QComboBox* combo_iss;
    QPushButton* btn_start;
    QProgressBar* progress_bar;
    QLabel* lbl_status;
    QPlainTextEdit* log_console;

signals:
    void engine_state_changed(bool active, const QString& profile_name);
    void notify(const QString& title, const QString& text);

public slots:
    void start_autonomous_system();
    void kill_dpi_service();

private slots:
    void restore_last_session();
    void util_flush_dns();
    void util_fast_dns();
    void util_restart_adapter();
    void util_ping_test();
    void util_export_logs();
    void util_safe_net_check();
    void util_winsock_reset();
    void util_diagnostic_report();
    void game_check();
    void health_check();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void update_column_layout();

private:
    enum class UiState { Idle, Busy, Success, Fail, Stopped };

    void log(const QString& tag, const QString& text);
    void set_ui_state(UiState state, const QString& status, const QString& mod);
    void set_progress(int value, const QString& color);
    void set_start_button(bool enabled, const QString& text);
    void apply_combo_style(QComboBox* combo);

    void run_async(const QString& program, const QStringList& args, int timeout_ms,
                   std::function<void(int exit_code, const QString& output)> done);
    void check_internet(std::function<void(bool online)> done);
    void test_dpi_bypass(std::function<void(bool passed, const QString& detail)> done);
    void probe_discord(int attempts, std::function<void(int ok, int total)> done);
    void detect_isp(std::function<void(int isp_index, const QString& isp_text)> done);

    bool start_engine(const DpiProfile& profile, QString* error);
    void start_scan(int isp_index);
    void build_and_run_scan(int isp_index);
    void run_auto_test_step();
    void verify_current_step();
    void on_bypass_success(const DpiProfile& profile);
    void on_scan_failed();
    void fail_no_internet(const QString& log_text);
    void apply_dns_mode(std::function<void(bool ok, const QString& error)> done);
    void revert_dns_mode();
    void begin_profile(const DpiProfile& profile, std::function<void(bool ok, const QString& error)> ready);
    void restore_wait_step();
    void self_test_next();
    void finish_diagnostic_report();
    void on_game_started(const QString& game);
    void on_game_stopped();
    void reset_game_state();

    bool is_dark_mode;
    bool is_busy;
    bool restoring;
    bool single_try;
    bool legacy_cleaned;
    int scan_generation;
    DpiProfile pending_restore;
    QList<DpiProfile> st_profiles;
    int st_index;
    QStringList st_report;
    QString st_path;
    bool st_in_app;
    bool game_running;
    bool game_dns_temp;
    QString game_name;
    QString game_paused_profile;
    int health_fail;
    UiState ui_state;
    int current_auto_test_index;
    int restore_retry;
    QList<DpiProfile> candidates;
    QString active_profile_id;

    QList<QProcess*> engine_procs;
    QString engine_output;
    void* engine_job;

    QVBoxLayout* outer_layout;
    QWidget* inner_container;
    QBoxLayout* flex_layout;
    QWidget* col_left;
    QWidget* col_right;
};

class TheTakanosu_Elite : public QMainWindow {
    Q_OBJECT
public:
    TheTakanosu_Elite(QWidget* parent = nullptr);
    ~TheTakanosu_Elite();
    void run_self_test(const QString& report_path) { dashboard_widget->run_self_test(report_path); }

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override; // 🚀 SİHİRLİ DİNLEYİCİMİZ

private slots:
    void toggle_maximize();
    void toggle_hamburger();
    void sync_sidebar_height();
    void set_theme(bool is_dark);
    void toggle_startup_setting();
    void toggle_tray_setting();
    void load_blacklist();
    void add_to_blacklist();
    void remove_from_blacklist();
    void filter_blacklist(const QString& text);
    void check_updates(bool manual = false);
    void on_engine_state_changed(bool active, const QString& profile_name);
    void save_disabled_profiles();

private:
    QSystemTrayIcon* tray_icon;
    QMenu* tray_menu;
    QAction* act_toggle_engine;
    void setup_tray_icon();
    void migrate_legacy_startup();
    static bool startup_task_exists();
    static bool create_startup_task();
    static bool delete_startup_task();
    void setupUi();
    void add_sidebar_btn(const QString& icon, const QString& text, int index);

    QScrollArea* create_powertoys_dashboard();
    QWidget* create_mod_settings_page();
    QWidget* create_dpi_modes_page();
    void apply_modes_theme(bool is_dark);
    QWidget* create_tools_page();
    QWidget* create_stats_page();

    QListWidget* blacklist_widget;
    QLineEdit* input_new_domain;
    QLineEdit* search_domain;
    QLabel* tools_header;
    QFrame* blacklist_block;
    QLabel* bl_title;
    QLabel* bl_desc;

    QLabel* stats_header;
    QFrame* stats_block;
    QLabel* stats_title;
    QLabel* stat_1_val;
    QLabel* stat_2_val;
    QLabel* stat_1_desc;
    QLabel* stat_2_desc;

    PowerToysDashboard* dashboard_widget;
    QLabel* modes_header;
    QComboBox* combo_engine_mode;
    QList<QFrame*> modes_blocks;
    QList<QLabel*> modes_titles;
    QList<QLabel*> modes_descs;
    QList<QCheckBox*> mode_checks;
    QList<QPushButton*> mode_try_btns;
    QMap<QString, QLabel*> mode_status_labels;
    QPushButton* btn_dark_theme;
    QPushButton* btn_light_theme;
    QLabel* mod_settings_header;
    QFrame* theme_block;
    QLabel* theme_title;
    QFrame* lang_block;
    QLabel* lang_title;
    QComboBox* combo_lang;
    QFrame* sys_block;
    QLabel* sys_title;
    QPushButton* btn_toggle_startup;
    QPushButton* btn_toggle_tray;
    QPushButton* btn_toggle_game;

    QRect m_safe_geometry;
    bool m_is_overlay_open;
    bool m_is_manually_collapsed;
    QWidget* central_widget;
    QVBoxLayout* main_layout;
    QWidget* title_bar;
    QLabel* logo_label;
    QLabel* title_label;
    QPushButton* btn_min;
    QPushButton* btn_max;
    QPushButton* btn_close;
    QWidget* body_widget;
    QHBoxLayout* body_layout;
    QWidget* sidebar_placeholder;
    QWidget* main_panel;
    QVBoxLayout* main_panel_layout;
    QStackedWidget* stacked_widget;
    QFrame* sidebar;
    QVBoxLayout* sidebar_layout;
    QPushButton* btn_hamburger;
    QList<QPushButton*> sidebar_btns;
};
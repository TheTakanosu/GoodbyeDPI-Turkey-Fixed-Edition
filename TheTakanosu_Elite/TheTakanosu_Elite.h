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

class PowerToysDashboard : public QWidget {
    Q_OBJECT
public:
    PowerToysDashboard(QWidget* parent = nullptr);
    void update_theme(bool is_dark);

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
    QProcess* motor_process;
    QProcess* ping_process;

private slots:
    void start_autonomous_system();
    void on_service_removed(int exitCode);
    void on_service_installed(int exitCode);
    void on_service_started(int exitCode);
    void verify_connection(int exitCode);
    void verify_dpi_connection(int exitCode);
    void run_auto_test_step();
    void restore_last_session();
    void util_flush_dns();
    void kill_dpi_service();
    void util_fast_dns();
    void util_restart_adapter();
    void util_ping_test();
    void util_export_logs();
    void util_safe_net_check();
    void util_winsock_reset();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    bool is_auto_detect_mode;
    bool is_dark_mode;
    int current_auto_test_index;
    QStringList auto_test_params;
    QStringList auto_test_names;
    QString current_run_param;

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
    void check_updates();

private:
    QSystemTrayIcon* tray_icon;
    QMenu* tray_menu;
    void setup_tray_icon();
    void setupUi();
    void add_sidebar_btn(const QString& icon, const QString& text, int index);

    QScrollArea* create_powertoys_dashboard();
    QWidget* create_mod_settings_page();
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
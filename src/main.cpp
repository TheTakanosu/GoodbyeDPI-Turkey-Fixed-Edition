#include "TheTakanosu_Elite.h"
#include <QtWidgets/QApplication>
#include <windows.h>
#include <QProcess>
#include <QTimer>
#include <cstring>

int main(int argc, char *argv[])
{
    // 🧪 Öz-test modu: tek kopya kilidini atla, diğer açık kopyaları kapat, tüm profilleri test edip rapor yaz
    bool self_test = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) self_test = true;
        // Kaldırıcı çağırır: şifreli DNS modu açıksa önceki DNS ayarlarını geri yükle ve çık
        if (strcmp(argv[i], "--restore-dns") == 0) {
            QCoreApplication core(argc, argv);
            return takanosu_restore_dns() ? 0 : 1;
        }
    }
    if (self_test) {
        QApplication app(argc, argv);
        app.setQuitOnLastWindowClosed(false);
        QProcess::execute("taskkill.exe", { "/F", "/FI", QString("PID ne %1").arg(GetCurrentProcessId()), "/IM", "TheTakanosu_Elite.exe" });
        const QStringList args = app.arguments();
        const int at = args.indexOf("--selftest");
        const QString report = (at + 1 < args.size()) ? args[at + 1] : QCoreApplication::applicationDirPath() + "/selftest_report.txt";
        TheTakanosu_Elite window;
        QTimer::singleShot(500, &window, [&window, report]() { window.run_self_test(report); });
        return app.exec();
    }

    // Tek kopya kilidi: iki kopya aynı anda iki DPI motoru çalıştırıp birbirini bozmasın.
    // Uygulama zaten açıksa (ör. tepside), mevcut pencereyi öne getirip çık.
    HANDLE instance_mutex = CreateMutexW(nullptr, FALSE, L"Local\\TheTakanosu_Elite_SingleInstance");
    if (instance_mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        AllowSetForegroundWindow(ASFW_ANY);
        PostMessageW(HWND_BROADCAST, RegisterWindowMessageW(L"TheTakanosu_Elite_Show"), 0, 0);
        CloseHandle(instance_mutex);
        return 0;
    }

    QApplication app(argc, argv);
    // Tepsi uygulaması: pencere gizliyken açılan bir mesaj kutusu kapanınca uygulama kapanmasın
    app.setQuitOnLastWindowClosed(false);

    TheTakanosu_Elite window;
    // Görev Zamanlayıcı açılışta "--tray" ile başlatır: pencere açılmadan sadece tepside çalışır
    if (!app.arguments().contains("--tray")) {
        window.show();
    }

    const int code = app.exec();
    if (instance_mutex) CloseHandle(instance_mutex);
    return code;
}

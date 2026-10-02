// Kara liste fonksiyonlarının testi (uygulamayı açmadan, geçici klasörde gerçek dosyalarla).
// Derleme ve çalıştırma: test\calistir.cmd
#include "../src/kara_liste.h"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  BASARISIZ: %s (satir %d)\n", #cond, __LINE__); ++failures; } } while (0)

static void write_raw(const QString& path, const QByteArray& data) {
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(data);
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using namespace kara_liste;

    std::printf("1) v1.1.0'dan yukseltme: yalniz kullanicinin ekledigi siteler tasinir\n");
    {
        QTemporaryDir tmp; const QString d = tmp.path();
        // v1.1.0 varsayılan listesi: discord.com + v1.2.0'da bilerek çıkarılan bir site
        write_raw(legacy_default_path(d), "discord.com\r\ncikarilan-site.com\r\n");
        // Yeni gelen liste
        write_raw(shipped_path(d), "discord.com\r\nyeni-site.com\r\n");
        // Kurulumun yedeklediği eski liste: varsayılanlar + kullanıcının 2 sitesi (biri büyük harf) + çöp satır + tekrar
        write_raw(legacy_backup_path(d), "discord.com\r\ncikarilan-site.com\r\nroblox.com\r\nOrnek.Org\r\n\r\n  \r\nbu bir site degil\r\nroblox.com\r\n");
        const int moved = migrate_legacy(d);
        const QStringList custom = read(custom_path(d));
        CHECK(moved == 2);
        CHECK(custom == QStringList({ "roblox.com", "ornek.org" }));
        CHECK(!custom.contains("cikarilan-site.com")); // bilerek çıkarılan geri gelmemeli
        CHECK(!QFileInfo::exists(legacy_backup_path(d))); // yedek silinmeli
        CHECK(custom_has_entries(d));
        // İkinci çalıştırma hiçbir şey yapmamalı
        CHECK(migrate_legacy(d) == 0);
        CHECK(read(custom_path(d)).size() == 2);
    }

    std::printf("2) Yedek yoksa: ozel-liste.txt bos olarak olusur\n");
    {
        QTemporaryDir tmp; const QString d = tmp.path();
        write_raw(shipped_path(d), "discord.com\n");
        CHECK(migrate_legacy(d) == 0);
        CHECK(QFileInfo::exists(custom_path(d)));
        CHECK(!custom_has_entries(d));
    }

    std::printf("3) Var olan ozel listeyle birlestirme: tekrar eklenmez, sira korunur\n");
    {
        QTemporaryDir tmp; const QString d = tmp.path();
        write_raw(shipped_path(d), "discord.com\n");
        write_raw(custom_path(d), "a-site.com\nroblox.com\n");
        write_raw(legacy_backup_path(d), "roblox.com\nb-site.com\n");
        CHECK(migrate_legacy(d) == 1);
        CHECK(read(custom_path(d)) == QStringList({ "a-site.com", "roblox.com", "b-site.com" }));
    }

    std::printf("4) Eski varsayilan liste dosyasi eksikse: hazir listede olmayanlar tasinir (guvenli taraf)\n");
    {
        QTemporaryDir tmp; const QString d = tmp.path();
        write_raw(shipped_path(d), "discord.com\n");
        write_raw(legacy_backup_path(d), "discord.com\nroblox.com\n");
        CHECK(migrate_legacy(d) == 1);
        CHECK(read(custom_path(d)) == QStringList({ "roblox.com" }));
    }

    std::printf("5) read: olmayan dosya ok=true bos; write atomik ve okunabilir\n");
    {
        QTemporaryDir tmp; const QString d = tmp.path();
        bool ok = false;
        CHECK(read(d + "/yok.txt", &ok).isEmpty() && ok);
        CHECK(write(d + "/x.txt", { "a.com", "b.com" }));
        CHECK(read(d + "/x.txt") == QStringList({ "a.com", "b.com" }));
        CHECK(write(d + "/x.txt", {}));
        CHECK(read(d + "/x.txt").isEmpty());
    }

    std::printf("6) Klasor kilitliyse (yazilamiyorsa) yedek silinmez\n");
    {
        QTemporaryDir tmp; const QString d = tmp.path();
        write_raw(shipped_path(d), "discord.com\n");
        write_raw(legacy_backup_path(d), "roblox.com\n");
        QDir().mkpath(custom_path(d)); // ozel-liste.txt adında KLASÖR: dosya yazılamaz
        CHECK(migrate_legacy(d) == 0);
        CHECK(QFileInfo::exists(legacy_backup_path(d)));
    }

    std::printf("7) valid_domain\n");
    CHECK(valid_domain("discord.com"));
    CHECK(valid_domain("cdn.discordapp.com"));
    CHECK(valid_domain("xn--bcher-kva.com"));
    CHECK(!valid_domain("discord"));
    CHECK(!valid_domain("disc ord.com"));
    CHECK(!valid_domain("-a.com"));
    CHECK(!valid_domain("a.com\";calc"));

    std::printf(failures == 0 ? "\nTUM TESTLER GECTI\n" : "\n%d TEST BASARISIZ\n", failures);
    return failures == 0 ? 0 : 1;
}

#pragma once
// ==========================================
// 📜 KARA LİSTELER
// turkey-blacklist.txt  : uygulamayla gelen liste; her güncellemede kurulum paketi üzerine yazar.
// ozel-liste.txt        : kullanıcının eklediği siteler; kurulum paketinde YOK, güncellemede korunur.
// (v1.1.0'da kullanıcının eklediği siteler turkey-blacklist.txt'ye yazılıyor ve güncellemede kayboluyordu.)
//
// Fonksiyonlar liste klasörünü parametre olarak alır; böylece uygulamadan bağımsız test edilebilir
// (bkz. test/kara_liste_test.cpp).
// ==========================================
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace kara_liste {

inline QString shipped_path(const QString& dir) { return dir + "/turkey-blacklist.txt"; }
inline QString custom_path(const QString& dir) { return dir + "/ozel-liste.txt"; }
inline QString legacy_backup_path(const QString& dir) { return dir + "/eski-liste.txt"; }
inline QString legacy_default_path(const QString& dir) { return dir + "/eski-varsayilan-liste.txt"; }

inline bool valid_domain(const QString& d) {
    static const QRegularExpression re("^(?=.{3,253}$)([a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\\.)+[a-z0-9-]{2,63}$");
    return re.match(d).hasMatch();
}

// Satır satır okur (boş satırları atlar). Dosya yoksa boş liste ve ok=true; açılamazsa ok=false.
inline QStringList read(const QString& path, bool* ok = nullptr) {
    QStringList lines;
    QFile f(path);
    if (!f.exists()) { if (ok) *ok = true; return lines; }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { if (ok) *ok = false; return lines; }
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty()) lines << line;
    }
    if (ok) *ok = true;
    return lines;
}

// Yarıda kesilirse eski dosya bozulmadan kalsın diye geçici dosyaya yazıp yerine koyar
inline bool write(const QString& path, const QStringList& lines) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    for (const QString& l : lines) out << l << "\n";
    out.flush();
    return f.commit();
}

inline bool custom_has_entries(const QString& dir) {
    return !read(custom_path(dir)).isEmpty();
}

// v1.1.0'dan yükseltme: kurulum paketi eski listeyi eski-liste.txt olarak yedekler. Yedekte olup v1.1.0'ın
// VARSAYILAN listesinde (eski-varsayilan-liste.txt) olmayan alan adları kullanıcının kendi eklediğidir;
// bunlar ozel-liste.txt'ye taşınır. Eski varsayılan listeden bilerek çıkarılan siteler geri gelmez.
// Döndürür: taşınan site sayısı.
inline int migrate_legacy(const QString& dir) {
    int moved = 0;
    if (QFileInfo::exists(legacy_backup_path(dir))) {
        bool ok_old = false, ok_custom = false;
        const QStringList old = read(legacy_backup_path(dir), &ok_old);
        QStringList custom = read(custom_path(dir), &ok_custom);
        if (!ok_old || !ok_custom) return 0; // okunamadıysa yedeğe dokunma, bir sonraki açılışta tekrar denenir

        const QStringList known_old = read(legacy_default_path(dir));
        const QStringList shipped = read(shipped_path(dir));
        QSet<QString> skip(known_old.begin(), known_old.end());
        skip.unite(QSet<QString>(shipped.begin(), shipped.end()));
        QSet<QString> have(custom.begin(), custom.end());

        for (const QString& raw : old) {
            const QString d = raw.toLower();
            if (!valid_domain(d) || skip.contains(d) || have.contains(d)) continue;
            custom << d;
            have.insert(d);
            ++moved;
        }
        if (!write(custom_path(dir), custom)) return 0;
        QFile::remove(legacy_backup_path(dir));
    }
    // Dosya her zaman var olsun: kurulum paketi "ozel-liste.txt yoksa eski sürümden geliyordur" diye bakar
    if (!QFileInfo::exists(custom_path(dir))) write(custom_path(dir), {});
    return moved;
}

} // namespace kara_liste

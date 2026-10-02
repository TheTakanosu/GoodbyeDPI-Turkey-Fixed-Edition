# Değişiklik günlüğü

## v1.2.0 (yayınlanmadı)

**Yeni**
- **Zapret motoru:** GoodbyeDPI'ın yanına 4 Zapret profili. Discord sesli sohbetini (UDP) ve QUIC'i de kapsar.
- **🧬 DPI Modları sayfası:** 13 profilin hepsini görün, tek tek deneyin veya kapatın.
- **Otomatik Oyun Modu:** Anti-cheat'li bir oyun açıldığında motor ve sürücü tamamen kapanır; Discord, sürücü gerektirmeyen Şifreli DNS ile devam eder. Oyun kapanınca eski profile dönülür.
- **Şifreli DNS Modu:** Sürücüsüz, Windows 11'in DoH özelliğiyle. Kapatıldığında DNS ayarlarınız birebir geri yüklenir.
- **Nöbetçi:** 5 dakikada bir ve ağ yeniden bağlandığında erişimi kontrol eder, gerekirse yeniden tarar.
- **🩺 Tanı Raporu:** Bütün profilleri test eder, sonucu panoya kopyalar.
- Kalıcı log dosyası (`%LOCALAPPDATA%\TheTakanosu_Elite\takanosu_log.txt`).

**Düzeltmeler**
- **Discord testi düzeltildi.** Eski `ping` testi engel varken de "başarılı" diyordu (Vodafone/Kablonet şikâyetlerinin kökü). Artık HTTPS ile **3 tur × 2 adres** test ediliyor, hepsi geçmeden profil seçilmiyor.
- **Başlangıç:** Windows yönetici izni isteyen programları açılışta sessizce engelliyordu. Artık Görev Zamanlayıcı kullanılıyor, açılışta UAC çıkmıyor.
- **Anti-cheat uyumu:** Motor artık Windows servisi olarak sürekli çalışmıyor. Uygulamaya bağlı alt süreç olarak çalışıyor, uygulama kapanınca o da kapanıyor.
- **Ayarlar** kayıt defteri yerine `settings.ini` dosyasında (bazı bilgisayarlarda Görev Zamanlayıcı'dan açılan uygulama ayarları göremiyordu).
- **Güncelleyici** GitHub Release'lerini kullanıyor ve indirilen dosyanın SHA-256 özetini doğruluyor.

**Güvenlik**
- Ayarlarda artık sadece profil kimliği saklanıyor. v1.1.0'da ayar dosyasındaki ham parametreler yönetici yetkisiyle çalıştırılıyordu (yetki yükseltme açığı).
- Sistem araçları mutlak yolla çağrılıyor (sahte exe ile ele geçirmeye karşı).
- Web bağlantıları tarayıcıyı yönetici yetkisi vermeden açıyor.

## v1.1.0 (2026-05-21)
- Qt/C++ arayüzlü uygulama: koyu tema, otomatik ISS algılama, sistem araçları, uygulama içi güncelleme.

## v1.0.1 (2026-05-03)
- Türkiye için düzeltilmiş GoodbyeDPI yapılandırma paketi (zip).

## v1.0.0 (2026-05-02)
- İlk kararlı sürüm (zip).

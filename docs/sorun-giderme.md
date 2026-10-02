# Sorun giderme

## Tanı raporu

Bir sorun bildirmeden önce lütfen tanı raporu oluşturun. Uygulama bütün profilleri sırayla dener (yaklaşık 2 dakika, bu sırada internetiniz birkaç kez kısa süreliğine kopabilir) ve sonuçları yazar. Raporda yalnızca ISS adınız, Windows sürümünüz ve test sonuçları bulunur. IP adresiniz yazılmaz.

1. Uygulamayı açın, **🩺 Tanı Raporu Oluştur (Sorun Bildir)** düğmesine basın.
2. Rapor hazırlanınca **panoya kopyalanır**. Ayrıca `%LOCALAPPDATA%\TheTakanosu_Elite\tani_raporu.txt` dosyasına kaydedilir.
3. **GitHub'da Paylaş** düğmesi [Tartışmalar](https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/discussions) sayfasını açar. Raporu oraya ya da [yeni bir hata bildirimine](https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/issues/new/choose) yapıştırın (Ctrl+V).

## Discord açılmıyor

1. Tepsideki simgeden uygulamayı açın, **OTONOM SİSTEMİ BAŞLAT** ile taramayı yeniden çalıştırın.
2. Olmazsa **🧬 DPI Modları** sayfasından ✅ işaretli profilleri tek tek **Şimdi Dene** ile deneyin.
3. Hâlâ olmuyorsa tanı raporu gönderin.

## Bir oyun açılmıyor / anti-cheat hatası

**Otomatik Oyun Modu**'nun açık olduğundan emin olun (Ayarlar). Oyun listede yoksa tepsi menüsünden **🎮 DPI'ı Durdur (Oyun Modu)** ile motoru elle kapatın ve oyunun adını bir hata bildirimiyle bize iletin.

## İnternetim tamamen gitti / DNS bozuldu

Şifreli DNS Modu açıkken bilgisayar beklenmedik şekilde kapandıysa DNS ayarları yarım kalmış olabilir. Yönetici olarak açılmış bir komut isteminde şunu çalıştırın:

```
"C:\Program Files (x86)\TheTakanosu Elite\TheTakanosu_Elite.exe" --restore-dns
```

Bu komut, uygulamanın yedeklediği DNS ayarlarınızı geri yükler.

## Ayarlar bozuldu

`%LOCALAPPDATA%\TheTakanosu_Elite\settings.ini` dosyasını silin. Uygulama bir sonraki açılışta varsayılan ayarlarla yeniden oluşturur.

## Log dosyası

Uygulamanın kalıcı logu: `%LOCALAPPDATA%\TheTakanosu_Elite\takanosu_log.txt`

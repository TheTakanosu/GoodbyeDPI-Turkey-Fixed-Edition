# Proton VPN'den ücretsiz WireGuard yapılandırması almak

Tünel Modu, seçtiğiniz siteleri (Instagram, TikTok, YouTube, X, Discord...) bir WireGuard tünelinden geçirir; oyunlar ve geri kalan her şey doğrudan bağlanır. Bunun için bir WireGuard yapılandırma dosyası (`.conf`) gerekir. En kolay ücretsiz kaynak Proton VPN'dir.

> Proton'un ücretsiz planında hız sınırı ve kayıt tutma yoktur. Kısıtlar: tek cihaz, yalnız ücretsiz sunucular.

## Adımlar

1. [account.protonvpn.com](https://account.protonvpn.com) adresinde ücretsiz bir Proton hesabı açın ya da mevcut Proton (Mail) hesabınızla giriş yapın.
   Site açılmıyorsa Elite'i açın: Proton adresleri varsayılan listede, engel aşılır.
2. Sol menüden **Downloads (İndirmeler)** sayfasına gidin, aşağıdaki **WireGuard configuration** bölümüne inin.
3. **Device/certificate name:** cihazı tanıyacağınız bir ad yazın (ör. `Elite-PC`).
4. **Platform:** `Windows`.
5. **VPN options:** NAT-PMP kapalı kalsın. VPN Accelerator açık kalabilir.
6. **Sunucu:** "Use the best server" yanındaki **Create** düğmesine basın ya da **Free server configs** listesinden bir sunucu seçin (yükü düşük olanı tercih edin).
7. Açılan pencerede **Download** deyin. `wg-XX-FREE-N.conf` adlı dosya inecek.

Dosyayı Elite'te **Tünel → Bağlantı ekle** ile içe aktarın (sürükle bırak da olur).

## Bilmeniz gerekenler

- **Dosyada özel anahtarınız var.** Kimseyle paylaşmayın, ekran görüntüsünü göndermeyin. Paylaşırsanız aynı sayfadan yapılandırmayı iptal edip yenisini oluşturun.
- **Süre:** Yapılandırma 1 yıl geçerlidir. Süresi dolmadan aynı sayfadaki **Extend (Uzat)** ile uzatın.
- **Ücretsiz ülkeler:** ABD, Hollanda, İsviçre, Japonya, Kanada, Meksika, Norveç, Polonya, Romanya, Singapur (Proton listeyi değiştirebilir). Türkiye'ye en yakın ve hızlı seçenek genelde Hollanda.
- **Tek ülkede kalın:** Hesaplarınıza hep aynı ülkeden bağlanmak, ülke değiştirip durmaktan daha az şüphe çeker.
- **Ücretli seçenekler:** Mullvad ve Proton'un ücretli planları da WireGuard `.conf` verir; aynı şekilde içe aktarılır.

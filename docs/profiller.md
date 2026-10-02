# DPI profilleri

The Takanosu Elite'te 13 profil var. **Otomatik taramada** uygulama ISS'nizi tespit eder, profilleri o ISS'ye uygun sırayla dener ve Discord'a **3 tur × 2 adresin hepsi** ulaşan ilk profili seçer. Tek bir denemeyle karar verilmez, çünkü bazı ISS'ler (ör. Türk Telekom) engeli aralıklı uygular ve tek deneme şans eseri geçebilir.

Profilleri tek tek denemek veya kapatmak için: **🧬 DPI Modları** sayfası.

## Profil listesi

| Profil | Motor | Ne yapar | Kime uygun |
|---|---|---|---|
| **Evrensel Otonom Joker (-5)** | GoodbyeDPI | Sadece `-5`. DNS'e dokunmaz | DNS'i elle ayarlı olanlar (ör. 8.8.8.8). En hafif profil |
| **Türk Telekom / Ortak Zırh** | GoodbyeDPI | `-5` + TTL 5 + Yandex DNS (1253 portu) | Türk Telekom, Superonline; DNS'i zehirlenen ağlar |
| **TurkNet / Cloudflare** | GoodbyeDPI | `-5` + TTL 5 + Cloudflare DNS | TurkNet |
| **Orijinal Alternatif v1 (ttl 3)** | GoodbyeDPI | TTL 3 sahte paket + Yandex DNS | Eski alternatif sürüm |
| **Elite Son Çare (-f 1)** | GoodbyeDPI | Ters parçalama + TTL 5 + Yandex DNS | Ağır sansür |
| **Tam Zırh (-9)** | GoodbyeDPI | `-9`, QUIC engelli + Yandex DNS | İnatçı altyapılar |
| **Sahte Paket Yağmuru** | GoodbyeDPI | `-9` + 5 sahte paket (2 tekrar) | İnatçı DPI |
| **SNI Cerrahı** | GoodbyeDPI | Paketi SNI noktasından böler | Hafif ve hızlı alternatif |
| **Zapret: Auto-TTL Sahte Bölme** | Zapret | Otomatik TTL'li sahte bölme | Vodafone / Kablonet adayı |
| **Zapret: Seqovl Bölücü** | Zapret | Sıra çakışmalı bölme | Vodafone / Kablonet adayı |
| **Zapret: Sahte + Çoklu Bölme** | Zapret | Sahte ClientHello + çoklu bölme | Alternatif |
| **Zapret: Ters Sıra** | Zapret | Parçaları ters sırada gönderir | Alternatif |
| **Şifreli DNS Modu** | Windows | Windows 11'in şifreli DNS'i (DoH). Sürücü yüklemez | Oyun Modu sırasında Discord'u ayakta tutmak; son çare |

Zapret profilleri ayrıca Discord sesli sohbetini (UDP) ve QUIC trafiğini de kapsar.

## Gerçek bir ölçüm (Türk Telekom, varsayılan DNS)

Uygulamanın öz-testi, her profili Discord'a 10 kez bağlanarak ölçer. Türk Telekom'da, DNS ISS varsayılanındayken (2026-10-01):

| Sonuç | Profiller |
|---|---|
| ✅ 10/10 | Türk Telekom / Ortak Zırh, Alternatif v1, Son Çare, SNI Cerrahı, Zapret Auto-TTL |
| ⚠️ Aralıklı | Şifreli DNS (6/10), Zapret Sahte+Çoklu (5/10), Tam Zırh (4/10), Sahte Paket Yağmuru (4/10), Seqovl (2/10), Ters Sıra (2/10) |
| ❌ 0/10 | Joker (-5), TurkNet / Cloudflare |

Joker'in burada 0/10 almasının nedeni DNS'e dokunmaması: DNS ISS varsayılanında kaldığında Discord adresleri zehirli çözülüyor. DNS'i elle 8.8.8.8 gibi bir adrese ayarlı kullanıcılarda Joker çalışır.

## Joker mi, Türk Telekom modu mu?

Bir kullanıcı gözlemi (Türk Telekom, DNS elle ayarlı):

- **Joker (-5):** ping çok düşük, sesli sohbet çok iyi. Ama Discord'da mesajlar ve kanal içerikleri bazen geç yükleniyor.
- **Türk Telekom / Ortak Zırh:** ping biraz daha yüksek (~48 ms), ama mesajlar hızlı geliyor.

Hangisinin size uyduğunu denemek için **🧬 DPI Modları** sayfasından profili seçip **Şimdi Dene** deyin. Seçiminiz kaydedilir; bilgisayar yeniden başladığında aynı profil açılır.

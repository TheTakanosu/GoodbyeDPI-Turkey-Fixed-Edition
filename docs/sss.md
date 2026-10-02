# Sık sorulan sorular

## Antivirüsüm uyarı veriyor. Virüs mü?

Hayır. Uygulama, engeli aşmak için **WinDivert** adlı bir ağ sürücüsü kullanıyor (GoodbyeDPI ve Zapret de aynı sürücüyü kullanır). Ağ paketlerine çekirdek seviyesinde dokunan her program bazı antivirüslerin yapay zekâ taramasında "şüpheli" görünebilir.

Kendiniz doğrulayabilirsiniz:

- Kaynak kodun tamamı bu depoda, [`src/`](../src) klasöründe.
- Kurulum dosyasının nasıl üretildiği [`kurulum/takanosu_script.iss`](../kurulum/takanosu_script.iss) içinde.
- Kurulum dosyasını [VirusTotal](https://www.virustotal.com/)'e kendiniz yükleyebilirsiniz.

**Kaspersky** WinDivert'i engelliyor. Uygulama Kaspersky açıkken çalışmayabilir.

## Oyunlarda ban yer miyim?

Bazı anti-cheat sistemleri (EAC, BattlEye, FACEIT) WinDivert sürücüsünü görünce oyunu başlatmıyor. Bunun için **Otomatik Oyun Modu** var:

1. Desteklenen bir oyun açıldığında uygulama motoru ve WinDivert sürücüsünü **tamamen kapatır**.
2. Oyun sırasında Discord'un çalışmaya devam etmesi için sürücü gerektirmeyen **Şifreli DNS Modu**'na geçer.
3. Oyun kapanınca eski profile geri döner, DNS ayarlarınızı birebir geri yükler.

Desteklenen oyunlar: Apex Legends ve diğer EA Anti-Cheat oyunları, Fortnite, Rainbow Six Siege, Rocket League, Hunt: Showdown, CS2 (FACEIT açıkken), Easy Anti-Cheat ve BattlEye kullanan oyunlar.

**Valorant listede yok:** test edildi, Vanguard WinDivert'i engellemiyor. Valorant oynarken koruma açık kalır, Discord kesilmez.

## Ping'im artar mı?

Ölçümlerde motor açıkken ve kapalıyken oyun gecikmesinde fark görülmedi. Discord'da profiller arasında küçük farklar olabiliyor, bkz. [Joker mi, Türk Telekom modu mu?](profiller.md#joker-mi-türk-telekom-modu-mu)

## DNS ayarlarımı değiştiriyor mu?

Sadece **Şifreli DNS Modu** (ve Oyun Modu) Windows'un DNS ayarına dokunur. Kapatıldığında önceki ayarlarınız birebir geri yüklenir. Uygulama kaldırılırken de geri yükleme yapılır.

GoodbyeDPI profillerindeki "Yandex DNS" / "Cloudflare DNS" Windows ayarlarınızı değiştirmez. Sadece motor çalışırken DNS isteklerini o adrese yönlendirir.

## Kara listeye eklediğim siteler güncellemede silinir mi?

Hayır. **Araçlar → Özel Kara Liste** bölümünden eklediğiniz siteler ayrı bir dosyada (`goodbyedpi\ozel-liste.txt`) tutulur ve listenin başında ★ ile gösterilir. Güncellemeler bu dosyaya dokunmaz. v1.1.0'da eklediğiniz siteler de ilk açılışta otomatik olarak bu dosyaya taşınır.

## Bilgisayar açılınca kendiliğinden başlıyor mu?

Evet, **Başlangıçta Çalıştır** açıksa. Uygulama bunun için Windows Görev Zamanlayıcı'yı kullanır, böylece açılışta UAC penceresi çıkmaz. Son kullandığınız profil birkaç saniye içinde yeniden açılır.

## Ağım değişirse ne olur?

**Nöbetçi** her 5 dakikada bir ve ağ yeniden bağlandığında Discord erişimini kontrol eder. Aktif profil çalışmayı bırakmışsa yeniden tarama yapar.

## Hangi ISS'lerde çalışıyor?

Bu sürüm Türk Telekom'da ayrıntılı test edildi. Superonline ve TurkNet için profil sırası önceki sürümlerin kullanıcı deneyimine dayanıyor. **Vodafone ve Kablonet** için henüz yeterli veri yok. Bu ISS'lerden birini kullanıyorsanız [tanı raporunuzu göndermeniz](sorun-giderme.md#tanı-raporu) çok işe yarar.

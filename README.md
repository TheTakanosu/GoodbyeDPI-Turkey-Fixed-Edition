# 🚀 The Takanosu Elite: Türkiye ISS Bypass & Otonom Siber Zırh

Bu proje, ValdikSS'in orijinal GoodbyeDPI yazılımı üzerine inşa edilmiş; Türkiye'deki ISS (İnternet Servis Sağlayıcı) sansürlerini aşmak için optimize edilmiş ve **oyun trafiğine (ping/gecikme) zerre zarar vermeyen** gelişmiş bir C++/Qt arayüz projesidir.

Artık karmaşık `.cmd` dosyalarıyla, klasörlerle veya siyah ekranlarla uğraşmanıza gerek yok. Her şey tek tıkla, otonom olarak çalışır!

![The Takanosu Elite Arayüzü](https://github.com/user-attachments/assets/7e92fec3-0bd2-4263-8055-33d0d1c972e8)

---

## ⚠️ Önemli Uyarılar

* **🛡️ Kaspersky Antivirüsü:** Kaspersky yazılımı, paketin çekirdek seviyesindeki manipülasyonuna (WinDivert) engel olmaktadır. Programın stabil çalışması için Kaspersky'nin tamamen kapatılması veya sistemden kaldırılması gerekebilir.
* **🚨 VirusTotal & Güvenlik (False Positive):** Uygulamamız, sansürü aşmak için işletim sisteminin derin ağ sürücülerine (Kernel seviyesinde) müdahale eder ve zombi DPI servislerini otomatik kapatır. Bu derin yetkiler sebebiyle bazı antivirüslerin yapay zeka (ML) taramaları uygulamayı "şüpheli" olarak işaretleyebilir (False Positive). Program, 70'ten fazla ana antivirüs motorundan temiz onayı almıştır ve %100 açık kaynak/güvenlidir.

---

## ✨ Yenilikler ve Öne Çıkan Özellikler (v1.1.0)

* **🎨 Modern Arayüz (Dashboard):** Tamamen yenilenmiş, karanlık tema destekli profesyonel kontrol paneli.
* **🤖 5 Aşamalı Otonom Motor:** "Otomatik Algıla" modundayken altyapınızı (Türk Telekom, TurkNet, Superonline, Yakarnet vb.) IP-API üzerinden saniyeler içinde tespit eder ve duvarı yıkacak en doğru siber mermiyi namluya sürer.
* **⚡ Sıfır Ping Kaybı:** Joker (-5) parametre optimizasyonu sayesinde, DNS yönlendirmesi yapmadan giden paketleri kılıç gibi parçalar. Rekabetçi (E-Spor) oyunlarda milisaniye bile kaybetmezsiniz.
* **🛠️ Sistem Araçları Merkezi:** Menü üzerinden tek tıkla; DNS Önbelleği Temizleme (Flush DNS), Ağ Adaptörü Yenileme, Zombi Süreçleri Katletme (Kill DPI) ve anlık Ping Testi yapabilirsiniz.
* **🔄 Otonom Güncelleme (OTA):** Yeni bir sürüm/yama yayınlandığında, sistem sizi otomatik olarak uyarır ve tek tıkla kendini günceller.

---

## 📥 Kurulum Rehberi

Artık manuel ZIP çıkarma işlemleriyle uğraşmıyoruz. Kurulum sadece 10 saniye sürer!

1. Sağ taraftaki **Releases** bölümünden (veya doğrudan [Buradan](https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/releases/latest)) güncel **`TheTakanosu_Elite_Setup.exe`** dosyasını indirin.
2. Kurulum sihirbazını çalıştırın (Uygulama, eski GoodbyeDPI sürümlerinden kalan çakışmaları ve zombi servisleri kurulum öncesi otomatik temizleyecektir).
3. Kurulum bitince masaüstünüze gelen `The Takanosu Elite` kısayoluna tıklayın.
4. Arayüzden **Otomatik Algıla (Önerilen)** modunu seçin ve **YENİDEN BAŞLAT / GÜNCELLE** butonuna basın.

*(Not: Uygulama arka planda Windows Servisi olarak sessizce ve kesintisiz çalışır. Zırh aktif edildikten sonra arayüzü kapatabilirsiniz, PC her açıldığında korumanız otomatik olarak devrede olacaktır.)*

---

## 📜 Credits & Teşekkür

* **Base Software:** ValdikSS
* **Original Turkey Configs:** Çağrı Taşkın
* **Optimization, Autonomous Engine & UI (Elite Edition):** TheTakanosu

# Geliştirme ve derleme

## Gerekenler

- Windows 10/11 x64
- Visual Studio 2026 (C++ masaüstü geliştirme)
- Qt 6 (msvc 64-bit) ve Qt Visual Studio Tools
- Inno Setup 6 (kurulum paketi için)

## Klasör yapısı

| Klasör | İçerik |
|---|---|
| `src/` | C++/Qt kaynak kodu ve Visual Studio projesi |
| `kurulum/` | Inno Setup betiği |
| `x64/Release/assets/` | Uygulama simgeleri |
| `x64/Release/goodbyedpi/turkey-blacklist.txt` | Varsayılan kara liste |
| `docs/` | Belgeler |

Qt DLL'leri, GoodbyeDPI ve Zapret ikilileri depoda yoktur. Derlemeden önce bunları `x64/Release/` altına koymanız gerekir:

- `x64/Release/goodbyedpi/x86_64/`: [GoodbyeDPI](https://github.com/ValdikSS/GoodbyeDPI/releases) `x86_64` klasörü
- `x64/Release/zapret/`: [Zapret](https://github.com/bol-van/zapret/releases) içinden `winws.exe`, `WinDivert.dll`, `WinDivert64.sys`, `cygwin1.dll`, `fake\` klasörü ve lisans dosyası
- Qt DLL'leri: derledikten sonra `windeployqt x64\Release\TheTakanosu_Elite.exe`

## Derleme

`TheTakanosu_Elite.slnx` dosyasını Visual Studio'da açın, **Release | x64** seçip derleyin. Çıktı `x64\Release\TheTakanosu_Elite.exe` olarak gelir.

Komut satırından:

```
MSBuild src\TheTakanosu_Elite.vcxproj -p:Configuration=Release -p:Platform=x64
```

## Testler

`test\calistir.cmd` kara liste fonksiyonlarını (`src/kara_liste.h`) uygulamayı açmadan, geçici klasörde gerçek dosyalarla test eder.

## Kurulum paketi

```
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" kurulum\takanosu_script.iss
```

Paket `cikti\` klasörüne çıkar.

## Yayınlama (sürüm çıkarma) kontrol listesi

1. `src/TheTakanosu_Elite.cpp` içindeki `APP_VERSION`, `src/app.rc` ve `kurulum/takanosu_script.iss` (`AppVersion`, `OutputBaseFilename`) aynı sürümü göstermeli.
2. GitHub Release'e kurulum dosyasını **iki adla** yükleyin:
   - `TheTakanosu_Elite_vX.Y.Z_Setup.exe` (v1.2.0 ve sonrası güncelleyici bunu arar)
   - `TheTakanosu_Elite_Setup.exe` (v1.1.0 güncelleyicisi bunu arar)
3. Release yayınlandıktan **sonra** `version.txt`'yi yeni sürüme güncelleyin. v1.1.0 kullanıcıları güncellemeyi bu dosyadan öğrenir; dosya erken güncellenirse indirme bulunamaz.

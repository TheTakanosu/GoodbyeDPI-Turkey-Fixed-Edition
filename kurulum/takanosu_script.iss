[Setup]
; 🚀 BUM! UYGULAMA KİMLİĞİ VE YAYINCI BİLGİLERİ
AppId={{TAKANOSU-ELITE-SIBER-ZIRH-V1}
AppName=The Takanosu Elite
AppVersion=1.2.0
AppPublisher=TheTakanosu
AppPublisherURL=https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition
AppSupportURL=https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition
AppUpdatesURL=https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition
; Revo ve Denetim Masasında dümdüz ve temiz görünmesi için zırh:
UninstallDisplayName=The Takanosu Elite
DirExistsWarning=no

; Varsayılan Kurulum Yeri (Program Files dizini)
DefaultDirName={autopf}\TheTakanosu Elite
DisableProgramGroupPage=yes
DefaultGroupName=The Takanosu Elite

; Kurulum dosyası deponun cikti\ klasörüne çıkar (git'te yok sayılır)
OutputDir={#SourcePath}..\cikti
; 🚀 Sürüm ismi v1.2.0 Setup olarak güncellendi! (Uygulama içi güncelleyici "TheTakanosu_Elite...._Setup.exe" adını arar)
OutputBaseFilename=TheTakanosu_Elite_v1.2.0_Setup

; 🎨 GÖRSEL TASARIM AYARLARI
SetupIconFile={#SourcePath}..\x64\Release\assets\icon.ico
UninstallDisplayIcon={app}\TheTakanosu_Elite.exe
WizardStyle=modern

; 🚀 YÖNETİCİ İZNİ ZORUNLULUĞU (Çok Önemli!)
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[Messages]
turkish.WelcomeLabel1=The Takanosu Elite Kurulum Sihirbazına Hoş Geldiniz!
turkish.WelcomeLabel2=Bu sihirbaz, bilgisayarınıza özgür ve sansürsüz internetin anahtarını (The Takanosu Elite v1.2.0) kuracaktır.%n%nDevam etmeden önce lütfen diğer tüm uygulamaları kapatın. Kuruluma hazır olduğunuzda 'İleri' butonuna basın!
turkish.WizardReady=The Takanosu Elite Kuruluma Hazır!
turkish.ReadyLabel2=Elite motorunu (GoodbyeDPI + Zapret), güncel kara listeyi ve gerekli tüm ağ protokollerini yüklemek için 'Kur' butonuna basın.
turkish.FinishedHeadingLabel=The Takanosu Elite Başarıyla Kuruldu!
turkish.FinishedLabelNoIcons=DPI engelleri parçalandı, sansür duvarı delindi! Özgür internetin tadını çıkarın.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[InstallDelete]
; v1.1.0'ın çalışırken ürettiği geçici .bat ve log dosyalarını temizle (artık kullanılmıyorlar)
Type: files; Name: "{app}\*.bat"
Type: files; Name: "{app}\takanosu_logs.txt"
Type: filesandordirs; Name: "{app}\update_cache"

[Files]
; Ana derlenmiş motor paketleniyor
Source: "{#SourcePath}..\x64\Release\TheTakanosu_Elite.exe"; DestDir: "{app}"; Flags: ignoreversion
; Bütün DLL'ler, assets, goodbyedpi, zapret ve blacklist dahil ediliyor; geçici dosyalar dışlanıyor
Source: "{#SourcePath}..\x64\Release\*"; Excludes: "TheTakanosu_Elite.exe,*.bat,takanosu_logs.txt,startup_task.xml,update_cache,dns_backup.json,selftest_report.txt,ozel-liste.txt,eski-liste.txt"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; NOT: Başlangıçta çalıştırma artık uygulamanın kendisi tarafından Görev Zamanlayıcı ile ayarlanıyor.
; (Yönetici yetkisi isteyen programları Windows "Run" kaydından açılışta sessizce engelliyordu.)

[Icons]
; Masaüstü ve Başlat Menüsü Kısayolları
Name: "{autoprograms}\The Takanosu Elite"; Filename: "{app}\TheTakanosu_Elite.exe"
Name: "{autodesktop}\The Takanosu Elite"; Filename: "{app}\TheTakanosu_Elite.exe"; Tasks: desktopicon

[Run]
; Kurulum bittikten sonra otomatik çalıştırma tikinin admin yetkisini tetiklemesi için shellexec modu active!
Filename: "{app}\TheTakanosu_Elite.exe"; Description: "{cm:LaunchProgram,The Takanosu Elite}"; Flags: nowait postinstall skipifsilent shellexec
Filename: "https://github.com/TheTakanosu/GoodbyeDPI-Turkey-Fixed-Edition/discussions/2"; Description: "Projeyi GitHub'da Ziyaret Et ve Destekle!"; Flags: shellexec runasoriginaluser postinstall unchecked

[UninstallRun]
; 1. AÇIK OLAN BÜTÜN EXE'LERİ ACIMADAN ÖLDÜR
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM TheTakanosu_Elite.exe"; Flags: runhidden; RunOnceId: "KillApp"
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM goodbyedpi.exe"; Flags: runhidden; RunOnceId: "KillGdpi"
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM winws.exe"; Flags: runhidden; RunOnceId: "KillWinws"

; 1b. ŞİFRELİ DNS MODU AÇIKSA KULLANICININ ESKİ DNS AYARLARINI GERİ YÜKLE
Filename: "{app}\TheTakanosu_Elite.exe"; Parameters: "--restore-dns"; Flags: runhidden waituntilterminated; RunOnceId: "RestoreDns"

; 2. BAŞLANGIÇ GÖREVİNİ SİL
Filename: "{sys}\schtasks.exe"; Parameters: "/Delete /TN ""TheTakanosu Elite"" /F"; Flags: runhidden; RunOnceId: "DelTask"

; 3. ESKİ SÜRÜM SERVİSLERİNİ VE ÇEKİRDEK SÜRÜCÜLERİNİ DURDUR / SİL (WinDivert zırhı aktif)
Filename: "{sys}\sc.exe"; Parameters: "stop ""GoodbyeDPI_Elite"""; Flags: runhidden; RunOnceId: "StopSvc"
Filename: "{sys}\sc.exe"; Parameters: "delete ""GoodbyeDPI_Elite"""; Flags: runhidden; RunOnceId: "DelSvc"
Filename: "{sys}\sc.exe"; Parameters: "stop ""WinDivert"""; Flags: runhidden; RunOnceId: "StopWd"
Filename: "{sys}\sc.exe"; Parameters: "delete ""WinDivert"""; Flags: runhidden; RunOnceId: "DelWd"
Filename: "{sys}\sc.exe"; Parameters: "stop ""WinDivert1.4"""; Flags: runhidden; RunOnceId: "StopWd14"
Filename: "{sys}\sc.exe"; Parameters: "delete ""WinDivert1.4"""; Flags: runhidden; RunOnceId: "DelWd14"

; 4. ⏱️ Windows'un sys dosyasını kilitten kurtarması için kısa bekleme
Filename: "{sys}\cmd.exe"; Parameters: "/c ping 127.0.0.1 -n 3 > nul"; Flags: runhidden waituntilterminated; RunOnceId: "Wait"

; 5. 🚀 ELVEDA VE ANKET EKRANI
; explorer.exe üzerinden açıyoruz: böylece tarayıcı kaldırıcının YÖNETİCİ yetkisini devralmadan, normal kullanıcı olarak açılır.
Filename: "{win}\explorer.exe"; Parameters: """https://docs.google.com/forms/d/e/1FAIpQLScS714WWFmrRAKRmIJE3yYDJ_TL_HskwTnUhOi43LJ_TTFgig/viewform?usp=dialog"""; Flags: nowait; RunOnceId: "Survey"

[Code]
// 1. OTONOM "ONAR / GÜNCELLE" BEYNİ
function InitializeSetup(): Boolean;
begin
  Result := True;
  if RegKeyExists(HKEY_LOCAL_MACHINE, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{TAKANOSU-ELITE-SIBER-ZIRH-V1}_is1') or
     RegKeyExists(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{TAKANOSU-ELITE-SIBER-ZIRH-V1}_is1') then
  begin
    // Sessiz kurulumda (uygulama içi güncelleme) soru sorma
    if not WizardSilent() then
    begin
      if MsgBox('The Takanosu Elite sisteminizde zaten kurulu!' + #13#10 + #13#10 + 'Mevcut dosyaların üzerine yazarak sistemi GÜNCELLEMEK veya ONARMAK ister misiniz?', mbConfirmation, MB_YESNO) = IDNO then
      begin
        Result := False;
      end;
    end;
  end;
end;

// 2. 🚀 BUM! KURULUM ÖNCESİ OTONOM SİBER TEMİZLİK!
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  // Inno Setup dosyaları kopyalamaya BAŞLAMADAN HEMEN ÖNCE bu kod çalışır!
  if CurStep = ssInstall then
  begin
    // Arka planda program veya DPI motorları açıksa kapat ki dosyalar kilitli kalmasın!
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM TheTakanosu_Elite.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM goodbyedpi.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM winws.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // v1.1.0'ın kurduğu, bilgisayar açılışında kendiliğinden başlayan servisleri kaldır
    Exec(ExpandConstant('{sys}\sc.exe'), 'stop "GoodbyeDPI_Elite"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\sc.exe'), 'delete "GoodbyeDPI_Elite"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\sc.exe'), 'stop "WinDivert"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Exec(ExpandConstant('{sys}\sc.exe'), 'stop "WinDivert1.4"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

    // Windows'un WinDivert.sys dosyasını kilit listesinden çıkarması için 1.5 saniye bekle
    Sleep(1500);

    // v1.1.0'dan yükseltme: kullanıcının eklediği siteler turkey-blacklist.txt içindeydi ve bu dosyanın
    // üzerine yazılmak üzere. Önce yedekle; uygulama ilk açılışta kullanıcının sitelerini ozel-liste.txt'ye taşır.
    // (ozel-liste.txt varsa kurulum zaten v1.2.0+ ve kullanıcının siteleri ayrı dosyada.)
    if FileExists(ExpandConstant('{app}\goodbyedpi\turkey-blacklist.txt')) and
       not FileExists(ExpandConstant('{app}\goodbyedpi\ozel-liste.txt')) and
       not FileExists(ExpandConstant('{app}\goodbyedpi\eski-liste.txt')) then
      FileCopy(ExpandConstant('{app}\goodbyedpi\turkey-blacklist.txt'), ExpandConstant('{app}\goodbyedpi\eski-liste.txt'), True);
  end;
end;

[UninstallDelete]
; Kurulum kaldırılırken goodbyedpi/zapret klasörlerini ve içindeki o inatçı WinDivert64.sys dosyalarını KÖKTEN SİL!
Type: filesandordirs; Name: "{app}\goodbyedpi"
Type: filesandordirs; Name: "{app}\zapret"
Type: filesandordirs; Name: "{app}"

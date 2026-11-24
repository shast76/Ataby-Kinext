; Kinext 1.0 - kurulum sihirbazi (Inno Setup 6.4+ ya da 7)
; Dogrudan derlemeyin: once build_installer.bat calistirilir, o "dist" klasorunu hazirlayip bu betigi derler.

#define AppName      "Kinext"
#define AppVersion   "1.0"
#define AppPublisher "Ataby"
#define AppURL       "https://github.com/shast76/Project-Kinext"
#define AppExe       "kinext_dashboard.exe"
; Visual C++ calisma zamani paketi varsa kuruluma eklenir
#define HaveRedist   FileExists(AddBackslash(SourcePath) + "redist\vc_redist.x64.exe")

[Setup]
; AppId kurulumu tanimlar: guncellemelerde ayni kalmali, degistirmeyin
AppId={{BBC449E7-5ACF-4C24-8C75-ACED33AEFFD2}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} {#AppVersion}
SetupIconFile=..\assets\kinext.ico
WizardStyle=modern
WizardImageFile=wizard_image.bmp,wizard_image_2x.bmp
WizardSmallImageFile=wizard_small.bmp,wizard_small_2x.bmp
OutputDir=Output
OutputBaseFilename=Kinext-{#AppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
; Sadece 64 bit Windows 10/11 (ONNX Runtime ve DirectML 64 bit)
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
; Kurulum dili Windows diline gore otomatik secilir, gerekirse sorulur
ShowLanguageDialog=auto
CloseApplications=yes

[Languages]
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
tr.VCRedist=Microsoft Visual C++ çalışma zamanı kuruluyor...
en.VCRedist=Installing the Microsoft Visual C++ runtime...
tr.LaunchApp=Kinext'i şimdi başlat
en.LaunchApp=Launch Kinext now
tr.CameraNote=Kinext bir web kamerası (ya da telefon kamerası) ile çalışır. Kinect gerekmez.
en.CameraNote=Kinext works with a webcam (or your phone's camera). No Kinect required.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Program, DLL'ler, model ve gorseller (build_installer.bat tarafindan "dist" klasorune toplanir)
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
#if HaveRedist
; Visual C++ calisma zamani: sadece bilgisayarda yoksa kurulur
Source: "redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: VCRedistNeeded
#endif

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
#if HaveRedist
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "{cm:VCRedist}"; Flags: waituntilterminated; Check: VCRedistNeeded
#endif
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchApp}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Code]
// Visual C++ 2015-2022 (x64) calisma zamani kurulu mu?
function VCRedistNeeded: Boolean;
var
  Installed: Cardinal;
begin
  Result := True;
  if RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) then
    Result := Installed <> 1;
end;

// Hosgeldin sayfasina kamera notu ekle
procedure InitializeWizard;
begin
  WizardForm.WelcomeLabel2.Caption := WizardForm.WelcomeLabel2.Caption + #13#10#13#10 + CustomMessage('CameraNote');
end;

// Kurulum dilini programin varsayilan dili yap (ayar dosyasi yoksa).
// Program ilk acilista yine dil sorar; bu sadece hangi dilin secili gelecegini belirler.
procedure CurStepChanged(CurStep: TSetupStep);
var
  Dir, FileName, Lang: String;
begin
  if CurStep = ssPostInstall then
  begin
    Dir := ExpandConstant('{userappdata}\Kinext');
    FileName := Dir + '\kinext_settings.txt';
    if not FileExists(FileName) then
    begin
      ForceDirectories(Dir);
      if ActiveLanguage = 'en' then Lang := '1' else Lang := '0';
      SaveStringToFile(FileName, 'lang=' + Lang + #13#10, False);
    end;
  end;
end;

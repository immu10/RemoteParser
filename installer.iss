; Inno Setup script for File Parser
; Build:  compile this with Inno Setup (ISCC.exe installer.iss) AFTER rebuilding
;         the app, so build\fileParser.exe is the current binary.
; Output: dist\fileParser-setup.exe

#define MyAppName "File Parser"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "immu10"
#define MyAppExeName "fileParser.exe"

[Setup]
AppId={{8F2A6E14-7C3B-4D9A-9E21-1A2B3C4D5E6F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=dist
OutputBaseFilename=fileParser-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 64-bit MinGW build -> install into the real Program Files and run 64-bit.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Writing to Program Files + adding a firewall rule needs elevation.
PrivilegesRequired=admin

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"
Name: "firewall";   Description: "Allow File Parser through Windows Firewall (needed to host/receive on this PC)"; GroupDescription: "Networking:"

[Files]
; --- main executable ---
Source: "build\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; --- Qt + MinGW + OpenSSL runtime DLLs ---
Source: "build\*.dll"; DestDir: "{app}"; Flags: ignoreversion

; --- TLS certificate + key (required for this PC to act as Server/host) ---
Source: "build\server.crt"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\server.key"; DestDir: "{app}"; Flags: ignoreversion

; --- Qt plugin folders (layout must be preserved next to the exe) ---
Source: "build\platforms\*";          DestDir: "{app}\platforms";          Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\tls\*";                DestDir: "{app}\tls";                Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\styles\*";             DestDir: "{app}\styles";             Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\imageformats\*";       DestDir: "{app}\imageformats";       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\iconengines\*";        DestDir: "{app}\iconengines";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\generic\*";            DestDir: "{app}\generic";            Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\translations\*";       DestDir: "{app}\translations";       Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Add an inbound firewall rule for the app (only if the task is selected).
Filename: "{sys}\netsh.exe"; \
    Parameters: "advfirewall firewall add rule name=""File Parser"" dir=in action=allow program=""{app}\{#MyAppExeName}"" enable=yes profile=private,domain"; \
    Flags: runhidden; Tasks: firewall
; Offer to launch after install.
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Remove the firewall rule on uninstall (matches by name; no-op if absent).
Filename: "{sys}\netsh.exe"; \
    Parameters: "advfirewall firewall delete rule name=""File Parser"""; \
    Flags: runhidden; RunOnceId: "DelFileParserFwRule"

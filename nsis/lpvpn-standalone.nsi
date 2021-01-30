SilentInstall silent
Icon icon.ico

Section
InitPluginsDir
SetOutPath "$pluginsdir" ;It is better to put stuff in $pluginsdir, $temp is shared

File discord_game_sdk.dll
File wintun.dll
File lpvpn.exe

ExecWait '"$pluginsdir\lpvpn.exe"' ;You should always use full paths and proper quotes

SetOutPath $exedir ;Change current dir so $temp and $pluginsdir is not locked by our open handle
SectionEnd

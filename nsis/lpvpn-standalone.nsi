SetCompressor lzma

SilentInstall silent
Icon icon.ico

Section
InitPluginsDir
SetOutPath "$pluginsdir"

File discord_game_sdk.dll
File wintun.dll
File lpvpn.exe

ExecWait '"$pluginsdir\lpvpn.exe"'

SetOutPath $exedir
SectionEnd


Debian
====================
This directory contains files used to package 405Coind/405Coin-qt
for Debian-based Linux systems. If you compile 405Coind/405Coin-qt yourself, there are some useful files here.

## 405Coin: URI support ##


405Coin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install 405Coin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your 405Coin-qt binary to `/usr/bin`
and the `../../share/pixmaps/405Coin128.png` to `/usr/share/pixmaps`

405Coin-qt.protocol (KDE)


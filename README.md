# PSVault

A native PlayStation 4 homebrew storefront built with OpenOrbis.

## Current first-version implementation
- SDL2 native interface with DualShock controller navigation
- Home, catalogue, downloads and settings screens
- HTTPS catalogue loading from this repository's `catalog.tsv`
- Direct PKG downloads to `/data/PSVault/`
- Sequential download queue, progress display, retry and removal
- Downloaded packages remain available for the PS4 Package Installer

## Catalogue format
One package per line:

```
Title|Category|Version|Size|https://host.example/path/file.pkg
```

Use direct HTTPS package URLs only. List only homebrew or packages you are authorized to distribute. Keep titles and fields free of the pipe character.

## Controls
- D-pad: navigate
- Cross: select / add / start selected download
- Circle: back
- Triangle: refresh catalogue
- Start: process queued downloads
- Delete key (keyboard): remove selected queue item

## Build
GitHub Actions builds the native application with the OpenOrbis PS4 toolchain.

## Important limitations
- Downloads are sequential and currently restart from zero after a failed transfer; HTTP Range resume is not implemented yet.
- Package installation is handed off to the PS4's Package Installer. PSVault does not bypass licences or install unauthorized commercial games.
- The native app still requires real-console verification for controller, network, storage and firmware compatibility.

PSVault is independent and not affiliated with Sony Interactive Entertainment.

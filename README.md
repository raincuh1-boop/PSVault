# PSVault

A native PlayStation 4 homebrew storefront built with SDL2 and the OpenOrbis toolchain.

## Storefront
- Native controller-operated home, catalogue, package details, downloads and settings screens
- Search by package title, category or description using the on-screen controller keyboard
- Category filtering with L1/R1 and local catalogue caching for offline browsing
- Optional package descriptions in the catalogue
- Direct HTTPS PKG downloads to `/data/PSVault/`
- Persistent download queue and download history
- Sequential queue processing, retry, removal, live transfer speed, ETA and content-length checks

## Catalogue format

One package per line:

```
Title|Category|Version|Size|https://host.example/path/file.pkg|Optional description
```

The description is optional. Use direct HTTPS package URLs ending in `.pkg`. Keep fields free of the pipe character. List only homebrew or packages you are authorized to distribute.

## DualShock controls

- D-pad: navigate
- Cross: open package details / add to queue / start selected download
- Circle: back
- Square in catalogue: open search keyboard; Square in downloads: remove selected item
- D-pad + Cross in search keyboard: type; Circle closes it; Triangle clears search
- L1 / R1: previous / next category
- Triangle in catalogue: refresh catalogue
- Start in downloads: process queued and failed items

## Build

GitHub Actions builds the native application with the OpenOrbis PS4 toolchain and publishes the PKG as a workflow artifact.

## Current limitations

- Failed transfers restart from zero. HTTP Range resume is not implemented.
- Downloaded packages are saved for the PS4 Package Installer. Installation is a manual handoff; PSVault does not bypass licences or distribute unauthorized commercial packages.
- The native app still needs one real-console verification pass for controller, network, storage and firmware compatibility.
- Package cover art and a verified GoldHEN-assisted install integration are not included yet.

PSVault is independent and not affiliated with Sony Interactive Entertainment.

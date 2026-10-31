# PSVault

A native PlayStation 4 homebrew storefront built with SDL2 and the OpenOrbis toolchain.

## Storefront
- Native controller-operated home, catalogue, package details, downloads and settings screens
- Search by package title, category or description using the on-screen controller keyboard
- Category filtering with L1/R1 and local catalogue caching for offline browsing
- Optional package descriptions in the catalogue
- Metadata-only catalogue entries are supported; entries without an authorized direct PKG URL cannot be queued
- Direct HTTPS PKG downloads, staged in `/data/PSVault/` and moved to `/data/pkg/` when complete
- PS4 PKG header validation before a file is marked downloaded
- GoldHEN internal Package Installer handoff through `/data/pkg/` (GoldHEN 2.3+)
- Persistent download queue and download history
- Sequential queue processing, retry, removal, live transfer speed, ETA and content-length checks
- Pause and resume using HTTP Range when the host supports it; partial files are checkpointed locally
- Safe full restart when a server ignores Range requests

## Catalogue format

One package per line. Leave the URL field empty for a metadata-only entry:

```
Title|Category|Version|Size|https://host.example/path/file.pkg|Optional description
```

The description is optional. Use direct HTTPS package URLs ending in `.pkg`. Keep fields free of the pipe character. List only homebrew or packages you are authorized to distribute.

## DualShock controls

- D-pad: navigate
- Cross: open package details / add to queue / start selected download
- Circle: back; during an active download, pause and keep the partial file
- Square in catalogue: open search keyboard; Square in downloads: remove selected item
- D-pad + Cross in search keyboard: type; Circle closes it; Triangle clears search
- L1 / R1: previous / next category
- Triangle in catalogue: refresh catalogue
- Start in downloads: process queued, paused and failed items

## Build

GitHub Actions builds the native application with the OpenOrbis PS4 toolchain and publishes the PKG as a workflow artifact.

## Current limitations

- Resume depends on the package host honoring HTTP Range requests. Hosts that ignore Range are safely restarted from byte zero.
- Validated packages are moved into `/data/pkg/`. On GoldHEN 2.3 or later, enable internal package installation and select HDD as the Package Installer source. Actual install remains a user-confirmed action in Debug Settings; PSVault does not trigger an undocumented installer API.
- The native app still needs one real-console verification pass for controller, network, storage, GoldHEN settings and firmware compatibility.
- Package cover art is not included yet. GoldHEN handoff is via the documented `/data/pkg/` scan path, not automatic installation.
- Metadata-only entries do not provide downloads; add an authorized direct HTTPS `.pkg` URL to enable queueing.

PSVault is independent and not affiliated with Sony Interactive Entertainment.

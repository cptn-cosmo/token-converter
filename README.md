# Token Converter

A native Qt 6 / KDE Kirigami desktop app that recursively batch-converts media
files inside project folders — out of the box, PNG → WebP.

- Projects in a sidebar, each with its own settings (seeded from global defaults)
- Lossless or lossy (0–100) quality, EXIF/ICC preservation
- Replace, trash, back up, or keep the originals
- Dry-run preview with a confirm step before anything is touched
- Include/exclude globs, folder exclusions, symlink policy
- Prefix/postfix and filename templates with live preview, collision handling
- Parallel conversion with pause / resume / cancel, per-file results and space saved
- Safe writes: temp file → verify → atomic rename, *then* deal with the original

## Building

### Dependencies

Qt ≥ 6.6 (Base, Declarative, QuickControls2), KDE Frameworks ≥ 6.8 (Kirigami,
KConfig, KI18n, KCoreAddons, extra-cmake-modules), libwebp (incl. libwebpmux),
CMake ≥ 3.22 and a C++20 compiler. `qqc2-desktop-style` is recommended so the
UI follows the Plasma style.

**Arch Linux**

```sh
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules \
    qt6-base qt6-declarative kirigami kconfig ki18n kcoreaddons \
    qqc2-desktop-style libwebp
```

**Fedora**

```sh
sudo dnf install gcc-c++ cmake ninja-build extra-cmake-modules \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-kirigami-devel kf6-kconfig-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
    kf6-qqc2-desktop-style libwebp-devel
```

**Debian / Ubuntu** (Debian 13+, Ubuntu 24.10+)

```sh
sudo apt install build-essential cmake ninja-build extra-cmake-modules \
    qt6-base-dev qt6-declarative-dev \
    libkirigami-dev qml6-module-org-kde-kirigami libkf6config-dev libkf6i18n-dev \
    libkf6coreaddons-dev qml6-module-org-kde-desktop qml6-module-qtquick-dialogs \
    libwebp-dev gettext
```

### Compile, test, install

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure     # QT_QPA_PLATFORM=offscreen on headless machines
sudo cmake --install build                      # or run build/bin/token-converter directly
```

### Arch Linux package

`packaging/arch/PKGBUILD` builds a package from the current checkout:

```sh
cd packaging/arch && makepkg -si
```

The `Arch Linux package` GitHub Actions workflow builds it in an `archlinux:base-devel`
container:

- **Every push / pull request** builds a prerelease package versioned
  `<last version>.r<commits>.g<hash>` and keeps it as a workflow artifact only.
- **Pushing a version tag** (`vX.Y.Z`, must match the version in `CMakeLists.txt`)
  builds version `X.Y.Z` and creates a GitHub release with the package attached.

The `[geeks.gay]` pacman repository gets its package from
[arch-pkg-builder](https://github.com/cptn-cosmo/arch-pkg-builder), which builds
`packages/token-converter` from the release tag.

To release: bump `project(token-converter VERSION …)` in `CMakeLists.txt` (and the
`<release>` entry in the metainfo), commit, then `git tag vX.Y.Z && git push --tags`.
To publish it to repo.geeks.gay, bump `pkgver` (and `sha256sums`, via `updpkgsums`) in
arch-pkg-builder's `packages/token-converter/PKGBUILD`.

## Usage

1. **Add a project** with the `+` button in the sidebar and pick a folder. The name
   defaults to the folder name. Right-click (or use `⋮`) to rename, remove (files are
   never deleted) or open the folder.
2. **Adjust settings** for the project. Settings that differ from the global defaults
   get a small dot and an undo button to reset just that setting. *Reset to Global
   Defaults…* in the page toolbar resets everything.
3. **Start.** The header shows *X of Y files*, the current file, and the space saved.
   Pause stops after the files currently being encoded; Cancel aborts them too.
   Expand **Results** to filter per-file outcomes (converted / skipped / failed) with
   sizes and error details.
4. With **Preview before converting** enabled, Start only scans and opens a table of
   what would happen (file, target name, estimated result, action). Nothing is touched
   until you confirm.

**Global Defaults** (gear entry at the bottom of the sidebar) uses the same form.
Edits only affect projects created afterwards — each project gets a snapshot copy, not
a live link. *Apply to All Projects…* lets you push chosen settings to existing projects
explicitly; *Restore Factory Defaults…* resets the global defaults.

### Filename templates

`prefix + template + postfix + "." + target extension`. Placeholders:
`{name}` `{ext}` `{date}` (yyyy-MM-dd, file mtime) `{time}` (HH-mm-ss) `{counter}`
(`{counter:3}` → `007`) `{parent}` `{width}` `{height}`. Use `{{`/`}}` for literal braces.

### Patterns

Comma-separated, case-insensitive globs: `*` within a folder, `**` across folders,
`?`, `[abc]`/`[!abc]`. Patterns without `/` match the file name; patterns with `/`
match the path relative to the project folder (e.g. `assets/**/*.png`). Excluded
folders match a folder name (`node_modules`) or relative path (`build/out`). The
backup folder and in-progress temp files are always skipped.

Settings live in `~/.config/token-converterrc`.

## Architecture

```
src/core/   tokenconverter_core (static library, no QML)
  settings.*           schema: keys, types, factory defaults, normalisation,
                       versioned migration, inheritance helpers (snapshot/reset/apply)
  settingsstore.*      KConfig persistence; one SchemaVersion per settings group
  settingscontroller.* QObject over one settings map (project or global) for QML;
                       tracks keys modified against a reference map
  rulematcher.*        "png, PNG -> webp" rules, case-insensitive extensions
  globfilter.*         include/exclude/folder globs → regexes
  filenametemplate.*   placeholder expansion + sanitising
  iconverter.h         pluggable converter interface
  converterregistry.*  thread-safe registry (later registrations win)
  imageconverter.*     libwebp encoder (+ libwebpmux for EXIF/ICC), Qt image I/O
                       for other formats
  scanner.*            recursive walk (symlink/cycle aware) + planning: target
                       names, counters, collision resolution within the batch
  safewriter.*         temp → verify → rename(2) → handle original; never loses data
  jobrunner.*          state machine: Scanning → (AwaitingConfirmation) → Running
                       ⇄ Paused → Finished/Cancelled, private QThreadPool
  resultsmodel.*       results, status filter proxy and dry-run preview models
src/app/    executable: AppController (projects, sessions, global settings,
            exposed as the App QML singleton) and the Kirigami QML UI
tests/      Qt Test suites (template, rules, globs, safe write, settings
            inheritance/migration/storage, scanner, end-to-end job runner)
```

**Threading.** The UI thread owns all state. Scanning and each file conversion run on
the project's `QThreadPool` (max threads = the worker count); workers post results
back with queued invocations. Pause stops dispatching new files; cancel also sets a
shared atomic flag that the libwebp progress hook polls to abort in-flight encodes,
whose temp files `SafeWriter` then removes.

**Adding a converter** (e.g. ffmpeg for video/audio): implement `IConverter`
(`sourceFormats`, `targetFormats`, `convert(input, tempOutput, format, options)`,
`verify`) and register it in `ConverterRegistry::instance()`. The rules, scanner,
safe-write logic and UI pick it up automatically, and its target formats appear in
the rule editor.

**Settings schema.** Adding an option means adding a key to `Settings::definitions()`
and `factoryDefaults()`. Groups stored with an older `SchemaVersion` are passed through
`Settings::migrate()` on load. Missing keys are filled from factory defaults, so new
options appear in existing projects with sensible values.

**Developer aids** (debug builds only): `TOKENCONVERTER_DEV_SCREENSHOTS="1500:/tmp/a.png"`
renders window snapshots (combine with `QT_QPA_PLATFORM=offscreen`).
`TOKENCONVERTER_DEV_VIEW=global` and `TOKENCONVERTER_DEV_START=1` drive the UI.

## License

GPL-3.0-or-later. Icon and metadata: CC0-1.0.

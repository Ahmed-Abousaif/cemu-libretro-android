# Bringing in newer Cemu Android changes

This repository is the Android libretro core. It is based on the Cemu Android port:

- remote: `upstream`
- repository: https://github.com/SSimco/Cemu.git
- branch: `android-port`

Libretro code lives in `src/libretro/`. Other Android fixes are in the Cemu sources. Fixes that have to change third-party checkouts are stored as patches in `patches/android/` and applied by the build scripts, so the submodule pointers can stay on the upstream commits.

To update from Cemu Android:

```
git fetch upstream
git merge upstream/android-port
```

Then build with `build_libretro_android.ps1` or `build_libretro_android.sh`. Those scripts apply `patches/android/` again when a merge left the dependencies clean.

A fresh clone needs the submodules, which include vcpkg and the other libraries the core links:

```
git clone --recurse-submodules <this-repo>
```

Disc keys are not part of this repository. The core reads `keys.txt` from the libretro system directory (`Cemu/keys.txt` and the parent system directory).

# Contributing to Cemu Android/libretro

The Android/libretro Cemu fork and its native integration.

## Issues and discussions

Search existing reports before opening a new one. Use the bug/feature forms and fill in the required reproduction, revision, and environment fields. Questions and design proposals belong in [Discussions](https://github.com/Ahmed-Abousaif/cemu-libretro-android/discussions). Treat contributors respectfully and provide actionable feedback.

App installation, library UI, or general Omnidroid behavior can be reported in [Omnidroid](https://github.com/Ahmed-Abousaif/Omnidroid/issues). Keep shared-library or native-runtime reports here when the problem is isolated to this project.

## Development and validation

Follow README.md and .github/workflows/build-android-libretro.yml for the pinned native toolchain. Build the affected Android target and report device/renderer tests for input, presentation, audio, keyboard, or lifecycle changes. Keep upstream Cemu guidance and dependency pins intact.

Read the project's [README](../README.md) and Omnidroid's [contributor setup](https://github.com/Ahmed-Abousaif/Omnidroid#-for-contributors--developers). Preserve the existing license notices, upstream build instructions, and recorded nested dependency revisions. Keep user preferences, product skins, and emulator-specific routing in the frontend unless this repository owns that behavior.

## Pull requests

Create a topic branch from current main, commit focused changes, and open a PR against main. Describe the concrete problem, resulting behavior, compatibility impact, and exact validation results. Link related issues and include visual evidence when relevant.

The default branch requires at least one approving review and code-owner review; force pushes and branch deletion are disabled, matching Omnidroid's baseline. Existing CI must be considered during review. The PR template is guidance, not an automatic body validator. Maintainers retain the same administrative override as Omnidroid.

Publish dependency changes before proposing a parent-project pin. Do not commit private credentials, signing material, game files, firmware, BIOS, or console keys.

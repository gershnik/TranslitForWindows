# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

### Added
- Support for system input languages other than US English
- Support for multiple system languages

### Changed
- Updated dependencies
- Fixed minor installer issues with rollbacks and action scheduling

## [1.1] - 2026-05-09

### Added
- Hebrew: support for `־` (_maqaf_), `﬩` and `וֺ` (_vav_ with _holam haser_) characters
- Hebrew: Additional mappings for _geresh_ and _gershayim_ characters

### Changed
- Updated external dependencies.
- Hebrew: simplified typing _niqqud_ characters
- Internal: transliteration machinery now supports producing more than one destination character as
  output (for example to produce grapheme clusters).

### Fixed
- Releasing and pressing Shift or Alt keys no longer terminates composition

## [1.0] - 2025-06-27

### Added
- Initial version



[1.0]: https://github.com/gershnik/TranslitForWindows/releases/v1.0
[1.1]: https://github.com/gershnik/TranslitForWindows/releases/v1.1

# Credits

rice-gtk4 is distributed under GPL-3.0-or-later (see `LICENSE`, `NOTICE`).

The stylesheet and the generator are written from scratch. No code, stylesheet text, icon or
other asset of the projects below is copied into rice-gtk4. They were read as references, and
they inspired measurements, state behaviour and structure. They are credited here with thanks
to their authors. The licence of each is listed so that a reader can see what it is; none of
those licences applies to rice-gtk4.

Each entry gives the upstream repository, the commit that was read and the licence as stated in
that commit.

## Main inspirations

### GTK
- Upstream: https://gitlab.gnome.org/GNOME/gtk (commit `e47186d`, 2026-11-12, version 4.25.1).
- Licence: LGPL-2.1-or-later, as stated in GTK's `README.md` and `meson.build`. Some source files
  carry an LGPL-2.0-or-later header instead, file by file; `COPYING` is the LGPL 2 text.
- Inspired: the CSS node names, widget structure and CSS behaviour that the stylesheet targets
  (for example the scrolled window, search bar, window controls, colour and font dialog buttons,
  the CSS parser and selector rules).

### Bubble
- Upstream: https://github.com/i-mint/Bubble (commit `3597361`, 2026-10-27).
- Licence: GPL-3.0 (`LICENSE` is the GPL 3 text).
- Inspired: the inset focus ring pattern, and fill-only hover on window controls.

### celestial-gtk-theme
- Upstream: https://github.com/zquestz/celestial-gtk-theme (commit `9d0d205`, 2026-11-11).
- Licence: GPL-3.0 (`LICENSE.md` is the GPL 3 text).
- Inspired: the inset focus ring, and accent colour families for checked states.

### Material Components Web (Material Design 2)
- Upstream: https://github.com/material-components/material-components-web (commit `6ceadb8`,
  2025-01-07).
- Licence: MIT, Copyright (c) 2014-2020 Google, Inc.
- Inspired: square icon buttons, button and list padding proportions, the dense bar height, the
  radio dot proportion, and computed readable `on-X` text colours.

### Breeze GTK and Breeze (KDE)
- Upstream: https://github.com/KDE/breeze-gtk (commit `3acbaab`, 2026-09-23) and
  https://github.com/KDE/breeze (commit `3d03086`, 2026-11-12).
- Licence: not stated for the Breeze GTK stylesheets in the commit read. Its `LICENSES/` folder
  holds only `BSD-3-Clause` and `CC0-1.0`, and the only SPDX tag in it is `BSD-3-Clause` on a
  CMake file. Breeze lists `BSD-3-Clause`, `CC0-1.0`, `GPL-2.0-only`, `GPL-2.0-or-later`,
  `GPL-3.0-only`, `LicenseRef-KDE-Accepted-GPL` and `MIT` in its `LICENSES/` folder, chosen per
  file.
- Inspired: border and frame contrast handling and the colour mixing approach, which the
  designs compare against measured contrast.

## Also studied

These were read for the GTK 3 and GTK 4 engine work. The licence line is what the commit states.

| Project | Upstream | Commit | Licence |
|---|---|---|---|
| Squared | https://github.com/EliverLara/Squared | `d4f5b17` | GPL-3.0 (`LICENSE`) |
| Nordic | https://github.com/EliverLara/Nordic | `be5bda3` | GPL-3.0 (`LICENSE`) |
| human-theme | https://github.com/luigifab/human-theme | `fb7d504` | GPL-3.0 (`LICENSE`) |
| libadwaita | https://gitlab.gnome.org/GNOME/libadwaita | `39fff66` | LGPL-2.1-or-later (SPDX tag in sources) |
| adw-gtk3 | https://github.com/lassekongo83/adw-gtk3 | `47922ed` | LGPL-2.1 text in `LICENSE` |
| libadapta | https://github.com/xapp-project/libadapta | `ac83b1b` | LGPL-2.1-or-later (SPDX tag in sources) |
| Gradience | https://github.com/GradienceTeam/Gradience | `f965109` | GPL-3.0 (`LICENSE`) |
| arc-theme | https://github.com/jnsh/arc-theme | `94ac8c7` | GPL-3.0 (`COPYING`) |
| materia-theme | https://github.com/nana-4/materia-theme | `d7f59a3` | GPL-2.0 (`COPYING`) |
| mint-themes | https://github.com/linuxmint/mint-themes | `6c6095d` | no licence file in the commit read |

## Dependencies

Linked, not bundled; see `NOTICE`: rice-colors (GPL-3.0-or-later), rice-ini (GPL-3.0-or-later),
GTK 4 (LGPL-2.1-or-later).

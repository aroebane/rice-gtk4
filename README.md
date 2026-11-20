# rice-gtk4

The GTK 4 engine of the rice theming system: a compiled C generator that writes a
theme named `rice` from one config and one palette file. The stylesheet is written
from scratch. The only motion is a colour-only transition (see `[Motion]`). No external theme
engine.

- `rice-gtk4-css`: writes `<themes>/rice/gtk-4.0/` (`gtk.css`, `gtk-dark.css`) and
  `index.theme`; `--settings FILE` also writes a GTK `settings.ini`.
- `rice-gtk4-env [--config FILE] [--themes DIR] [--out FILE|-]`: writes `rice-gtk4.env`
  (`GTK_THEME=rice` and `XDG_DATA_DIRS`; default beside the config, `-` for stdout), or writes
  nothing and exits non-zero if the theme is not there. `--themes` defaults to `themes/` beside
  the config.

## Build

    make build
    make check          # format-check, test, lint (clang-tidy)
    make compdb         # compile_commands.json for clangd, via bear

Needs GLib, meson, ninja, and `rice-colors`, `rice-ini` and `rice-design` installed (found
with pkg-config; set `PKG_CONFIG_PATH` if they are outside the default path). Everything
lands in `.build/`.

## Install

    make install                 # PREFIX defaults to ~/.local
    make install PREFIX=<dir>

Binaries go to `PREFIX/bin`, samples to `PREFIX/share/rice-gtk4/`. `make uninstall`
removes them. Nothing is written to system directories or to your config, and no sudo
is needed.

## Use

    rice-gtk4-css --out ~/.themes        # writes ~/.themes/rice
    rice-gtk4-env                        # writes rice-gtk4.env beside the config
    GTK_THEME=rice <app>

GTK looks a theme up in `$XDG_DATA_HOME/themes`, `~/.themes`, then each
`$XDG_DATA_DIRS` entry + `/themes`. `rice-gtk4-env` exports the parent of `themes/`.

## Config

`$RICE_GTK4_CONFIG`, else `$XDG_CONFIG_HOME/rice/gtk4.conf`. Systemd-style INI; see
`data/gtk4.conf` (every value there is the built-in default). Sizes (control height,
padding, spacing, scrollbar width, indicator size) and `Radius` come from `[Shape]`;
the stylesheet derives its measurements from them.

Colors come from a palette file, the base layer: `[Source] Path=` (default
`gtk4-palette.ini` beside the config). It holds the thirteen palette roles and
`[Theme] Mode=` (light or dark); `[Colors]` in `gtk4.conf` overrides it key by key.
`[Style] ColorScheme=dark|light` in `gtk4.conf` overrides the palette's `Mode`. See
`data/gtk4-palette.ini`.

`[Fonts] General=` and `[Icons] Theme=` do not change the stylesheet: they are written to
the `settings.ini` that `rice-gtk4-css --settings FILE` produces (as `gtk-font-name` and
`gtk-icon-theme-name`). Without `[Fonts] General`, the design's `[Typography] General` is used.

`rice-gtk4-css` prints the config's problems to stderr, and the unknown keys with them. When
there are no problems, unknown keys are printed only if `RICE_GTK4_DEBUG` is set (any value).

## Designs

A design is an INI file of data that sets corner shapes and metrics; nothing in it runs.
Pick one in `gtk4.conf` with `[Design] Name=soft` (or `Path=` for a file), or with
`$RICE_GTK4_DESIGN` (name) / `$RICE_GTK4_DESIGN_PATH` (file), which win over the config.
A name is looked up as `<name>.ini` in `$RICE_GTK4_DESIGN_DIR`, then `<config dir>/designs`,
then `<prefix>/share/rice-gtk4/designs`. Three ship: `sharp` (the plain look, byte-identical
to no design), `soft`, `industrial`.

Layers, later wins: built-in defaults, the design (its `Inherits=` parents first), the
palette file, `gtk4.conf` (`[Shape]` keys and `[Shape.<family>]`). So `[Shape] Radius=` in the
config overrides the design's radii, and `[Shape] Spacing=` its metrics.

A design file reads (schema 1): `[Design]` Name, Schema, Family, Inherits; `[Radius]` the
scale none xs sm md lg xl xxl (px); `[Border]` Width; `[Metrics]` Spacing, Padding,
ControlHeight, ScrollBarWidth, IndicatorSize, FocusWidth, FocusOffset; `[Typography]`
General; `[Shape.<family>]` for window frame surface dialog headerbar button input menu
popover tooltip tab row badge indicator check radio switch scrollbar slider progress focus,
with Shape (sharp, rounded, chamfered, cut, capsule, asymmetric), Radius, Chamfer, Cut
(px, or `full`), and TopLeft/TopRight/BottomRight/BottomLeft as `<treatment> [px]`.
`Family=` is a preset for every family: sharp, rounded, chamfered, cut, asymmetric,
capsule, adaptive. Per family the order is: its `[Shape.<family>]` block, legacy
`[Shape] Radius`, the preset, the scale slot, the built-in default.

What GTK 4 CSS can draw is `border-radius` only (uniform or per corner; fractional px are
fine; border widths are whole px because GTK rounds them). The engine declares this as
capabilities (`fractional_radius` yes, `fractional_border` no, shown in `--dump-design`); a
toolkit that cannot draw a fraction gets it rounded and reported as a downgrade. So:

- chamfer and cut are drawn as a rounded corner of the same size (a size of 0 is sharp);
- a capsule is half of the shorter side where the box is known (check, radio, switch, slider,
  scrollbar) and a radius GTK clamps to the box otherwise. GTK scales all four corners by one
  factor when two neighbours do not fit, so this is a pill only when every corner of the
  family is a capsule. A capsule corner beside other corners in a box of unknown size is
  drawn as the largest scale radius and reported;
- asymmetric is per-corner;
- adaptive limits the radius to a third of the shorter side.

A radius is checked against the known box of the component and moved down to half of it.
Lengths are 0..200 px, metrics keep their ranges; a value outside is moved in and reported.

`rice-gtk4-css --fidelity` prints, per family, what was asked and what is drawn, then the
downgrades, clamps, unknown keys and problems. `--dump-design` prints the resolved design
as sorted `key=value` lines for diffing. Neither writes files. The dialog, headerbar and
indicator families resolve but no rule of this stylesheet uses them yet; `badge` is used only
for the keycaps of the shortcuts window.

GTK 4 CSS has no `outline-radius`: the focus ring takes the corners of the widget it surrounds
and grows them by the outline offset and width, so the `focus` family cannot be styled apart from
its widget. `--fidelity` says so instead of printing a radius for it.

## State behaviour (gtk4.conf)

Sections `[State]`, `[Contrast]`, `[Focus]`, `[Selection]` and `[Motion]` tune the state
model; see `data/gtk4.conf` for every key and default. The contrast guard raises a border,
focus, accent or primary colour that misses its floor and prints each correction
(`rice-gtk4-css` stderr); `Enforce=false` only reports. `[Motion]` turns the colour-only
transitions off (`Enabled=false`) or sets their length (`Duration`, ms; 0 means none).

`[State] StatusTint` sets how much of the status colour an infobar fill takes over the base;
the text on it is chosen readable on that fill.

## Emphasis and per-element overrides

`[Emphasis]` sets how a family rests: `Button`, `Input`, `Tab`, `Row` or `MenuItem` as
`outline`, `flat` or `solid`. Hover, pressed, checked and disabled keep the template's
rules, and the focus ring is not touched.

`[Element.<id>]` overrides one element of `data/element-hooks.yaml` (`Fill`, `Text`,
`Border`, `Radius`, `BorderWidth`, `PaddingX`, `PaddingY`, `MinHeight`, `Emphasis`). Only the
element's resting node is changed (hooks with a pseudo-class are left to the template). An
unknown id, an unknown key or a bad value is reported by `rice-gtk4-css` and skipped.

The contrast guard also covers them: when both `Fill` and `Text` are given and resolvable (a
role name or `#rrggbb`), a `Text` under the text floor is replaced by the readable colour,
and a `Border` under the boundary floor is raised; `Enforce=false` only reports. Not
checked: a `Text` or `Fill` given alone, `transparent`, alpha colours, and `[Emphasis]`
`flat` (it removes the boundary on request). `StatusTint` is lowered the same way when
status text on the tinted fill would miss the text floor.

## Licence

GPL-3.0-or-later (`LICENSE`). The stylesheet and generator are written from scratch; see
`NOTICE` and `CREDITS.md`.

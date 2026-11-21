project('rice-gtk4', ['c'],
  version: files('VERSION'),
  license: 'GPL-3.0-or-later',
  meson_version: '>=1.0',
  default_options: ['c_std=gnu11', 'warning_level=2'])

glib = dependency('glib-2.0', version: '>=2.66')
colors = dependency('rice-colors')
ini = dependency('rice-ini')
design = dependency('rice-design')
m = meson.get_compiler('c').find_library('m')

# The CSS template is compiled into the binary as a C byte array.
res = custom_target('rice-css-template',
  input: 'src/core/rice.css.in', output: 'css-template.c',
  command: [find_program('python3'), files('src/core/embed.py'), '@INPUT@', '@OUTPUT@', 'r4_css_template'])

elements = custom_target('rice-elements-table',
  input: 'data/element-hooks.yaml', output: 'elements-table.c',
  command: [find_program('python3'), files('src/core/elements.py'), '@INPUT@', '@OUTPUT@'])

deps = [colors, ini, design, glib, m]
core = static_library('rice-gtk4-core', 'src/core/theme.c', 'src/core/css.c', 'src/core/design.c', res, elements,
  dependencies: deps, pic: true, include_directories: include_directories('src/core'),
  c_args: ['-DR4_DESIGN_DIR="@0@"'.format(get_option('prefix') / get_option('datadir') / 'rice-gtk4' / 'designs')])
core_dep = declare_dependency(link_whole: core, include_directories: include_directories('src/core'),
  dependencies: deps)

executable('rice-gtk4-css', 'src/tools/rice-gtk4-css.c', dependencies: [core_dep], install: true)
executable('rice-gtk4-env', 'src/tools/rice-gtk4-env.c', dependencies: [core_dep], install: true)

install_data('data/gtk4.conf', 'data/gtk4-palette.ini', install_dir: get_option('datadir') / 'rice-gtk4')
install_data('data/designs/sharp.ini', 'data/designs/soft.ini', 'data/designs/industrial.ini',
  install_dir: get_option('datadir') / 'rice-gtk4' / 'designs')

if get_option('tests')
  subdir('tests')
endif

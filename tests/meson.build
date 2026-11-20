test('theme', executable('test-theme', 'test-theme.c', dependencies: [core_dep]))
test('design', executable('test-design', 'test-design.c', dependencies: [core_dep]),
  env: {'G_TEST_SRCDIR': meson.current_source_dir()})

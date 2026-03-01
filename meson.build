project('VFirewall', 'c',
  version: '0.1.0',
  meson_version: '>= 0.59.0',
  default_options: [ 'warning_level=2', 'c_std=gnu11' ],
)

# Dependencies
gtk_dep = dependency('gtk4')
adwaita_dep = dependency('libadwaita-1')
gio_dep = dependency('gio-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
notify_dep = dependency('libnotify')

# Application configurations
app_id = 'com.vaxp.VFirewall'

# Subdirectories
subdir('data')
subdir('src')

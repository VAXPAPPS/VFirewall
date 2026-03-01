vaxp_sources = [
  'main.c',
  'vaxp_window.c',
  'vaxp_ufw_backend.c',
  'vaxp_polkit_helper.c'
]

vaxp_deps = [
  gtk_dep,
  adwaita_dep,
  gio_dep,
  gio_unix_dep,
  notify_dep
]

gnome = import('gnome')
vaxp_resources = gnome.compile_resources('vaxp-firewall-resources',
  '../data/vaxp-firewall.gresource.xml',
  source_dir: '../data',
  c_name: 'vaxp_firewall'
)

executable('VFirewall',
  vaxp_sources,
  vaxp_resources,
  dependencies: vaxp_deps,
  install: true
)

import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk

# Unfortunately, PyGObject cannot easily attach to an external GTK4 process unless we use a debugging tool.

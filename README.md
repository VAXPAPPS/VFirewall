# VFirewall

**VFirewall** is a modern, graphical user interface (GUI) designed specifically for managing the Uncomplicated Firewall (UFW) in Linux systems. Built entirely using the **C** programming language and the latest **GTK4 + libadwaita** widget library, VFirewall offers an elegant, performant, and intuitive way to secure your workstation or server without touching the command line.

## 🌟 Key Features

*   **Sleek Modern Dashboard:** Utilize a beautiful dashboard with native dark-mode and custom translucent styling `rgba(0, 0, 0, 0.392)` that blends seamlessly with your desktop.
*   **One-Click Toggle:** Easily enable or disable your entire firewall service directly from the main interface.
*   **Default Policy Management:** Configure default incoming and outgoing behaviors (Allow/Deny/Reject) through simple dropdown menus.
*   **Live Traffic Monitoring:** Features a real-time Cairo-rendered network graph that displays current upload and download speeds (`RX/TX`) in KB/s.
*   **Application Profile Support (App Profiles):** Automatically detects UFW application profiles installed on the system (e.g., OpenSSH, Nginx, CUPS). You can allow or deny apps with a single click without remembering specific port numbers.
*   **Custom Rules Management:** Craft specific firewall rules based on exact ports and protocols (TCP/UDP/Any).
*   **Rule Management:** Easily view, inspect, and delete active network rules directly from the "Rules" tab.
*   **Desktop Notifications:** Integrated with `libnotify`, VFirewall sends instant, native desktop notifications whenever rules are added or deleted, or when the firewall changes state.
*   **Live Logs Viewer:** A dedicated logs viewer tab reads directly from `/var/log/ufw.log`, enabling you to inspect blocked or allowed packets in real time.
*   **Seamless Polkit Integration:** Employs PolicyKit mapping, running standard operations through `pkexec` without the need for repetitive root-password prompts for everyday administration.

## 📦 Installation
VFirewall provides a Debian package for one-step installation.

```bash
sudo dpkg -i vaxp-firewall_1.0-1_amd64.deb
```

## 🛠️ Building from Source

### Dependencies
You need the following libraries installed to build VFirewall:
*   `libgtk-4-dev`
*   `libnotify-dev`
*   `meson` & `ninja-build`
*   `ufw` & `policykit-1`

### Build Instructions
```bash
meson setup build
ninja -C build
# ./build/src/VFirewall
```


---
*Built for VAXP-OS*

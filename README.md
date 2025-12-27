# LS-Addon-ReShade

This is an addon for **LS Addons Manager (LosslessProxy)** designed to improve interaction with overlays like ReShade by managing input passthrough and cursor visibility.

## Features

*   **Input Passthrough**: Manages the passing of mouse and keyboard inputs to the game window or the overlay.
*   **Cursor Management**: Forces cursor visibility when passthrough is active, ensuring it is always visible when needed.
*   **Configurable Hotkey**: Hotkey (Default: `F9`) to toggle features on the fly.
*   **ImGui Interface**: Integrated configuration panel to modify settings in real-time.

---

<img width="2165" height="1371" alt="Screenshot 2025-12-27 233804" src="https://github.com/user-attachments/assets/db37a1b0-8ac8-4ac0-a300-00941d460b21" />

---

## Installation

1.  Ensure you have **LS Addons Manager (LosslessProxy)** installed.
2.  Download the latest release of `LS_ReShade.dll`.
3.  Copy the `LS_ReShade.dll` file into the `addons` folder of your LosslessProxy installation.
4.  Start LosslessProxy.

## Build

To build the project from source:

### Prerequisites
*   CMake 3.15+
*   C++17 compatible C++ compiler (e.g., MSVC)

### Instructions
1.  Clone the repository.
2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Generate project files:
    ```bash
    cmake ..
    ```
4.  Build:
    ```bash
    cmake --build . --config Release
    ```

## Configuration

Once the addon is loaded, you can access settings via the LosslessProxy/ImGui interface to configure:
*   Enable/Disable Input Passthrough.
*   Hotkey Key (Virtual Key).
*   Hotkey Modifiers (Ctrl, Alt, Shift).


## ⚠️ Disclaimer

This add-on is an unofficial extension for Lossless Scaling.

It is NOT affiliated with, endorsed by, or supported by
Lossless Scaling Developers in any way.

This add-on was developed through independent analysis and
reverse engineering of the software behavior. No proprietary
source code or assets are included.

Use at your own risk.

The author assumes no responsibility for any damage, data loss,
account bans, or other consequences resulting from the use of
this add-on.

This add-on may interact with the software at runtime in
non-documented ways.


## Legal Notice / Trademarks

All trademarks, product names, and company names are the property
of their respective owners and are used for identification purposes only.



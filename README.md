# LS-Addon-ReShade

This is an addon for **LS Addons Manager (LosslessProxy)** designed to improve interaction with overlays like ReShade by managing input passthrough and cursor visibility.

## Features

*   **Input Passthrough**: Manages the passing of mouse and keyboard inputs to the game window or the overlay.
*   **Cursor Management**: Forces cursor visibility when passthrough is active, ensuring it is always visible when needed.
*   **Configurable Hotkey**: Hotkey (Default: `F9`) to toggle features on the fly.
*   **ImGui Interface**: Integrated configuration panel to modify settings in real-time.

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

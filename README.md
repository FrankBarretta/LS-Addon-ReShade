# LS ReShade

**LS ReShade** is an addon for **Lossless Scaling** that allows users to **use ReShade directly on the Lossless Scaling overlay**.

After launching the addon, the user can press a **custom hotkey**, configurable from the addon’s menu.
When the hotkey is pressed:

* the **ReShade interface opens on top of the Lossless Scaling overlay**
* the user can **fully interact with ReShade using both keyboard and mouse**

Once the ReShade menu is closed, the same hotkey can be **pressed again to return to normal gameplay**, with the Lossless Scaling overlay still active.

This addon makes it possible to apply **ReShade effects directly to the Lossless Scaling overlay itself**, enabling advanced post-processing and extensive visual customization.

### Important note

**LS ReShade does not install or include ReShade.**
Users must **download and install ReShade separately** and configure it for **Lossless Scaling (DirectX 11)** before using this addon.

---

<img width="2165" height="1371" alt="Screenshot 2025-12-27 233804" src="https://github.com/user-attachments/assets/db37a1b0-8ac8-4ac0-a300-00941d460b21" />

---

## Installation

To use this addon, you must **first install the LS Addon Manager (LosslessProxy)**.

### 1. Install LS Addon Manager

Download the latest release of **LS Addon Manager (LosslessProxy)** from:
👉 [https://github.com/FrankBarretta/LS-Addons-Manager](https://github.com/FrankBarretta/LS-Addons-Manager)

Follow the instructions provided in the repository to complete the installation.

---

### 2. Install LS ReShade

1. Download **`LS_ReShade.dll`** from the addon project’s release page.
2. Go to the **main Lossless Scaling installation folder**.
3. Create a folder named:

   ```
   addons
   ```
4. Inside the `addons` folder, create another folder named:

   ```
   LS_ReShade
   ```
5. Place the downloaded **`LS_ReShade.dll`** file inside the `LS_ReShade` folder.

The final folder structure should look like this:

```
Lossless Scaling/
└── addons/
    └── LS_ReShade/
        └── LS_ReShade.dll
```

---

✅ **Done!**
The addon is now installed and ready to be used with Lossless Scaling.


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



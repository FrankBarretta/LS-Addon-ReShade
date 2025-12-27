#include "../../LosslessProxy/src/addon_api.hpp"
#include "logger.hpp"
#include <windows.h>
#include <thread>
#include <atomic>
#include <chrono>

#include <map>
#include <mutex>

#include "imgui.h"

std::atomic<bool> g_ThreadShouldExit = false;
std::thread g_Thread;
std::map<HWND, WNDPROC> g_SubclassedWindows;
std::mutex g_SubclassMutex;

// Store original window styles to restore on shutdown
struct WindowStyles {
    LONG_PTR exStyle;
    LONG_PTR style;
};
std::map<HWND, WindowStyles> g_OriginalStyles;
std::mutex g_StylesMutex;

// Settings - controlled by ImGui
std::atomic<bool> g_EnableInputPassthrough = false;  // Default: enabled
std::atomic<int> g_HotkeyVirtualKey = VK_F9;  // Default: F9 key
std::atomic<bool> g_HotkeyCtrl = false;
std::atomic<bool> g_HotkeyAlt = false;
std::atomic<bool> g_HotkeyShift = false;
ImGuiContext* g_ImGuiContext = nullptr;

// Hotkey state tracking
bool g_LastHotkeyState = false;

LRESULT CALLBACK NewWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Handle cursor visibility FIRST, regardless of addon state
    if (uMsg == WM_SETCURSOR || uMsg == WM_MOUSEMOVE) {
        if (g_EnableInputPassthrough) {
            // Only force cursor when passthrough is enabled
            SetCursor(LoadCursor(NULL, IDC_ARROW));

            CURSORINFO ci = { sizeof(CURSORINFO) };
            if (GetCursorInfo(&ci)) {
                if (!(ci.flags & CURSOR_SHOWING)) {
                    while (ShowCursor(TRUE) < 0);
                }
            }

            // Aggressively unclip cursor on every move to fight LS clipping
            ClipCursor(NULL);

            if (uMsg == WM_SETCURSOR) return TRUE; // Prevent default handling
        }
    }

    if (uMsg == WM_LBUTTONDOWN && g_EnableInputPassthrough) {
        Log("WM_LBUTTONDOWN received on window %p", hwnd);
    }

    WNDPROC oldProc = NULL;
    {
        std::lock_guard<std::mutex> lock(g_SubclassMutex);
        auto it = g_SubclassedWindows.find(hwnd);
        if (it != g_SubclassedWindows.end()) {
            oldProc = it->second;
        }
    }

    // Critical fix: If we don't have oldProc in our map, this window wasn't subclassed yet
    // This can happen briefly when a new window is created before our loop catches it
    // In this case, read the CURRENT wndproc from the window (which might be LS's or another subclasser)
    if (!oldProc) {
        WNDPROC currentProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
        // If the current proc is OUR proc, it means we're in a recursive call
        // This shouldn't happen normally, but protect against infinite loop
        if (currentProc != NewWndProc) {
            oldProc = currentProc;
            Log("NewWndProc called for unknown window %p, using current proc %p", hwnd, currentProc);
        }
    }

    if (oldProc) {
        LRESULT ret = CallWindowProc(oldProc, hwnd, uMsg, wParam, lParam);

        // Fix click-through ONLY when passthrough is enabled
        if (g_EnableInputPassthrough && uMsg == WM_NCHITTEST && ret == HTTRANSPARENT) {
            return HTCLIENT;
        }
        return ret;
    }

    // Last resort fallback
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void SubclassWindow(HWND hwnd) {
    std::lock_guard<std::mutex> lock(g_SubclassMutex);

    // Always get the current WNDPROC to ensure we have the latest one
    WNDPROC currentProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);

    // If it's already our proc, check if we have it in our map
    if (currentProc == NewWndProc) {
        // Verify we have the original proc saved
        auto it = g_SubclassedWindows.find(hwnd);
        if (it != g_SubclassedWindows.end()) {
            return; // Already subclassed and saved
        } else {
            // Our proc is installed but we don't have the original saved!
            // This shouldn't happen, but log it
            Log("WARNING: NewWndProc already installed on %p but not in map!", hwnd);
            return; // Can't do anything about it
        }
    }

    // Check if we already have this window in our map with a different proc
    // This means the window's WNDPROC was changed by someone else after we subclassed it
    auto it = g_SubclassedWindows.find(hwnd);
    if (it != g_SubclassedWindows.end()) {
        // Update the saved proc to the current one
        Log("Window %p WNDPROC changed from %p to %p, updating", hwnd, it->second, currentProc);
        it->second = currentProc;
    }

    // Install our proc and save the current one
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)NewWndProc);
    if (oldProc && oldProc != NewWndProc) {
        g_SubclassedWindows[hwnd] = oldProc;
        Log("Subclassed window: %p (saved proc: %p)", hwnd, oldProc);
    }
}

BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam) {
    SubclassWindow(hwnd);
    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) {
        if (IsWindowVisible(hwnd)) {
            LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            LONG_PTR styleNormal = GetWindowLongPtr(hwnd, GWL_STYLE);

            // Check if we need to save original styles
            {
                std::lock_guard<std::mutex> lock(g_StylesMutex);
                if (g_OriginalStyles.find(hwnd) == g_OriginalStyles.end()) {
                    // Save original styles on first encounter
                    WindowStyles original;
                    original.exStyle = exStyle;
                    original.style = styleNormal;
                    g_OriginalStyles[hwnd] = original;
                    Log("Saved original styles for window %p: exStyle=%llx, style=%llx", hwnd, exStyle, styleNormal);
                }
            }

            LONG_PTR newExStyle = exStyle;
            LONG_PTR newStyle = styleNormal;
            bool needsStyleChange = false;

            if (exStyle & WS_EX_TRANSPARENT) {
                newExStyle &= ~WS_EX_TRANSPARENT;
                needsStyleChange = true;
            }

            if (exStyle & WS_EX_NOACTIVATE) {
                newExStyle &= ~WS_EX_NOACTIVATE;
                needsStyleChange = true;
            }

            if (exStyle & WS_EX_LAYERED) {
                newExStyle &= ~WS_EX_LAYERED;
                needsStyleChange = true;
            }

            if (styleNormal & WS_DISABLED) {
                newStyle &= ~WS_DISABLED;
                needsStyleChange = true;
            }

            // Apply style changes if needed
            if (needsStyleChange) {
                char title[256];
                GetWindowTextA(hwnd, title, sizeof(title));

                if (newStyle != styleNormal) {
                    SetWindowLongPtr(hwnd, GWL_STYLE, newStyle);
                }
                if (newExStyle != exStyle) {
                    SetWindowLongPtr(hwnd, GWL_EXSTYLE, newExStyle);
                }

                // Force window update and Z-order to TOPMOST
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

                // Try to bring to foreground to capture keyboard input
                SetForegroundWindow(hwnd);
                SetFocus(hwnd);
            }

            // ALWAYS subclass AFTER style changes
            // This ensures the window can receive input after we modified its styles
            SubclassWindow(hwnd);
            EnumChildWindows(hwnd, EnumChildWindowsProc, 0);
        }
    }
    return TRUE;
}

BOOL CALLBACK RestoreWindowStylesProc(HWND hwnd, LPARAM lParam) {
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) {
        if (IsWindowVisible(hwnd)) {
            std::lock_guard<std::mutex> lock(g_StylesMutex);
            auto it = g_OriginalStyles.find(hwnd);
            if (it != g_OriginalStyles.end()) {
                // Restore original styles
                const WindowStyles& original = it->second;

                LONG_PTR currentExStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
                LONG_PTR currentStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

                bool needsRestore = false;

                if (currentExStyle != original.exStyle) {
                    SetWindowLongPtr(hwnd, GWL_EXSTYLE, original.exStyle);
                    needsRestore = true;
                }

                if (currentStyle != original.style) {
                    SetWindowLongPtr(hwnd, GWL_STYLE, original.style);
                    needsRestore = true;
                }

                if (needsRestore) {
                    // Force window update
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                    Log("Restored original styles for window %p", hwnd);
                }
            }

            // Also restore child windows
            EnumChildWindows(hwnd, RestoreWindowStylesProc, 0);
        }
    }
    return TRUE;
}

void RestoreAllWindowStyles() {
    Log("Restoring all window styles to original state");
    EnumWindows(RestoreWindowStylesProc, 0);
}

bool CheckHotkey() {
    int vk = g_HotkeyVirtualKey.load();
    if (vk == 0) return false; // No hotkey set

    // Check if the main key is pressed
    bool keyPressed = (GetAsyncKeyState(vk) & 0x8000) != 0;

    // Check modifiers
    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    // Check if all required modifiers match
    bool ctrlMatch = g_HotkeyCtrl.load() == ctrlPressed;
    bool altMatch = g_HotkeyAlt.load() == altPressed;
    bool shiftMatch = g_HotkeyShift.load() == shiftPressed;

    return keyPressed && ctrlMatch && altMatch && shiftMatch;
}

void InputBlockerLoop() {
    Log("InputBlocker thread started (persistent)");
    int cleanupCounter = 0;
    bool previousPassthroughState = g_EnableInputPassthrough.load();

    while (!g_ThreadShouldExit) {
        // Check hotkey (works whether passthrough is enabled or not)
        bool hotkeyPressed = CheckHotkey();
        if (hotkeyPressed && !g_LastHotkeyState) {
            // Hotkey just pressed (rising edge)
            bool newState = !g_EnableInputPassthrough.load();
            g_EnableInputPassthrough = newState;
            Log("Hotkey pressed - Input passthrough %s", newState ? "ENABLED" : "DISABLED");
        }
        g_LastHotkeyState = hotkeyPressed;

        // Check if passthrough state changed
        bool currentPassthroughState = g_EnableInputPassthrough.load();
        if (currentPassthroughState != previousPassthroughState) {
            if (!currentPassthroughState) {
                // Passthrough was just disabled - restore original window styles
                Log("Passthrough disabled - restoring original window styles");
                RestoreAllWindowStyles();
            } else {
                // Passthrough was just enabled
                Log("Passthrough enabled - will apply window modifications");
            }
            previousPassthroughState = currentPassthroughState;
        }

        if (g_EnableInputPassthrough) {
            // Passthrough is enabled - do the work
            EnumWindows(EnumWindowsProc, 0);

            // Periodically clean up dead windows from our maps (every 100 iterations = ~1 second)
            if (++cleanupCounter >= 100) {
                cleanupCounter = 0;
                {
                    std::lock_guard<std::mutex> lock(g_SubclassMutex);
                    auto it = g_SubclassedWindows.begin();
                    while (it != g_SubclassedWindows.end()) {
                        if (!IsWindow(it->first)) {
                            Log("Removing dead window %p from subclass map", it->first);
                            it = g_SubclassedWindows.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(g_StylesMutex);
                    auto it = g_OriginalStyles.begin();
                    while (it != g_OriginalStyles.end()) {
                        if (!IsWindow(it->first)) {
                            Log("Removing dead window %p from styles map", it->first);
                            it = g_OriginalStyles.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }

            // Force cursor visibility
            CURSORINFO ci = { sizeof(CURSORINFO) };
            if (GetCursorInfo(&ci)) {
                if (!(ci.flags & CURSOR_SHOWING)) {
                    while (ShowCursor(TRUE) < 0);
                }
            }

            // Force cursor shape
            SetCursor(LoadCursor(NULL, IDC_ARROW));

            // Unconditionally unclip cursor to fix alignment issues
            ClipCursor(NULL);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            // Passthrough is disabled - sleep longer to save CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    Log("InputBlocker thread exiting");
}

extern "C" __declspec(dllexport) void AddonInitialize(IHost* host, ImGuiContext* ctx, void* alloc_func, void* free_func, void* user_data) {
    // Get the path of this DLL
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&AddonInitialize, &hModule);

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);

    // Remove filename to get directory
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }

    std::wstring logPath = std::wstring(path) + L"LS_ReShade.log";
    Logger::Init(logPath);

    Log("======== AddonInitialize called ========");
    Log("Log file path: %S", logPath.c_str());

    // Store ImGui context
    g_ImGuiContext = ctx;

    // Check if thread needs to be started
    static bool threadStarted = false;
    if (!threadStarted) {
        Log("Starting persistent worker thread...");
        g_Thread = std::thread(InputBlockerLoop);
        g_Thread.detach();
        threadStarted = true;
        Log("Thread started");
    } else {
        Log("AddonInitialize called again, thread already running");
    }

    Log("Addon initialization complete");
}

extern "C" __declspec(dllexport) void AddonShutdown() {
    Log("AddonShutdown called - stopping thread");

    // Signal thread to exit
    g_ThreadShouldExit = true;

    // Wait a bit for thread to exit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Restore all WNDPROCs
    {
        std::lock_guard<std::mutex> lock(g_SubclassMutex);
        Log("Restoring %zu subclassed windows", g_SubclassedWindows.size());
        for (auto const& [hwnd, oldProc] : g_SubclassedWindows) {
            if (IsWindow(hwnd)) {
                WNDPROC current = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
                if (current == NewWndProc) {
                    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)oldProc);
                    Log("Restored WndProc for window: %p", hwnd);
                }
            }
        }
        g_SubclassedWindows.clear();
    }

    Log("Addon shutdown complete");
    Logger::Close();
}

const char* GetKeyName(int vk) {
    static char buffer[32];
    switch (vk) {
        case 0: return "None";
        case VK_F1: return "F1";
        case VK_F2: return "F2";
        case VK_F3: return "F3";
        case VK_F4: return "F4";
        case VK_F5: return "F5";
        case VK_F6: return "F6";
        case VK_F7: return "F7";
        case VK_F8: return "F8";
        case VK_F9: return "F9";
        case VK_F10: return "F10";
        case VK_F11: return "F11";
        case VK_F12: return "F12";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        default:
            if (vk >= 'A' && vk <= 'Z') {
                sprintf_s(buffer, "%c", vk);
                return buffer;
            }
            sprintf_s(buffer, "Key %d", vk);
            return buffer;
    }
}

extern "C" __declspec(dllexport) void AddonRenderSettings() {
    if (!g_ImGuiContext) return;

    ImGui::SetCurrentContext(g_ImGuiContext);

    ImGui::TextWrapped("Input Blocker allows you to interact with the Lossless Scaling overlay instead of the game behind it.");
    ImGui::Separator();

    bool enabled = g_EnableInputPassthrough.load();
    if (ImGui::Checkbox("Enable Input Passthrough to Overlay", &enabled)) {
        g_EnableInputPassthrough = enabled;
        Log("Input passthrough %s via settings UI", enabled ? "ENABLED" : "DISABLED");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextWrapped("Hotkey Configuration:");

    // Modifiers
    bool ctrl = g_HotkeyCtrl.load();
    bool alt = g_HotkeyAlt.load();
    bool shift = g_HotkeyShift.load();

    if (ImGui::Checkbox("Ctrl", &ctrl)) {
        g_HotkeyCtrl = ctrl;
        Log("Hotkey modifier Ctrl: %s", ctrl ? "ON" : "OFF");
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Alt", &alt)) {
        g_HotkeyAlt = alt;
        Log("Hotkey modifier Alt: %s", alt ? "ON" : "OFF");
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Shift", &shift)) {
        g_HotkeyShift = shift;
        Log("Hotkey modifier Shift: %s", shift ? "ON" : "OFF");
    }

    // Key selection
    int currentKey = g_HotkeyVirtualKey.load();
    const char* currentKeyName = GetKeyName(currentKey);

    if (ImGui::BeginCombo("Key", currentKeyName)) {
        const int keys[] = {
            0, // None
            VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6,
            VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
            VK_INSERT, VK_DELETE, VK_HOME, VK_END,
            VK_PRIOR, VK_NEXT,
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
        };

        for (int vk : keys) {
            const char* keyName = GetKeyName(vk);
            bool isSelected = (currentKey == vk);
            if (ImGui::Selectable(keyName, isSelected)) {
                g_HotkeyVirtualKey = vk;
                Log("Hotkey key changed to: %s", keyName);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    // Display current hotkey
    ImGui::Spacing();
    char hotkeyDisplay[128];
    if (currentKey == 0) {
        sprintf_s(hotkeyDisplay, "Current Hotkey: None (toggle disabled)");
    } else {
        sprintf_s(hotkeyDisplay, "Current Hotkey: %s%s%s%s",
            ctrl ? "Ctrl + " : "",
            alt ? "Alt + " : "",
            shift ? "Shift + " : "",
            currentKeyName);
    }
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", hotkeyDisplay);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextWrapped("\nWhen enabled:\n- Mouse and keyboard input goes to the LS overlay\n- Cursor is visible and free\n- You can interact with overlay UI");
    ImGui::TextWrapped("\nWhen disabled:\n- Input goes to the game behind the overlay\n- Normal Lossless Scaling behavior");
}

extern "C" __declspec(dllexport) uint32_t GetAddonCapabilities() {
    return ADDON_CAP_HAS_SETTINGS;
}

extern "C" __declspec(dllexport) const char* GetAddonName() {
    return "Input Blocker";
}

extern "C" __declspec(dllexport) const char* GetAddonVersion() {
    return "0.1.0";
}

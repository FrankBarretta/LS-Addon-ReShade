#include "../../../LosslessProxy/src/addon_api.hpp"
#include "logger.hpp"
#include "imgui.h"
#include <windows.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <functional>

// --- Global Settings ---
struct Settings {
    std::atomic<bool> threadRunning{ true };
    std::atomic<bool> inputPassthrough{ false };
    std::atomic<int> hotkeyVk{ VK_HOME };
    std::atomic<bool> hotkeyCtrl{ false };
    std::atomic<bool> hotkeyAlt{ false };
    std::atomic<bool> hotkeyShift{ false };
    std::atomic<bool> autoClickRepress{ true };
    std::atomic<bool> isSimulating{ false };
} g_Settings;

ImGuiContext* g_ImGuiContext = nullptr;

// --- Input Simulation Helper ---
namespace InputSim {
    void SendKey(WORD vk, bool down) {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    void Click() {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    void PressCombo(int vk, bool ctrl, bool alt, bool shift) {
        if (ctrl) SendKey(VK_CONTROL, true);
        if (alt) SendKey(VK_MENU, true);
        if (shift) SendKey(VK_SHIFT, true);

        SendKey(vk, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        SendKey(vk, false);

        if (shift) SendKey(VK_SHIFT, false);
        if (alt) SendKey(VK_MENU, false);
        if (ctrl) SendKey(VK_CONTROL, false);
    }
}

// --- Window Management Helper ---
class WindowManager {
    struct WindowState {
        WNDPROC originalProc = nullptr;
        LONG_PTR originalStyle = 0;
        LONG_PTR originalExStyle = 0;
        bool stylesModified = false;
        bool activated = false;
    };

    static std::map<HWND, WindowState> windows;
    static std::mutex mutex;

    // Custom Window Procedure
    static LRESULT CALLBACK HookProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        // 1. Handle Cursor Visibility & Clipping (Priority)
        if (g_Settings.inputPassthrough) {
            if (uMsg == WM_SETCURSOR || uMsg == WM_MOUSEMOVE) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                CURSORINFO ci = { sizeof(CURSORINFO) };
                if (GetCursorInfo(&ci) && !(ci.flags & CURSOR_SHOWING)) {
                    while (ShowCursor(TRUE) < 0);
                }
                ClipCursor(NULL);
                if (uMsg == WM_SETCURSOR) return TRUE;
            }
        }

        // 2. Retrieve Original WndProc
        WNDPROC oldProc = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = windows.find(hwnd);
            if (it != windows.end()) oldProc = it->second.originalProc;
        }

        // Fallback if not found (shouldn't happen often)
        if (!oldProc) {
            oldProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
            if (oldProc == HookProc) return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        // 3. Call Original WndProc
        LRESULT ret = CallWindowProc(oldProc, hwnd, uMsg, wParam, lParam);

        // 4. Fix Click-Through
        if (g_Settings.inputPassthrough && uMsg == WM_NCHITTEST && ret == HTTRANSPARENT) {
            return HTCLIENT;
        }

        return ret;
    }

public:
    static void ProcessWindow(HWND hwnd) {
        // 1. Subclassing
        WNDPROC currentProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
        if (currentProc != HookProc) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                windows[hwnd].originalProc = currentProc;
            }
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)HookProc);
        }

        // 2. Style Modification
        if (g_Settings.inputPassthrough) {
            LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);

            bool isNew = false;
            bool isOverlay = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                WindowState& state = windows[hwnd];
                if (!state.stylesModified) {
                    state.originalExStyle = exStyle;
                    state.originalStyle = style;
                    state.stylesModified = true;
                }
                if (!state.activated) {
                    isNew = true;
                    state.activated = true;
                }
                isOverlay = (state.originalExStyle & (WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE)) != 0;
            }

            LONG_PTR newExStyle = exStyle & ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_LAYERED);
            LONG_PTR newStyle = style & ~WS_DISABLED;

            bool stylesChanged = (newExStyle != exStyle || newStyle != style);

            if (stylesChanged) {
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, newExStyle);
                SetWindowLongPtr(hwnd, GWL_STYLE, newStyle);
            }

            if (stylesChanged || isNew) {
                if (isOverlay) {
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
                    
                    // Force Foreground
                    DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
                    DWORD curThread = GetCurrentThreadId();
                    if (foreThread != curThread) {
                        AttachThreadInput(foreThread, curThread, TRUE);
                        BringWindowToTop(hwnd);
                        SetForegroundWindow(hwnd);
                        AttachThreadInput(foreThread, curThread, FALSE);
                    } else {
                        SetForegroundWindow(hwnd);
                    }
                } else if (stylesChanged) {
                    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOZORDER);
                }
            }
        }
    }

    static void RestoreAll() {
        std::vector<std::pair<HWND, WindowState>> toRestore;
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& pair : windows) {
                toRestore.push_back(pair);
            }
        }

        Log("Restoring %zu windows...", toRestore.size());
        
        for (const auto& item : toRestore) {
            HWND hwnd = item.first;
            const WindowState& state = item.second;

            if (!IsWindow(hwnd)) continue;

            // Restore Styles
            if (state.stylesModified) {
                SetWindowLongPtr(hwnd, GWL_EXSTYLE, state.originalExStyle);
                SetWindowLongPtr(hwnd, GWL_STYLE, state.originalStyle);
                SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }

            // Restore WndProc
            if (GetWindowLongPtr(hwnd, GWLP_WNDPROC) == (LONG_PTR)HookProc) {
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)state.originalProc);
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            windows.clear();
        }
    }

    static void CleanupDeadWindows() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = windows.begin(); it != windows.end();) {
            if (!IsWindow(it->first)) it = windows.erase(it);
            else ++it;
        }
    }
};

std::map<HWND, WindowManager::WindowState> WindowManager::windows;
std::mutex WindowManager::mutex;

// --- Main Logic ---

void WorkerThread() {
    Log("Worker thread started");
    bool lastHotkeyState = false;
    int cleanupCounter = 0;

    while (g_Settings.threadRunning) {
        // 1. Check Hotkey
        bool hotkeyPressed = false;
        if (g_Settings.hotkeyVk != 0) {
            bool k = (GetAsyncKeyState(g_Settings.hotkeyVk) & 0x8000) != 0;
            bool c = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool a = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool s = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            
            if (k && 
                c == g_Settings.hotkeyCtrl.load() && 
                a == g_Settings.hotkeyAlt.load() && 
                s == g_Settings.hotkeyShift.load()) {
                hotkeyPressed = true;
            }
        }

        // 2. Handle Toggle
        if (hotkeyPressed && !lastHotkeyState && !g_Settings.isSimulating) {
            bool newState = !g_Settings.inputPassthrough;
            g_Settings.inputPassthrough = newState;
            Log("Passthrough toggled: %s", newState ? "ON" : "OFF");

            if (!newState) {
                WindowManager::RestoreAll();
            }

            // Auto Click & Repress Logic
            if (g_Settings.autoClickRepress) {
                std::thread([newState]() {
                    g_Settings.isSimulating = true;
                    int vk = g_Settings.hotkeyVk;
                    bool c = g_Settings.hotkeyCtrl;
                    bool a = g_Settings.hotkeyAlt;
                    bool s = g_Settings.hotkeyShift;

                    if (newState) { // ON
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        InputSim::Click();
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        InputSim::PressCombo(vk, c, a, s);
                    } else { // OFF
                        std::this_thread::sleep_for(std::chrono::milliseconds(450));
                        InputSim::Click();
                    }
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    g_Settings.isSimulating = false;
                }).detach();
            }
        }
        lastHotkeyState = hotkeyPressed;

        // 3. Active Loop
        if (g_Settings.inputPassthrough) {
            // Enumerate and process windows
            EnumWindows([](HWND hwnd, LPARAM) -> BOOL {
                DWORD pid;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd)) {
                    WindowManager::ProcessWindow(hwnd);
                    EnumChildWindows(hwnd, [](HWND c, LPARAM) -> BOOL {
                        WindowManager::ProcessWindow(c);
                        return TRUE;
                    }, 0);
                }
                return TRUE;
            }, 0);

            // Cleanup occasionally
            if (++cleanupCounter >= 100) {
                WindowManager::CleanupDeadWindows();
                cleanupCounter = 0;
            }

            // Force cursor
            ClipCursor(NULL);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    Log("Worker thread stopped");
}

// --- Exports ---

extern "C" __declspec(dllexport) void AddonInitialize(IHost* host, ImGuiContext* ctx, void* alloc_func, void* free_func, void* user_data) {
    // Init Logger
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&AddonInitialize, &hModule);
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hModule, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    Logger::Init(std::wstring(path) + L"LS_ReShade.log");
    
    Log("Addon Initialized");
    g_ImGuiContext = ctx;

    // Start Thread
    static bool threadStarted = false;
    if (!threadStarted) {
        std::thread(WorkerThread).detach();
        threadStarted = true;
    }
}

extern "C" __declspec(dllexport) void AddonShutdown() {
    g_Settings.threadRunning = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    WindowManager::RestoreAll();
    Logger::Close();
}

extern "C" __declspec(dllexport) uint32_t GetAddonCapabilities() {
    return ADDON_CAP_HAS_SETTINGS;
}

extern "C" __declspec(dllexport) const char* GetAddonName() {
    return "Input Blocker (Optimized)";
}

extern "C" __declspec(dllexport) const char* GetAddonVersion() {
    return "0.2.0";
}

// --- UI Helper ---
const char* GetKeyName(int vk) {
    static char buffer[32];
    if (vk == 0) return "None";
    if (vk >= VK_F1 && vk <= VK_F12) { sprintf_s(buffer, "F%d", vk - VK_F1 + 1); return buffer; }
    switch (vk) {
        case VK_INSERT: return "Insert"; case VK_DELETE: return "Delete";
        case VK_HOME: return "Home"; case VK_END: return "End";
        case VK_PRIOR: return "Page Up"; case VK_NEXT: return "Page Down";
    }
    if (vk >= 'A' && vk <= 'Z') { sprintf_s(buffer, "%c", vk); return buffer; }
    sprintf_s(buffer, "Key %d", vk);
    return buffer;
}

extern "C" __declspec(dllexport) void AddonRenderSettings() {
    if (!g_ImGuiContext) return;
    ImGui::SetCurrentContext(g_ImGuiContext);

    ImGui::TextWrapped("Input Blocker allows interaction with the overlay.");
    ImGui::Separator();

    bool enabled = g_Settings.inputPassthrough;
    if (ImGui::Checkbox("Enable Input Passthrough", &enabled)) {
        g_Settings.inputPassthrough = enabled;
        if (!enabled) WindowManager::RestoreAll();
    }

    bool autoClick = g_Settings.autoClickRepress;
    if (ImGui::Checkbox("Enable Auto Click & Repress", &autoClick)) g_Settings.autoClickRepress = autoClick;

    ImGui::Separator();
    ImGui::Text("Hotkey:");
    
    bool c = g_Settings.hotkeyCtrl;
    bool a = g_Settings.hotkeyAlt;
    bool s = g_Settings.hotkeyShift;
    if (ImGui::Checkbox("Ctrl", &c)) g_Settings.hotkeyCtrl = c;
    ImGui::SameLine();
    if (ImGui::Checkbox("Alt", &a)) g_Settings.hotkeyAlt = a;
    ImGui::SameLine();
    if (ImGui::Checkbox("Shift", &s)) g_Settings.hotkeyShift = s;

    int currentKey = g_Settings.hotkeyVk;
    if (ImGui::BeginCombo("Key", GetKeyName(currentKey))) {
        const int keys[] = { 0, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12, VK_INSERT, VK_DELETE, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z' };
        for (int vk : keys) {
            if (ImGui::Selectable(GetKeyName(vk), currentKey == vk)) g_Settings.hotkeyVk = vk;
        }
        ImGui::EndCombo();
    }
}

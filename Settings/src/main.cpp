// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Common/err.h>
#include <Common/com.h>

#include "main.h"
#include "MainWindow.h"

path g_dataFolder;
unique_brush g_windowBrush;
COLORREF g_windowColor = 0;
COLORREF g_textColor = 0;
BOOL g_isDarkMode = FALSE;

ULONG Module::add_ref() noexcept
    { return 1; }
ULONG Module::sub_ref() noexcept
    { return 1; }


void applyUISettings() {
    winrt::Windows::UI::ViewManagement::UISettings settings;
    auto colorObj = settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Background);
    g_windowColor = RGB(colorObj.R, colorObj.G, colorObj.B);
    g_windowBrush.reset(CreateSolidBrush(g_windowColor));
    if (!g_windowBrush)
        throwLastError();

    g_isDarkMode = (((5 * colorObj.G) + (2 * colorObj.R) + colorObj.B) <= (8 * 128));

    colorObj = settings.GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Foreground);
    g_textColor = RGB(colorObj.R, colorObj.G, colorObj.B);
}

class RootWindow : public Window<RootWindow> {
    friend Window;
    friend ref_counted;

private:
    static void fillClassInfo(WNDCLASSEX & cls) {
        cls.lpszClassName = L"TranslitRootWindow";
    }
public:
    static refcnt_ptr<RootWindow> create() {
        return Window::create(0, nullptr, WS_OVERLAPPED, 
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                              nullptr, nullptr, nullptr);
    }
private:
    RootWindow(HWND hwnd, CREATESTRUCT & /*cs*/):
        Window(hwnd)
    {}
    ~RootWindow() = default;

    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        switch(msg) {
        case WM_WININICHANGE:
            applyUISettings();
            return 0;
        }
        return Window::handleMessage(msg, wParam, lParam);
    }

};



int APIENTRY wWinMain([[maybe_unused]] _In_ HINSTANCE hInstance,
                      [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance,
                      [[maybe_unused]] _In_ LPWSTR    lpCmdLine,
                      [[maybe_unused]] _In_ int nCmdShow) {

    auto existing = FindWindow(MainWindow::className, nullptr);
    if (existing) {
        FLASHWINFO fi{};
        fi.cbSize = sizeof(fi);
        fi.hwnd = existing;
        fi.dwFlags = FLASHW_ALL;
        fi.uCount = 3;
        fi.dwTimeout = 200;
        FlashWindowEx(&fi);
        return EXIT_SUCCESS;
    }

    comTest(CoInitialize(nullptr));

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        abort();

    INITCOMMONCONTROLSEX icx;
    icx.dwSize = sizeof(icx);
    icx.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&icx))
        abort();

    unqiue_co_membuf<wchar_t[]> pathBuf;
    comTest(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE | KF_FLAG_INIT, nullptr, std::out_ptr(pathBuf)));
    path localAppdata(pathBuf.get());
    path dataFolder = localAppdata / L"Translit";
    std::error_code ec;
    create_directories(dataFolder, ec);
    if (ec) {
        MessageBox(nullptr, 
                   sys_string::std_format("Unable to create {}\nError 0x{:08x}", dataFolder.string(), ec.value()).w_str(),
                   L"Translit: Fatal Error", MB_ICONHAND);
        return EXIT_FAILURE;
    }
    g_dataFolder = std::move(dataFolder);

    unqiue_co_membuf<wchar_t []> verInfo;
    auto hres = GetAvailableCoreWebView2BrowserVersionString(nullptr, std::out_ptr(verInfo));
    if (FAILED(hres) || !verInfo) {
        MessageBox(nullptr, 
                   L"WebView2 is not present on your machine. "
                      "Please ensure it is fully updated and Microsoft Edge browser is installed "
                      "(it doesn't need to be default)",
                   L"Translit: Fatal Error", MB_ICONHAND);
        return EXIT_FAILURE;
    }

    applyUISettings();

    auto rootWindow = RootWindow::create();

    auto mainWindow = MainWindow::create(rootWindow->handle());

    ShowWindow(mainWindow->handle(), SW_SHOWNORMAL);
    UpdateWindow(mainWindow->handle());

    int ret = EXIT_SUCCESS;
    for ( ; ; ) {
        MSG msg;
        BOOL res = GetMessage(&msg, nullptr, 0, 0);
        if (res == 0) {
            ret = int(msg.wParam);
            break;
        }
        if (res == -1)
            throwLastError();

        if (!IsDialogMessage(mainWindow->handle(), &msg)) {
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }
    }

    return ret;
}


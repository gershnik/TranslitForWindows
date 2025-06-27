// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later


#include "AboutWindow.h"
#include "main.h"
#include "ImageUtil.h"
#include "Util.h"

#include "../res/resource.h"

void AboutWindow::showModal(HWND hwndParent) {
    auto about = Window::create(WS_EX_DLGMODALFRAME | WS_EX_OVERLAPPEDWINDOW, L"About Translit",
                                WS_OVERLAPPED | WS_POPUP | WS_CAPTION | WS_SYSMENU, 
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                                hwndParent, nullptr, nullptr);
    ShowWindow(about->handle(), SW_SHOWNORMAL);
    UpdateWindow(about->handle());
    EnableWindow(hwndParent, false);
    NotifyWinEvent(EVENT_SYSTEM_DIALOGSTART, about->handle(), 0, 0);
    bool hadMessage = false;
    for ( ; ; ) {
        
        if (about->m_ended)
            break;
        
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

            hadMessage = true;
            
            if (msg.message == WM_QUIT) {
                PostQuitMessage(int(msg.wParam));
                break;
            }

            if(!IsDialogMessage(about->handle(), &msg)) {
                TranslateMessage(&msg); 
                DispatchMessage(&msg);
            }

        } else {
            if (hadMessage) {
                hadMessage = false;
                SendMessage(hwndParent, WM_ENTERIDLE, 0, 0);
            }
            WaitMessage();
        }
    }
    NotifyWinEvent(EVENT_SYSTEM_DIALOGEND, about->handle(), 0, 0);
    EnableWindow(hwndParent, true);
}

AboutWindow::AboutWindow(HWND hwnd, CREATESTRUCT & cs):
	Window(hwnd),
    hwndParent(cs.hwndParent) {

    auto scaling = Scaling::getCurrent(handle());
    m_minWidth = scaling.dpiScale(s_unscaledMinWidth);
    m_minHeight = scaling.dpiScale(s_unscaledMinHeight);
}

LRESULT AboutWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {

    switch(msg) {
    case WM_CLOSE:
        m_ended = true;
        return 0;
    case WM_CREATE:
        onCreate();
        return 0;
    case WM_SIZE:
        onResize(wParam);
        return 0;
    case WM_GETMINMAXINFO:
        onMinMaxInfo(*reinterpret_cast<MINMAXINFO *>(lParam));
        return 0;
    case WM_DPICHANGED:
        onDpiChanged(LOWORD(wParam), *reinterpret_cast<RECT *>(lParam));
        return 0;
    case WM_ERASEBKGND:
        eraseBackground(HDC(wParam));
        return TRUE;
    case WM_WININICHANGE:
        onSettingsChange();
        return 0;
    }
	return Window::handleMessage(msg, wParam, lParam);
}

void AboutWindow::onCreate() {

    SetFocus(handle());

    RECT parentRect;
    if (!GetWindowRect(hwndParent, &parentRect))
        throwLastError();

    int myWidth = m_minWidth;
    int myHeight = m_minHeight;
    int myLeft  = parentRect.left + (parentRect.right  - parentRect.left - myWidth) / 2;
    int myTop = parentRect.top  + (parentRect.bottom - parentRect.top  - myHeight) / 2;

    SetWindowPos(handle(), HWND_TOP, myLeft, myTop, myWidth, myHeight, SWP_NOZORDER | SWP_NOACTIVATE);

    path webFolder = getWebFolderPath(handle());

    comTest(CreateCoreWebView2EnvironmentWithOptions(nullptr, webFolder.native().c_str(), nullptr,
        makeWebViewEnvironmentCompletedHandler(
            [weakMe = get_weak_ptr()](HRESULT errorCode, ICoreWebView2Environment * environment) {

                auto me = strong_cast(weakMe);
                if (!me)
                    return;

                if (errorCode != S_OK || !environment)
                    return;

                comTest(environment->CreateCoreWebView2Controller(me->handle(), makeWebViewControllerCompletedHandler(
                    [weakMe](HRESULT errorCode, ICoreWebView2Controller * controller) {

                        auto me = strong_cast(weakMe);
                        if (!me)
                            return;

                        if (errorCode != S_OK || !controller)
                            return;

                        comTest(controller->get_CoreWebView2(std::out_ptr(me->m_webView)));
                        me->m_webViewController = com_retain(controller);

                        me->initWebView();

                    }).get()));

            }).get()));

    onSettingsChange();
}

void AboutWindow::initWebView() {

    com_shared_ptr<ICoreWebView2Settings> baseSettings;
    comTest(m_webView->get_Settings(std::out_ptr(baseSettings)));
    auto settings = com_cast<ICoreWebView2Settings6>(baseSettings);

    comTest(settings->put_IsScriptEnabled(true));
    comTest(settings->put_IsWebMessageEnabled(false));
    comTest(settings->put_IsStatusBarEnabled(false));
    comTest(settings->put_AreDefaultContextMenusEnabled(false));
    comTest(settings->put_AreDevToolsEnabled(false));
    comTest(settings->put_IsZoomControlEnabled(false));
    comTest(settings->put_AreBrowserAcceleratorKeysEnabled(false));
    comTest(settings->put_IsPinchZoomEnabled(false));
    comTest(settings->put_IsSwipeNavigationEnabled(false));

    EventRegistrationToken token;
    comTest(m_webView->add_NavigationStarting(makeWebViewNavigationStartingEventHandler(
        [weakMe = get_weak_ptr()](ICoreWebView2 * /*sender*/, ICoreWebView2NavigationStartingEventArgs * args) {

            auto me = strong_cast(weakMe);
            if (!me)
                return;
            
            BOOL userInitiated;
            comTest(args->get_IsUserInitiated(&userInitiated));

            if (userInitiated) {
                unqiue_co_membuf<wchar_t[]> uri;
                comTest(args->get_Uri(std::out_ptr(uri)));
                ShellExecute(me->handle(), L"open", uri.get(), nullptr, nullptr, SW_SHOWNORMAL);
                comTest(args->put_Cancel(true));
            }
        }
    ).get(), &token));

    RECT bounds;
    if (!GetClientRect(handle(), &bounds))
        throwLastError();
    m_webViewController->put_Bounds(bounds);

    m_webView->add_NavigationCompleted(makeWebViewNavigationCompletedEventHandler(
        [weakMe = get_weak_ptr()](ICoreWebView2 * /*sender*/, ICoreWebView2NavigationCompletedEventArgs * /*args*/) {

            auto me = strong_cast(weakMe);
            if (!me)
                return;

            me->m_webView->ExecuteScript(L"document.getElementById('content').scrollHeight", 
                makeWebViewExecuteScriptCompletedHandler(
                    [weakMe](HRESULT errorCode, LPCWSTR result) {
                        auto me = strong_cast(weakMe);
                        if (!me)
                            return;

                        if (errorCode != S_OK)
                            return;

                        wchar_t * end;
                        errno = 0;
                        auto height = wcstol(result, &end, 10);
                        if (end != result + wcslen(result) || errno)
                            return;
                        me->m_htmUnscaledHeight = int(height);
                        auto scaling = Scaling::getCurrent(me->handle());
                        me->ensureHtmlHeight(scaling);
                    }
                ).get());
        }
    ).get(), &token);

    loadContent();
}

void AboutWindow::onResize(WPARAM wParam) {
    if (wParam != SIZE_RESTORED && wParam != SIZE_MAXIMIZED)
        return;
    if (!m_webViewController)
        return;

    RECT bounds;
    if (!GetClientRect(handle(), &bounds))
        throwLastError();
    m_webViewController->put_Bounds(bounds);
}

void AboutWindow::ensureHtmlHeight(const Scaling & scaling) {

    int height = scaling.dpiScale(m_htmUnscaledHeight);
    RECT bounds;
    if (!GetClientRect(handle(), &bounds))
        throwLastError();
    int minimumClientHeight = height;
    RECT windowRect;
    if (!GetWindowRect(handle(), &windowRect))
        throwLastError();
    int ncDelta = (windowRect.bottom - windowRect.top) - (bounds.bottom - bounds.top);
    m_minHeight = minimumClientHeight + ncDelta;

    if (windowRect.bottom - windowRect.top < m_minHeight) {
        auto top = windowRect.top;
        auto & monitorRect = scaling.monitorRect();
        if (top + m_minHeight > monitorRect.bottom) {
            auto delta = (top + m_minHeight) - monitorRect.bottom;
            top -= std::min(top, delta);
        }
        SetWindowPos(handle(), HWND_TOP, windowRect.left, top, windowRect.right - windowRect.left, m_minHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void AboutWindow::onMinMaxInfo(MINMAXINFO & info) {
    info.ptMinTrackSize.x = m_minWidth;
    info.ptMinTrackSize.y = m_minHeight;
    info.ptMaxTrackSize.x = m_minWidth;
    info.ptMaxTrackSize.y = m_minHeight;
}

void AboutWindow::onDpiChanged(int dpi, RECT & suggestedRect) {

    auto scaling = Scaling::getCurrentForDpi(handle(), dpi);
    m_minWidth = scaling.dpiScale(s_unscaledMinWidth);
    m_minHeight = scaling.dpiScale(s_unscaledMinHeight);

    SetWindowPos(handle(), HWND_TOP, 
                 suggestedRect.left, suggestedRect.top, 
                 suggestedRect.right - suggestedRect.left, 
                 suggestedRect.bottom - suggestedRect.top, SWP_NOZORDER | SWP_NOACTIVATE);

    ensureHtmlHeight(scaling);
}

void AboutWindow::eraseBackground(HDC hdc) {
    RECT rect;
    if (!GetClientRect(handle(), &rect))
        throwLastError();
    if (!FillRect(hdc, &rect, g_windowBrush.get()))
        throwLastError();
}

void AboutWindow::onSettingsChange() {
    BOOL value = g_isDarkMode;
    DwmSetWindowAttribute(handle(), DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

    RECT rect;
    if (!GetClientRect(handle(), &rect))
        throwLastError();
    InvalidateRect(handle(), &rect, true);
    UpdateWindow(handle());
}


void AboutWindow::loadContent() {

    if (!m_webView)
        return;

    auto html = [&]() {
        auto resourceInfo = FindResource(Module::handle(), MAKEINTRESOURCE(IDR_HTML_ABOUT), RT_HTML);
        if (!resourceInfo)
            throwLastError();
        auto resource = LoadResource(Module::handle(), resourceInfo);
        if (!resource)
            throwLastError();
        auto data = reinterpret_cast<const char *>(LockResource(resource));
        auto size = SizeofResource(Module::handle(), resourceInfo);
        return sys_string(data, size);
    }();

    auto [version, copyright] = []() {

        auto filename = getModuleFileName(Module::handle());
        auto size = GetFileVersionInfoSize(filename.w_str(), nullptr);
        if (!size)
            throwLastError();
        std::vector<uint8_t> buf(size);
        if (!GetFileVersionInfo(filename.w_str(), 0, DWORD(size), buf.data()))
            throwLastError();

        wchar_t * valBuf;
        UINT valBufSize;
        if (!VerQueryValue(buf.data(), L"\\StringFileInfo\\040904b0\\ProductVersion", (void **)&valBuf, &valBufSize))
            throwLastError();
        while(valBufSize && !valBuf[valBufSize - 1])
            --valBufSize;
        sys_string version(valBuf, valBufSize);

        if (!VerQueryValue(buf.data(), L"\\StringFileInfo\\040904b0\\LegalCopyright", (void **)&valBuf, &valBufSize))
            throwLastError();
        while(valBufSize && !valBuf[valBufSize - 1])
            --valBufSize;
        sys_string copyright(valBuf, valBufSize);

        return std::make_pair(version, copyright);
    }();

    auto iconUrl = []() {
        auto icon = HICON(LoadImage(Module::handle(), MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 256, 256, LR_SHARED));
        if (!icon)
            throwLastError();
        auto png = imageFromIcon(icon, S("image/png"));
        
        sys_string_builder builder;
        builder.append(L"data:image/png;base64, ");
        base64Encode(png, builder);
        return builder.build();
    }();


    html = html.replace(S("%VERSION%"), version, size_t(-1));
    html = html.replace(S("%COPYRIGHT%"), copyright, size_t(-1));
    html = html.replace(S("%ICONURL%"), iconUrl, size_t(-1));

    m_webView->NavigateToString(html.w_str());
}



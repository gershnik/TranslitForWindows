// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Translit/Identifiers.h>

#include "MainWindow.h"
#include "Util.h"
#include "AddDialog.h"
#include "AboutWindow.h"
#include "main.h"

MainWindow::MainWindow(HWND hwnd, CREATESTRUCT & /*cs*/) : 
    Window(hwnd),
    m_model(refcnt_attach(new Model)),
    m_appKey(RegKey::currentUser->create(L"Software\\Translit", 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY)) {

    m_model->addListener(this);
}

MainWindow::~MainWindow() {
    m_model->removeListener(this); 
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        onCreate();
        return 0;
    case WM_SIZE:
        onResize(wParam);
        return 0;
    case WM_WINDOWPOSCHANGED:
        if (onPosChanged(reinterpret_cast<WINDOWPOS *>(lParam)))
            return 0;
        break;
    case WM_GETMINMAXINFO:
        onMinMaxInfo(*reinterpret_cast<MINMAXINFO *>(lParam));
        return 0;
    case WM_DPICHANGED:
        onDpiChanged(LOWORD(wParam), *reinterpret_cast<RECT *>(lParam));
        return 0;
    case WM_COMMAND:
        if (lParam == 0) {
            if (handleCommand(bool(HIWORD(wParam)), LOWORD(wParam)))
                return 0;
        }
        break;
    case WM_ERASEBKGND:
        eraseBackground(HDC(wParam));
        return TRUE;
    case WM_WININICHANGE:
        onSettingsChange();
        return 0;
    case WM_CLOSE:
        onClose();
        return 0;
    }
    return Window::handleMessage(msg, wParam, lParam);
}

bool MainWindow::handleCommand(bool /*isAccel*/, WORD commandId) {
    switch(commandId) {
    case s_executeAsyncCommand: executeAsyncCommand(); return true;
    }
    return false;
}

void MainWindow::onCreate() {

    onSettingsChange();

    auto scaling = Scaling::getCurrent(handle());
    m_minWidth = scaling.dpiScale(s_unscaledMinWidth);
    m_minHeight = scaling.dpiScale(s_unscaledMinHeight);

    auto & monRect = scaling.monitorRect();

    int myWidth = m_minWidth;
    int myHeight = m_minHeight;
    int myLeft  = monRect.left + (monRect.right - monRect.left - myWidth) / 2;
    int myTop = monRect.top  + (monRect.bottom - monRect.top - myHeight) / 2;

    RECT savedRect;
    std::error_code ec;
    auto size = m_appKey->getValue(L"Placement", REG_BINARY, {LPBYTE(&savedRect), sizeof(savedRect)}, ec);
    if (!ec && size == sizeof(savedRect)) {
        if (savedRect.left >= monRect.left && savedRect.right <= monRect.right &&
            savedRect.top >= monRect.top && savedRect.bottom <= monRect.bottom &&
            (savedRect.right - savedRect.left) >= m_minWidth &&
            (savedRect.bottom - savedRect.top) >= m_minHeight) {
        
            myWidth = savedRect.right - savedRect.left;
            myHeight = savedRect.bottom - savedRect.top;
            myLeft = savedRect.left;
            myTop = savedRect.top;
        }
    }

    SetWindowPos(handle(), HWND_TOP, myLeft, myTop, myWidth, myHeight, SWP_SHOWWINDOW);
    
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
}

void MainWindow::onClose() {

    RECT rect;
    if (!GetWindowRect(handle(), &rect))
        throwLastError();
    m_appKey->setValue(L"Placement", REG_BINARY, {LPBYTE(&rect), sizeof(rect)});
    PostQuitMessage(0);
}


void MainWindow::initWebView() {

    RECT bounds;
    if (!GetClientRect(handle(), &bounds))
        throwLastError();
    m_webViewController->put_Bounds(bounds);

    com_shared_ptr<ICoreWebView2Settings> baseSettings;
    comTest(m_webView->get_Settings(std::out_ptr(baseSettings)));
    auto settings = com_cast<ICoreWebView2Settings6>(baseSettings);

    comTest(settings->put_IsScriptEnabled(true));
    comTest(settings->put_IsWebMessageEnabled(true));
    comTest(settings->put_IsStatusBarEnabled(false));
    comTest(settings->put_AreDefaultContextMenusEnabled(false));
    comTest(settings->put_AreDevToolsEnabled(false));
    comTest(settings->put_IsZoomControlEnabled(false));
    comTest(settings->put_AreBrowserAcceleratorKeysEnabled(false));
    comTest(settings->put_IsPinchZoomEnabled(false));
    comTest(settings->put_IsSwipeNavigationEnabled(false));
    
    EventRegistrationToken token;
    comTest(m_webView->add_WebMessageReceived(makeWebViewWebMessageReceivedEventHandler(
        [weakMe = get_weak_ptr()](ICoreWebView2 * /*sender*/, ICoreWebView2WebMessageReceivedEventArgs * args) {
            if (auto me = strong_cast(weakMe))
                me->handleWebEvent(args);
        }
    
    ).get(), &token));

    comTest(m_webView->add_NavigationCompleted(makeWebViewNavigationCompletedEventHandler(
        [weakMe = get_weak_ptr()](ICoreWebView2 * /*sender*/, ICoreWebView2NavigationCompletedEventArgs * /*args*/) {
        
            if (auto me = strong_cast(weakMe))
                me->loadModelData(); 
        }
    ).get(), &token));

    auto resourceInfo = FindResource(Module::handle(), MAKEINTRESOURCE(IDR_HTML_MAIN), RT_HTML);
    if (!resourceInfo)
        throwLastError();
    auto resource = LoadResource(Module::handle(), resourceInfo);
    if (!resource)
        throwLastError();
    auto data = reinterpret_cast<const char *>(LockResource(resource));
    auto size = SizeofResource(Module::handle(), resourceInfo);
    auto html = sys_string(data, size);

    comTest(m_webView->NavigateToString(html.w_str()));
}

void MainWindow::handleWebEvent(ICoreWebView2WebMessageReceivedEventArgs * args) {

    unqiue_co_membuf<wchar_t[]> uri;
    comTest(args->get_Source(std::out_ptr(uri)));

    if (wcscmp(uri.get(), L"about:blank") != 0)
        return;

    unqiue_co_membuf<wchar_t[]> message;
    comTest(args->get_WebMessageAsJson(std::out_ptr(message)));

    rapidjson::WDocument data;
    rapidjson::ParseResult ok = data.Parse(message.get());
    if (!ok)
        abort();
    const rapidjson::WValue & type = data[L"type"];
    if (auto typeName = type.GetString(); wcscmp(typeName, L"onSelectLanguage") == 0) {
        const rapidjson::WValue & value = data[L"value"];
        uuid profileId = uuid::from_chars(std::span{value.GetString(), value.GetStringLength()}).value();
        scheduleAsyncCommand([profileId](MainWindow * me) {
            me->onSelectLanguage(profileId);
        });
    } else if (wcscmp(typeName, L"onSelectMapping") == 0) {
        const rapidjson::WValue & value = data[L"value"];
        int mappingIdx = value.GetInt();
        scheduleAsyncCommand([mappingIdx](MainWindow * me) {
            me->onSelectMapping(mappingIdx);
        });
    } else if (wcscmp(typeName, L"onAddLanguage") == 0) {
        scheduleAsyncCommand([](MainWindow * me) {
            me->onAddLanguage();
        });
    } else if (wcscmp(typeName, L"onRemoveLanguage") == 0) {
        scheduleAsyncCommand([](MainWindow * me) {
            me->onRemoveLanguage();
        });
    } else if (wcscmp(typeName, L"onShowAbout") == 0) {
        scheduleAsyncCommand([](MainWindow * me) {
            me->onShowAbout();
        });
    }
}

void MainWindow::onResize(WPARAM wParam) {
    if (wParam != SIZE_RESTORED && wParam != SIZE_MAXIMIZED)
        return;
    if (!m_webViewController)
        return;

    RECT bounds;
    if (!GetClientRect(handle(), &bounds))
        throwLastError();
    m_webViewController->put_Bounds(bounds);
}

bool MainWindow::onPosChanged(WINDOWPOS * /*pos*/) {
    if (m_webViewController)
        m_webViewController->NotifyParentWindowPositionChanged();
    return false;
}

void MainWindow::ensureHtmlHeight(const Scaling & scaling) {

    int htmlRequestedHeight = scaling.dpiScale(m_htmlRequestedHeight);
    RECT bounds;
    if (!GetClientRect(handle(), &bounds))
        throwLastError();
    int minimumClientHeight = htmlRequestedHeight;
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

void MainWindow::onMinMaxInfo(MINMAXINFO & info) {
    info.ptMinTrackSize.x = m_minWidth;
    info.ptMinTrackSize.y = m_minHeight;
}

void MainWindow::onDpiChanged(int dpi, RECT & suggestedRect) {

    auto scaling = Scaling::getCurrentForDpi(handle(), dpi);
    m_minWidth = scaling.dpiScale(s_unscaledMinWidth);
    m_minHeight = scaling.dpiScale(s_unscaledMinHeight);

    if (suggestedRect.right - suggestedRect.left < m_minWidth)
        suggestedRect.right = suggestedRect.left + m_minWidth;

    if (suggestedRect.bottom - suggestedRect.top < m_minHeight)
        suggestedRect.bottom = suggestedRect.top + m_minHeight;

    SetWindowPos(handle(), HWND_TOP, 
                 suggestedRect.left, suggestedRect.top, 
                 suggestedRect.right - suggestedRect.left, 
                 suggestedRect.bottom - suggestedRect.top, SWP_NOZORDER | SWP_NOACTIVATE);
    
    ensureHtmlHeight(scaling);
}

void MainWindow::eraseBackground(HDC hdc) {
    RECT rect;
    if (!GetClientRect(handle(), &rect))
        throwLastError();
    if (!FillRect(hdc, &rect, g_windowBrush.get()))
        throwLastError();
}

void MainWindow::onSettingsChange() {
    BOOL value = g_isDarkMode;
    DwmSetWindowAttribute(handle(), DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

    RECT rect;
    if (!GetClientRect(handle(), &rect))
        throwLastError();
    InvalidateRect(handle(), &rect, true);
    UpdateWindow(handle());
}

// Logic event handlers

void MainWindow::loadModelData() {
    if (!m_webView)
        return;

    rapidjson::WStringBuffer buffer;
    rapidjson::WWriter<rapidjson::WStringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key(L"type");
    writer.String(L"model");

    writer.Key(L"languages");
    writer.StartArray();
    for(auto [index, profileInfo]: std::views::enumerate(m_model->enabledProfiles())) {
        writer.StartObject();
        writer.Key(L"name");
        writer.String(profileInfo.displayName);
        writer.Key(L"id");
        writer.String(sys_string(uuid(*profileInfo.id).to_chars()).w_str());    
        writer.EndObject();
    }
    writer.EndArray();

    auto * activeProfile = m_model->getActiveProfile();
    writer.Key(L"mappings");
    writer.StartArray();
    if (activeProfile) {
        for(auto & mapping: activeProfile->mappings) {
            writer.String(mapping->displayName);
        }
    }
    writer.EndArray();

    if (activeProfile) {
        writer.Key(L"selectedLanguage");
        writer.String(sys_string(uuid(*activeProfile->id).to_chars()).w_str());
    }

    writer.Key(L"selectedMapping");
    writer.Int(m_model->getActiveProfileMappingIndex());

    writer.Key(L"mappingTable");
    writer.String(m_model->getActiveMappingTableName().w_str());

    writer.Key(L"canAdd");
    writer.Bool(!m_model->disabledProfiles().empty());

    writer.Key(L"canRemove");
    writer.Bool(!m_model->enabledProfiles().empty());

    writer.EndObject();
    
    comTest(m_webView->PostWebMessageAsJson(buffer.GetString()));

    comTest(m_webView->ExecuteScript(L"document.getElementById('content').scrollHeight", 
        makeWebViewExecuteScriptCompletedHandler(
            [weakMe = get_weak_ptr()](HRESULT errorCode, LPCWSTR result) {
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
                me->m_htmlRequestedHeight = int(height);
                auto scaling = Scaling::getCurrent(me->handle());
                me->ensureHtmlHeight(scaling);                        
            }
        ).get()));
    
}

void MainWindow::onModelReload() {
    loadModelData();
}

void MainWindow::onActiveProfileChange() {
    loadModelData();
}

void MainWindow::onSelectLanguage(const uuid & profileId) {

    for(auto & profile: m_model->enabledProfiles()) {
        if (uuid(*profile.id) == profileId) {
            m_model->setActiveProfile(&profile);
        }
    }
}

void MainWindow::onSelectMapping(int mappingIdx) {

    m_model->setActiveProfileMappingIndex(mappingIdx);
    loadModelData();
}

void MainWindow::onAddLanguage() {

    if (AddDialog::show(handle(), m_model)) {

        rapidjson::WStringBuffer buffer;
        rapidjson::WWriter<rapidjson::WStringBuffer> writer(buffer);
        writer.StartObject();

        writer.Key(L"type");
        writer.String(L"showlangs");
        writer.EndObject();

        comTest(m_webView->PostWebMessageAsJson(buffer.GetString()));
    }
}

void MainWindow::onRemoveLanguage() {

    if (auto profile = m_model->getActiveProfile())
        m_model->enableProfiles(std::array{profile}, false);
}

void MainWindow::onShowAbout() {

    AboutWindow::showModal(handle());
}




// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/ComObject.h>

#include "main.h"

inline path getWebFolderPath(HWND hwndParent) {
    path webFolder = g_dataFolder / L"WebView2";
    std::error_code ec;
    create_directory(webFolder, ec);
    if (ec) {
        MessageBox(hwndParent, 
                   sys_string::std_format("Unable to create {}\nError 0x{:08x}", webFolder.string(), ec.value()).w_str(),
                   L"Translit: Fatal Error", MB_ICONHAND);
        exit(EXIT_FAILURE);
    }
    return webFolder;
}

inline void base64Encode(std::span<const uint8_t> data, sys_string_builder & builder) {
    DWORD b64Count = 0;
    if (!CryptBinaryToString(data.data(), DWORD(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, 
                             nullptr, &b64Count))
        abort();
    auto offset = builder.storage_size();
    builder.resize_storage(offset + b64Count); //here it includes null terminator
    if (!CryptBinaryToString(data.data(), DWORD(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, 
                             LPWSTR(builder.chars().begin() + offset), &b64Count))
        abort();
    builder.resize_storage(offset + b64Count); //and here it doesn't, sigh!
}


template<class T, class Iface, class... Args>
class WebViewEventHandler final : 
    public ComObject<WebViewEventHandler<T, Iface, Args...>, Iface> {

    static_assert(std::is_invocable_v<T, Args...>);
public:
    WebViewEventHandler(T && func): m_func(std::move(func)) {}
    WebViewEventHandler(const T & func): m_func(func) {}

    STDMETHODIMP Invoke(Args... args) override {
        //COM_PROLOG
        m_func(args...);
        return S_OK;
        //COM_EPILOG
    }
private:
    T m_func;
};

#define MAKE_WEBVIEW_HANDLER(name, iface, ...) \
    template<class T> \
    using name = WebViewEventHandler<T, iface, __VA_ARGS__>; \
    template<class T> \
    inline auto make##name(T && func) { \
        return com_attach(new name<std::remove_cvref_t<T>>(std::forward<T>(func))); \
    }

MAKE_WEBVIEW_HANDLER(WebViewControllerCompletedHandler,
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller *);

MAKE_WEBVIEW_HANDLER(WebViewEnvironmentCompletedHandler, 
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment *);

MAKE_WEBVIEW_HANDLER(WebViewNavigationStartingEventHandler, 
    ICoreWebView2NavigationStartingEventHandler, ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *);

MAKE_WEBVIEW_HANDLER(WebViewNavigationCompletedEventHandler, 
    ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *);

MAKE_WEBVIEW_HANDLER(WebViewExecuteScriptCompletedHandler,
    ICoreWebView2ExecuteScriptCompletedHandler, HRESULT, LPCWSTR);

MAKE_WEBVIEW_HANDLER(WebViewFocusChangedEventHandler,
    ICoreWebView2FocusChangedEventHandler, ICoreWebView2Controller *, IUnknown *);

MAKE_WEBVIEW_HANDLER(WebViewWebMessageReceivedEventHandler,
    ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *)

#undef MAKE_WEBVIEW_HANDLER

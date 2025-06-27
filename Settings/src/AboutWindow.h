// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Scaling.h"

#include <Common/Window.h>

class AboutWindow final : public Window<AboutWindow> {
	friend ref_counted;
	friend Window;

private:
	static void fillClassInfo(WNDCLASSEX & cls) {
		cls.lpszClassName = L"TranslitAbout";
	}

public:
	static void showModal(HWND hwndParent);
private:
	AboutWindow(HWND hwnd, CREATESTRUCT & cs);
	~AboutWindow() = default;

	LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

	void onCreate();
	void initWebView();
	void ensureHtmlHeight(const Scaling & scaling);
	void onResize(WPARAM wParam);
	void onMinMaxInfo(MINMAXINFO & info);
	void onDpiChanged(int dpi, RECT & suggestedRect);
	void eraseBackground(HDC hdc);
	void onSettingsChange();

	void loadContent();

private:
	static constexpr int s_unscaledMinWidth = 465;
	static constexpr int s_unscaledMinHeight = 180;

	HWND hwndParent;
	bool m_ended = false;
	int m_htmUnscaledHeight = 0;
	int m_minWidth;
	int m_minHeight;

	com_shared_ptr<ICoreWebView2Controller> m_webViewController;
	com_shared_ptr<ICoreWebView2> m_webView;
};
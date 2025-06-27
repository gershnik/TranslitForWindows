// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/Window.h>

#include "Model.h"
#include "Scaling.h"
#include "../res/resource.h"

class MainWindow final : 
	public Window<MainWindow>,
	private Model::Listener {

	friend ref_counted;
	friend Window;

public:
	static inline const wchar_t * className = L"TranslitSettings";

private:
	static void fillClassInfo(WNDCLASSEX & cls) {
		cls.hIcon = LoadIcon(Module::handle(), MAKEINTRESOURCE(IDI_APP_ICON));
		cls.lpszClassName = className;
		cls.hIconSm = cls.hIcon;
	}
	
public:
	static refcnt_ptr<MainWindow> create(HWND hwndParent) {
		return Window::create(WS_EX_OVERLAPPEDWINDOW | WS_EX_TOPMOST, L"Translit",
							  WS_OVERLAPPED | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, 
							  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
							  hwndParent, nullptr, nullptr);
	}
	

private:
	MainWindow(HWND hwnd, CREATESTRUCT & cs);
	~MainWindow();

	LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
	bool handleCommand(bool isAccel, WORD commandId);
	void onCreate();
	void onClose();
	void initWebView();
	void handleWebEvent(ICoreWebView2WebMessageReceivedEventArgs * args);
	
	void onResize(WPARAM wParam);
	bool onPosChanged(WINDOWPOS * pos);
	void ensureHtmlHeight(const Scaling & scaling);
	void onMinMaxInfo(MINMAXINFO & info);
	void onDpiChanged(int dpi, RECT & suggestedRect);
	void eraseBackground(HDC hdc);
	void onSettingsChange();

	void loadModelData();
	void onModelReload() override;
	void onActiveProfileChange() override;
	void onSelectLanguage(const uuid & profileId);
	void onSelectMapping(int mappingIdx);
	void onAddLanguage();
	void onRemoveLanguage();
	void onShowAbout();

	void executeAsyncCommand() {
		while(!m_asyncCommands.empty()) {
			auto & command = m_asyncCommands.back();
			command(this);
			m_asyncCommands.pop_back();
		}
	}

	template<class Func>
	void scheduleAsyncCommand(Func && func) {
		m_asyncCommands.emplace_back(std::forward<Func>(func));
		PostMessage(handle(), WM_COMMAND, s_executeAsyncCommand, 0);
	}

private:
    static constexpr int s_unscaledMinWidth = 425;
	static constexpr int s_unscaledMinHeight = 307;
	static constexpr WORD s_executeAsyncCommand = 100;
	
	refcnt_ptr<Model> m_model;
	std::unique_ptr<RegKey> m_appKey;
	
	int m_htmlRequestedHeight = 0;
	int m_minWidth = s_unscaledMinWidth;
	int m_minHeight = s_unscaledMinHeight;
	
	com_shared_ptr<ICoreWebView2Controller> m_webViewController;
	com_shared_ptr<ICoreWebView2> m_webView;

	std::vector<std::function<void (MainWindow *)>> m_asyncCommands;
};
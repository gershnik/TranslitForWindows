// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Model.h"

#include <Common/win32api.h>
#include <Common/err.h>

#include "../res/resource.h"

class AddDialog {

public:
	static INT_PTR show(HWND parent, const refcnt_ptr<Model> & model) {
		return DialogBoxParam(Module::handle(), MAKEINTRESOURCE(IDD_ADD_LANGUAGE), parent, dialogProc, LPARAM(&model));
	}

private:
	AddDialog(HWND hwnd, LPARAM lParam): 
		m_handle(hwnd),
		m_model(*reinterpret_cast<const refcnt_ptr<Model> *>(lParam))
	{
		auto parent = GetParent(m_handle);
		RECT parentRect, myRect;
		if (!GetWindowRect(parent, &parentRect))
			throwLastError();
		if (!GetWindowRect(m_handle, &myRect))
			throwLastError();

		int myWidth = myRect.right - myRect.left;
		int myHeight = myRect.bottom - myRect.top;
		int myLeft  = parentRect.left + (parentRect.right - parentRect.left - myWidth) / 2;
		int myTop = parentRect.top  + (parentRect.bottom - parentRect.top - myHeight) / 2;

		SetWindowPos(m_handle, HWND_TOP, myLeft, myTop, myWidth, myHeight, SWP_NOZORDER | SWP_NOACTIVATE);

		m_languageList = GetDlgItem(m_handle, IDC_LANGUAGE_LIST);
		if (!m_languageList)
			abort();
		onSettingsChange();

		EnableWindow(GetDlgItem(m_handle, IDOK), false);

		RECT languageListRect;
		if (!GetClientRect(m_languageList, &languageListRect))
			throwLastError();
		SendMessage(m_languageList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);
		LVCOLUMN lvc = {};
		lvc.mask = LVCF_WIDTH;
		lvc.cx = languageListRect.right - languageListRect.left;
		SendMessage(m_languageList, LVM_INSERTCOLUMN, 0, LPARAM(&lvc));

		LVITEM lvi = {};
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		for (auto [index, profileInfo]: std::views::enumerate(m_model->disabledProfiles())) {
			lvi.iItem = int(index);
			lvi.pszText = const_cast<wchar_t *>(profileInfo.displayName);
			lvi.lParam = reinterpret_cast<LPARAM>(&profileInfo);
			SendMessage(m_languageList, LVM_INSERTITEM, 0, LPARAM(&lvi));
		}
	}

	static INT_PTR CALLBACK dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
		if (message ==  WM_INITDIALOG) {
			std::unique_ptr<AddDialog> me(new AddDialog(hwnd, lParam));
			SetLastError(0);
			auto res = SetWindowLongPtr(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(me.get()));
			if (auto err = GetLastError(); res == 0 && err != 0)
				throwWinError(err);
			auto ret = me->handleMessage(WM_INITDIALOG, wParam, lParam);
			me.release();
			return ret;
		}
		auto me = reinterpret_cast<AddDialog *>(GetWindowLongPtr(hwnd, DWLP_USER));
		if (!me)
			return FALSE;

		return me->handleMessage(message, wParam, lParam);
	}

	void endMyself(INT_PTR ret) {
		SetLastError(0);
		auto res = SetWindowLongPtr(m_handle, DWLP_USER, reinterpret_cast<LONG_PTR>(nullptr));
		if (auto err = GetLastError(); res == 0 && err != 0)
			throwWinError(err);
		auto hwnd = m_handle;
		delete this;
		if (!EndDialog(hwnd, ret))
			throwLastError();
	}

	INT_PTR handleMessage(UINT message, [[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam) {
		switch(message) {
		case WM_INITDIALOG:
			return true;
		case WM_COMMAND:
			switch(wParam) {
			case IDOK:
				onOk();
				return true;
			case IDCANCEL:
				onCancel();
				return true;
			}
			break;
		case WM_CLOSE:
			onCancel();
			return true;
		case WM_WININICHANGE:
			onSettingsChange();
			return true;
		case WM_CTLCOLORDLG:
			return onCtlColorDialog(HDC(wParam));
		case WM_NOTIFY: {
			auto * header = reinterpret_cast<NMHDR *>(lParam);
			switch (header->idFrom) {
			case IDC_LANGUAGE_LIST:
				switch (header->code) {
				case NM_CUSTOMDRAW: 
					return onCustomDrawLanguageList(reinterpret_cast<NMCUSTOMDRAW *>(lParam));
				case LVN_ITEMCHANGED:
				case LVN_ODSTATECHANGED:
					return onLanguageListChanged();
				}
				break;
			}
			break;
		}
		}
		return false;
	}

	void onSettingsChange() {
		BOOL value = g_isDarkMode;
		DwmSetWindowAttribute(m_handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

		auto theme = g_isDarkMode ? L"DarkMode_Explorer" : L"Explorer";

		SetWindowTheme(GetDlgItem(m_handle, IDOK), theme, nullptr);
		SetWindowTheme(GetDlgItem(m_handle, IDCANCEL), theme, nullptr);
		SetWindowTheme(m_languageList, theme, nullptr);
		SendMessage(m_languageList, LVM_SETTEXTCOLOR, 0, g_textColor);
		SendMessage(m_languageList, LVM_SETTEXTBKCOLOR, 0, g_windowColor);
		SendMessage(m_languageList, LVM_SETBKCOLOR, 0, g_windowColor);

		RECT rect;
		if (!GetClientRect(m_handle, &rect))
			throwLastError();
		InvalidateRect(m_handle, &rect, true);
		UpdateWindow(m_handle);
	}

	INT_PTR onCtlColorDialog(HDC dc) {
		SetBkMode(dc, OPAQUE);
		SetTextColor(dc, g_textColor);
		SetBkColor(dc, g_windowColor);
		return (INT_PTR)g_windowBrush.get();
	}

	bool onCustomDrawLanguageList(NMCUSTOMDRAW * nmcd) {
		switch (nmcd->dwDrawStage)
		{
		case CDDS_PREPAINT:
			SetWindowLongPtr(m_handle, DWL_MSGRESULT, CDRF_NOTIFYITEMDRAW);
			return true;
		case CDDS_ITEMPREPAINT:
			SetTextColor(nmcd->hdc, g_textColor);
			SetWindowLongPtr(m_handle, DWL_MSGRESULT, CDRF_DODEFAULT);
			return true;
		}
		return false;
	}

	bool onLanguageListChanged() {
		int count = int(SendMessage(m_languageList, LVM_GETITEMCOUNT, 0, 0));
		LVITEM lvi = {};
		lvi.mask = LVIF_STATE;
		lvi.stateMask = LVIS_STATEIMAGEMASK;
		for(lvi.iItem = 0; lvi.iItem < count; ++lvi.iItem) {
			if (SendMessage(m_languageList, LVM_GETITEM, 0, LPARAM(&lvi))) {
				bool checked = (lvi.state >> 12) - 1;
				if (checked) {
					EnableWindow(GetDlgItem(m_handle, IDOK), true);
					return true;
				}
			}
		}
		EnableWindow(GetDlgItem(m_handle, IDOK), false);
		return true;
	}

	void onOk() {
		int count = int(SendMessage(m_languageList, LVM_GETITEMCOUNT, 0, 0));
		std::vector<const ProfileInfo *> toEnable;
		LVITEM lvi = {};
		lvi.mask = LVIF_PARAM | LVIF_STATE;
		lvi.stateMask = LVIS_STATEIMAGEMASK;
		for(lvi.iItem = 0; lvi.iItem < count; ++lvi.iItem) {
			if (SendMessage(m_languageList, LVM_GETITEM, 0, LPARAM(&lvi))) {
				bool checked = (lvi.state >> 12) - 1;
				auto profile = reinterpret_cast<const ProfileInfo *>(lvi.lParam);
				if (checked)
					toEnable.push_back(profile);
			}
		}
		if (!toEnable.empty())
			m_model->enableProfiles(toEnable, true);
			
		endMyself(true);
	}

	void onCancel() {
		endMyself(false);
	}

private:
	HWND m_handle;
	refcnt_ptr<Model> m_model;

	HWND m_languageList;
};
// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/ComObject.h>
#include <Translit/Identifiers.h>

#include "../res/resource.h"

template<>
struct InterfacesOf<ITfLangBarItemButton> {
	using type = TypeList<IUnknown, ITfLangBarItem, ITfLangBarItemButton>;
};

class SettingsButton final : public ComObject<SettingsButton, ITfLangBarItemButton, ITfSource> {

public:
    void query(IUnknown ** ret) noexcept
        { *ret = static_cast<ITfLangBarItemButton *>(this); }

    //ITfLangBarItem
    STDMETHODIMP GetInfo(__RPC__out TF_LANGBARITEMINFO *pInfo) override {

        pInfo->clsidService = g_translitServiceId;                                  // This LangBarItem belongs to this TextService.
        pInfo->guidItem = GUID_LBI_INPUTMODE;								        // GUID of this LangBarItem.
        pInfo->dwStyle = (TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY);      // This LangBar is a button type.
        pInfo->ulSort = 0;                                                          // The position of this LangBar Item is not specified.
        pInfo->szDescription[0] = 0;                                                // Set the description of this LangBar Item.
        return S_OK;
    }
    STDMETHODIMP GetStatus(__RPC__out DWORD *pdwStatus) override {
        *pdwStatus = m_status;
        return S_OK;
    }
    STDMETHODIMP Show(BOOL /*fShow*/) override {

        for(auto & item: m_sinks) {
            item.second->OnUpdate(TF_LBI_STATUS);
        }
        return S_OK;
    }
    STDMETHODIMP GetTooltipString(__RPC__deref_out_opt BSTR *pbstrToolTip) override {
        COM_PROLOG
            *pbstrToolTip = sys_string_bstr(L"Open Translit Settings").release();
            return S_OK;
        COM_EPILOG
    }

    //ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick /*click*/, POINT /*pt*/, __RPC__in const RECT */*prcArea*/) override {
        COM_PROLOG
            auto filename = getModuleFileName(Module::handle());
            sys_string dir;
            if (auto parts = filename.partition_at_last(U'\\'))
                dir = parts->first;
            sys_string settings = dir + S("\\TranslitSettings.exe");
            STARTUPINFO si = {};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = {};
            if (!CreateProcess(settings.w_str(), nullptr, nullptr, nullptr, false, 
                               CREATE_BREAKAWAY_FROM_JOB | CREATE_DEFAULT_ERROR_MODE,
                               nullptr, dir.w_str(), &si, &pi))
                throwLastError();
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return S_OK;
        COM_EPILOG
    }
    STDMETHODIMP InitMenu(__RPC__in_opt ITfMenu * /*pMenu*/) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP OnMenuSelect(UINT /*wID*/) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP GetIcon(__RPC__deref_out_opt HICON *phIcon) override {
        *phIcon = reinterpret_cast<HICON>(LoadImage(Module::handle(), MAKEINTRESOURCE(IDI_ICON_SETTINGS), IMAGE_ICON, 64, 64, 0));
        return S_OK;
    }
    STDMETHODIMP GetText(__RPC__deref_out_opt BSTR *pbstrText) override {
        *pbstrText = sys_string_bstr(L"Translit Settings").release();
        return S_OK;
    }

    //ITfSource
    STDMETHODIMP AdviseSink(__RPC__in REFIID riid, __RPC__in_opt IUnknown *punk, __RPC__out DWORD * pdwCookie) override {
        COM_PROLOG
            if (riid != __uuidof(ITfLangBarItemSink))
                return CONNECT_E_CANNOTCONNECT;
            while (m_sinks.find(m_nextCookie) != m_sinks.end()) {
                ++m_nextCookie;
            }
            m_sinks[m_nextCookie] = com_cast<ITfLangBarItemSink>(punk);
            *pdwCookie = m_nextCookie++;
            return S_OK;
        COM_EPILOG
    }
    STDMETHODIMP UnadviseSink(DWORD dwCookie) override {
        COM_PROLOG
            if (m_sinks.erase(dwCookie))
                return S_OK;
            return E_INVALIDARG;
        COM_EPILOG
    }

private:
    DWORD m_status = 0;
    std::map<DWORD, com_shared_ptr<ITfLangBarItemSink>> m_sinks;
    DWORD m_nextCookie = 0;

};
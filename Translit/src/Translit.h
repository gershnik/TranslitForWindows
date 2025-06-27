// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/ComObject.h>
#include "ActivatedProcessor.h"


template<>
struct InterfacesOf<ITfTextInputProcessorEx> {
    using type = TypeList<IUnknown, ITfTextInputProcessor, ITfTextInputProcessorEx>;
};

class Translit final : public ComObject<Translit,
        ITfTextInputProcessorEx,
        ITfThreadMgrEventSink,
        ITfTextEditSink,
        ITfKeyEventSink,
        ITfCompositionSink,
        ITfDisplayAttributeProvider,
        ITfActiveLanguageProfileNotifySink,
        ITfThreadFocusSink,
        //ITfFunctionProvider,
        ITfCompartmentEventSink
    >
{
public:
    Translit() = default;

    void query(IUnknown ** ret) noexcept
        { *ret = static_cast<ITfTextInputProcessor *>(this); }

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr * pThreadMgr, TfClientId tfClientId) override {
        return ActivateEx(pThreadMgr, tfClientId, 0);
    }

    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr * pThreadMgr, TfClientId tfClientId, DWORD dwFlags) override;
    STDMETHODIMP Deactivate() override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(_In_ ITfDocumentMgr * /*pDocMgr*/) override
        { return E_NOTIMPL; }
    STDMETHODIMP OnUninitDocumentMgr(_In_ ITfDocumentMgr * /*pDocMgr*/) override
        { return E_NOTIMPL; }
    STDMETHODIMP OnSetFocus(_In_ ITfDocumentMgr * pDocMgrFocus, _In_ ITfDocumentMgr  *pDocMgrPrevFocus) override;
    STDMETHODIMP OnPushContext(_In_ ITfContext * /*pContext*/) override
        { return E_NOTIMPL; }
    STDMETHODIMP OnPopContext(_In_ ITfContext * /*pContext*/) override
        { return E_NOTIMPL; }

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(__RPC__in_opt ITfContext *pContext, TfEditCookie ecReadOnly, __RPC__in_opt ITfEditRecord *pEditRecord) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL /*fForeground*/) override
        { return S_OK; }
    STDMETHODIMP OnTestKeyDown(ITfContext * pContext, WPARAM wParam, LPARAM lParam, BOOL * pIsEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext * pContext, WPARAM wParam, LPARAM lParam, BOOL * pIsEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext * pContext, WPARAM wParam, LPARAM lParam, BOOL * pIsEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext * pContext, WPARAM wParam, LPARAM lParam, BOOL * pIsEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext * pContext, REFGUID rguid, BOOL * pIsEaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, _In_ ITfComposition *pComposition) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(__RPC__deref_out_opt IEnumTfDisplayAttributeInfo **ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(__RPC__in REFGUID guidInfo, __RPC__deref_out_opt ITfDisplayAttributeInfo **ppInfo) override;

    // ITfActiveLanguageProfileNotifySink
    STDMETHODIMP OnActivated(_In_ REFCLSID clsid, _In_ REFGUID guidProfile, _In_ BOOL isActivated) override;

    // ITfThreadFocusSink
    STDMETHODIMP OnSetThreadFocus() override
        { return S_OK; }
    STDMETHODIMP OnKillThreadFocus() override
        { return S_OK; }

    // ITfCompartmentEventSink
    STDMETHODIMP OnChange(__RPC__in REFGUID rguid) override;

private:
    std::unique_ptr<ActivatedProcessor> m_activated;
};

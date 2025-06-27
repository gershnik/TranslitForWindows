// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Translit.h"
#include "DisplayAttributes.h"

#include <Translit/Identifiers.h>

#include <Common/com.h>



// ITfTextInputProcessorEx

STDMETHODIMP Translit::ActivateEx(ITfThreadMgr * pThreadMgr, TfClientId tfClientId, DWORD dwFlags) {
    COM_PROLOG
        m_activated.reset(new ActivatedProcessor(this, com_retain(pThreadMgr), tfClientId, dwFlags));
        return S_OK;
    COM_EPILOG
}

STDMETHODIMP Translit::Deactivate() {
    COM_PROLOG
        m_activated.reset();
        return S_OK;
    COM_EPILOG
}


// ITfThreadMgrEventSink

// Sink called by the framework when focus changes from one document to
// another.  Either document may be NULL, meaning previously there was no
// focus document, or now no document holds the input focus.
STDMETHODIMP Translit::OnSetFocus(_In_ ITfDocumentMgr * pDocMgrFocus, _In_ ITfDocumentMgr * /*pDocMgrPrevFocus*/) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;
        m_activated->setDocumentMgr(com_retain(pDocMgrFocus));
        return S_OK;
    COM_EPILOG
}

// ITfTextEditSink

// Called by the system whenever anyone releases a write-access document lock.
STDMETHODIMP Translit::OnEndEdit(__RPC__in_opt ITfContext * pContext, TfEditCookie ecReadOnly, __RPC__in_opt ITfEditRecord * pEditRecord) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;
        m_activated->onEndEdit(pContext, ecReadOnly, pEditRecord);
        return S_OK;
    COM_EPILOG
}

// ITfKeyEventSink

// Called by the system to query this service wants a potential keystroke.
STDMETHODIMP Translit::OnTestKeyDown(ITfContext * pContext, WPARAM wParam, LPARAM lParam, BOOL * pIsEaten) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;
    
        *pIsEaten = m_activated->onKeyDown(true, pContext, wParam, lParam);
        return S_OK;
    COM_EPILOG
}

// Called by the system to offer this service a keystroke.  If *pIsEaten == TRUE
// on exit, the application will not handle the keystroke.
STDMETHODIMP Translit::OnKeyDown(ITfContext * pContext, WPARAM wParam, LPARAM lParam, BOOL * pIsEaten) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;

        *pIsEaten = m_activated->onKeyDown(false, pContext, wParam, lParam);
        return S_OK;
    COM_EPILOG
}

// Called by the system to query this service wants a potential keystroke.
STDMETHODIMP Translit::OnTestKeyUp(ITfContext * /*pContext*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL * /*pIsEaten*/) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;

        return S_OK;
    COM_EPILOG
}

// Called by the system to offer this service a keystroke.  If *pIsEaten == TRUE
// on exit, the application will not handle the keystroke.
STDMETHODIMP Translit::OnKeyUp(ITfContext * /*pContext*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL * /*pIsEaten*/) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;

        return S_OK;
    COM_EPILOG
}

// Called when a hotkey (registered by us, or by the system) is typed.
STDMETHODIMP Translit::OnPreservedKey(ITfContext * /*pContext*/, REFGUID /*rguid*/, BOOL * /*pIsEaten*/) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;

        return S_OK;
    COM_EPILOG
}

// ITfCompositionSink

// Callback for ITfCompositionSink.  The system calls this method whenever
// someone other than this service ends a composition.
STDMETHODIMP Translit::OnCompositionTerminated(TfEditCookie ecWrite, _In_ ITfComposition * pComposition) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;

        m_activated->onCompositionTerminated(ecWrite, pComposition);

        return S_OK;
    COM_EPILOG
}

// ITfDisplayAttributeProvider

STDMETHODIMP Translit::EnumDisplayAttributeInfo(__RPC__deref_out_opt IEnumTfDisplayAttributeInfo ** ppEnum) {
    COM_PROLOG
        *ppEnum = new EnumDisplayAttributes;
        return S_OK;
    COM_EPILOG
}

STDMETHODIMP Translit::GetDisplayAttributeInfo(__RPC__in REFGUID guidInfo, __RPC__deref_out_opt ITfDisplayAttributeInfo ** ppInfo) {
    COM_PROLOG
        if (guidInfo == __uuidof(DisplayAttributeCompositionInfo)) {
            *ppInfo = new DisplayAttributeCompositionInfo;
            return S_OK;
        }
        return E_INVALIDARG; 
    COM_EPILOG
}

// ITfActiveLanguageProfileNotifySink

// Sink called by the framework when changes activate language profile.
STDMETHODIMP Translit::OnActivated(_In_ REFCLSID clsid, _In_ REFGUID guidProfile, _In_ BOOL isActivated)  {
    COM_PROLOG
        if (clsid != g_translitServiceId)
            return S_OK;

        if (!m_activated)
            return E_FAIL;

        if (isActivated)
            m_activated->setProfile(guidProfile);
        return S_OK;
    COM_EPILOG
}


// ITfCompartmentEventSink

STDMETHODIMP Translit::OnChange(__RPC__in REFGUID rguid) {
    COM_PROLOG
        if (!m_activated)
            return E_FAIL;

        m_activated->onCompartmentChange(rguid);
        return S_OK;
    COM_EPILOG
}


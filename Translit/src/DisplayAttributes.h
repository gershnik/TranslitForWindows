// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/ComObject.h>

class __declspec(uuid("1f6419e4-a816-45dd-a9a5-17ce33f520b0")) DisplayAttributeCompositionInfo final :
    public ComObject<DisplayAttributeCompositionInfo, ITfDisplayAttributeInfo> {

public:
    // ITfDisplayAttributeInfo
    STDMETHODIMP GetGUID(_Out_ GUID *pguid) override;
    STDMETHODIMP GetDescription(_Out_ BSTR *pbstrDesc) override;
    STDMETHODIMP GetAttributeInfo(_Out_ TF_DISPLAYATTRIBUTE *pTSFDisplayAttr) override;
    STDMETHODIMP SetAttributeInfo(_In_ const TF_DISPLAYATTRIBUTE *ptfDisplayAttr) override;
    STDMETHODIMP Reset() override;
};

class EnumDisplayAttributes final : 
    public ComObject<EnumDisplayAttributes, IEnumTfDisplayAttributeInfo> {

private:
    typedef com_shared_ptr<ITfDisplayAttributeInfo> Factory();
public:
    EnumDisplayAttributes(ULONG idx = 0):
        m_idx(idx)
    {}

    // IEnumTfDisplayAttributeInfo
    STDMETHODIMP Clone(_Out_ IEnumTfDisplayAttributeInfo **ppEnum) override;
    STDMETHODIMP Next(ULONG ulCount, __RPC__out_ecount_part(ulCount, *pcFetched) ITfDisplayAttributeInfo **rgInfo, __RPC__out ULONG *pcFetched) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Skip(ULONG ulCount) override;

private:
    ULONG m_idx = 0;

    static Factory * s_factories[];
};

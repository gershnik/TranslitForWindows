// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DisplayAttributes.h"

EnumDisplayAttributes::Factory * EnumDisplayAttributes::s_factories[] = {
	[]() { return com_attach(static_cast<ITfDisplayAttributeInfo *>(new DisplayAttributeCompositionInfo)); }
};

STDMETHODIMP EnumDisplayAttributes::Clone(_Out_ IEnumTfDisplayAttributeInfo ** ppEnum) {
	COM_PROLOG
		auto ret = com_attach(new EnumDisplayAttributes(m_idx));
		*ppEnum = ret.release();
		return S_OK;
	COM_EPILOG
}
STDMETHODIMP EnumDisplayAttributes::Next(ULONG ulCount, 
	                                     __RPC__out_ecount_part(ulCount, *pcFetched) ITfDisplayAttributeInfo ** rgInfo, 
									     __RPC__out ULONG * pcFetched) {
	COM_PROLOG
		std::vector<com_shared_ptr<ITfDisplayAttributeInfo>> temp;
		ULONG fetched;
		for (fetched = 0; fetched != ulCount && m_idx != std::size(s_factories); ++fetched, ++m_idx) {
			temp.push_back(s_factories[m_idx]());
		}
		for(ULONG i = 0; i < fetched; ++i) {
			rgInfo[i] = temp[i].release();
		}
		if (pcFetched)
			*pcFetched = fetched;
		return (fetched == ulCount) ? S_OK : S_FALSE;
	COM_EPILOG
}
STDMETHODIMP EnumDisplayAttributes::Reset() {
	m_idx = 0;
	return S_OK;
}

STDMETHODIMP EnumDisplayAttributes::Skip(ULONG ulCount) {
	m_idx += std::min(ulCount, ULONG(std::size(s_factories) - m_idx));
	return S_OK;
}


STDMETHODIMP DisplayAttributeCompositionInfo::GetGUID(_Out_ GUID * pguid) {
	*pguid = __uuidof(*this);
	return S_OK;
}

STDMETHODIMP DisplayAttributeCompositionInfo::GetDescription(_Out_ BSTR *pbstrDesc) {

	COM_PROLOG
		*pbstrDesc = sys_string_bstr("Translit Composition Attribute").release();
		return S_OK;
	COM_EPILOG
}

STDMETHODIMP DisplayAttributeCompositionInfo::GetAttributeInfo(_Out_ TF_DISPLAYATTRIBUTE * pTSFDisplayAttr) {
	static const TF_DISPLAYATTRIBUTE info = {
		{ TF_CT_SYSCOLOR, COLOR_BTNTEXT },    // text color
		{ TF_CT_SYSCOLOR, COLOR_BTNFACE },    // background color (TF_CT_NONE => app default)
		TF_LS_DOT,							  // underline style
		false,								  // underline boldness
		{ TF_CT_SYSCOLOR, COLOR_BTNTEXT },    // underline color
		TF_ATTR_INPUT					      // attribute info
	};
	*pTSFDisplayAttr = info;
	return S_OK;
}

STDMETHODIMP DisplayAttributeCompositionInfo::SetAttributeInfo(_In_ const TF_DISPLAYATTRIBUTE * /*ptfDisplayAttr*/) {
	return E_NOTIMPL;
}

STDMETHODIMP DisplayAttributeCompositionInfo::Reset() {
	return S_OK;
}

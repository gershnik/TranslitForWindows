// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/ComObject.h>

template<std::invocable<TfEditCookie> Func>
class EditSession final : public ComObject<EditSession<Func>, ITfEditSession> {

	friend ref_counted;

public:
	EditSession(const Func & func):
		m_func(func)
	{}
	EditSession(Func && func):
		m_func(std::move(func))
	{}

	// ITfEditSession
	STDMETHODIMP DoEditSession(TfEditCookie cookie) override {
		COM_PROLOG
			m_func(cookie);
			return S_OK;
		COM_EPILOG
	}

private:
	Func m_func;
};

void doEditSession(SmartOrDumb<ITfContext> auto && context, TfClientId clientId, DWORD flags, std::invocable<TfEditCookie> auto && func) {
	auto session = com_attach(new EditSession(std::forward<decltype(func)>(func)));
	HRESULT hres;
	comTest(std::forward<decltype(context)>(context)->RequestEditSession(clientId, session.get(), flags, &hres));
	comTest(hres);
}
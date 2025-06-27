// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "err.h"

template<class T, class Iface>
concept SmartOrDumb = std::is_convertible_v<T, Iface *> ||  
					  std::is_convertible_v<T, const com_shared_ptr<Iface> &>;

template<class Iface>
inline
Iface * c_ptr(Iface * src) 
	{ return src; }

template<class Iface>
inline
Iface * c_ptr(const com_shared_ptr<Iface> & src) 
	{ return src.get(); }


template<class Dest, class Src>
com_shared_ptr<Dest> com_cast(Src && ptr) {
	if (!ptr)
		return {};
	com_shared_ptr<Dest> ret;
	comTest(std::forward<Src>(ptr)->QueryInterface(__uuidof(Dest), std::out_ptr(ret)));
	return ret;
}

template<class Dest, class Src>
com_shared_ptr<Dest> com_as(Src && ptr) {
	if (!ptr)
		return {};
	com_shared_ptr<Dest> ret;
	HRESULT hres = std::forward<Src>(ptr)->QueryInterface(__uuidof(Dest), std::out_ptr(ret));
	if (SUCCEEDED(hres))
		return ret;
	if (hres == E_NOINTERFACE)
		return {};
	throwHresult(hres);
}

template<class Dest, class Src>
com_shared_ptr<Dest> com_as_noexcept(Src && ptr) noexcept {
	if (!ptr)
		return {};
	com_shared_ptr<Dest> ret;
	HRESULT hres = std::forward<Src>(ptr)->QueryInterface(__uuidof(Dest), std::out_ptr(ret));
	if (SUCCEEDED(hres))
		return ret;
	return {};
}


template<class Dest>
com_shared_ptr<Dest> comCreate(_In_ REFCLSID rclsid,
							   _In_opt_ LPUNKNOWN pUnkOuter,
							   _In_ DWORD dwClsContext) {
	com_shared_ptr<Dest> ret;
	comTest(CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(Dest), std::out_ptr(ret)));
	return ret;
}

struct CoTaskDeleter {
	void operator()(void * ptr) { CoTaskMemFree(ptr); }
};

template<class T>
requires(
	(!std::is_array_v<T> && std::is_trivially_destructible_v<T>) ||
	(std::is_array_v<T> && std::is_trivially_destructible_v<std::remove_all_extents_t<T>>)
)
using unqiue_co_membuf = std::unique_ptr<T, CoTaskDeleter>;

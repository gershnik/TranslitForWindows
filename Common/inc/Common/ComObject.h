// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/com.h>
#include <Common/TypeList.h>
#include <Common/win32api.h>


template<std::derived_from<IUnknown> Iface>
struct InterfacesOf {
	using type = TypeList<IUnknown, Iface>;
};


template<class T, class Impl>
bool doComQueryInterface(Impl * obj, REFIID riid, _Outptr_ void ** ret) noexcept {

	if (riid == __uuidof(T)) {
		if constexpr (std::is_convertible_v<Impl *, T *>) {
			*ret = static_cast<T *>(obj);
			return true;
		} else {
			T * res;
			static_assert(noexcept(obj->query(&res)));
			obj->query(&res);
			*ret = res;
			return true;
		}
	}
	*ret = nullptr;
	return false;
}

template<class SupportedIfacesTL, class Impl>
bool comQueryInterface(Impl * obj, REFIID riid, _Outptr_ void ** ret) noexcept {
	if constexpr (SupportedIfacesTL::size > 0) {
		if (doComQueryInterface<typename SupportedIfacesTL::head>(obj, riid, ret)) {
			return true;
		}
		return comQueryInterface<typename SupportedIfacesTL::tail>(obj, riid, ret);
	} else {
		*ret = nullptr;
		return false;
	}
}

template<class... Bases>
using SupportedIfacesOf = TypeListMakeSet<TypeListConcat<typename InterfacesOf<Bases>::type...>>;


template<class Derived, class... Bases>
class ComObject:
	public Bases...,
	public ref_counted<Derived, ref_counted_flags::none, ULONG>,
	private ModuleReference {

	friend ref_counted;

public:
	STDMETHODIMP QueryInterface(REFIID riid, _Outptr_ void ** ret) final {
		*ret = nullptr;

#ifndef NDEBUG
		debugPrint("Translit:  {} was asked for interface: {}\n", typeid(*this).name(), uuid(riid));
#endif

		auto * me = static_cast<Derived *>(this);

		if (comQueryInterface<SupportedIfacesOf<Bases...>>(me, riid, ret)) {
			this->add_ref();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() final {
		this->add_ref();
		return 42;
	}
	STDMETHODIMP_(ULONG) Release() final {
		this->sub_ref();
		return 42;
	}
};

template<class Derived, class... Bases>
class ComStaticObject: public Bases... {

public:
	STDMETHODIMP QueryInterface(REFIID riid, _Outptr_ void ** ret) final {
		*ret = nullptr;

		auto * me = static_cast<Derived *>(this);

		if (comQueryInterface<SupportedIfacesOf<Bases...>>(me, riid, ret)) {
			Module::add_ref();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	STDMETHODIMP_(ULONG) AddRef() final {
		return Module::add_ref();
	}
	STDMETHODIMP_(ULONG) Release() final {
		return Module::sub_ref();
	}

	void * operator new(size_t) = delete;
	void * operator new[](std::size_t) = delete; 
	void * operator new(size_t, std::align_val_t) = delete;
	void * operator new[](std::size_t, std::align_val_t) = delete;
};

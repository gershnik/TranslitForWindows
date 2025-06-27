// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/err.h>
#include <Common/win32api.h>

template<class Derived>
class Window : public weak_ref_counted<Derived>{
public:
	HWND handle() const noexcept
		{ return m_wnd.get(); }

private:
	struct Registrator {
		ATOM atom;
	
		Registrator() {
			WNDCLASSEX cls{};
			Derived::fillClassInfo(cls);
			cls.cbSize = sizeof(cls);
			cls.lpfnWndProc = Window::wndProc;
			cls.cbWndExtra += sizeof(void*);
			cls.hInstance = Module::handle();
			this->atom = RegisterClassEx(&cls);
			if (!this->atom)
				throwLastError();
		}
		~Registrator() {
			UnregisterClass(MAKEINTATOM(this->atom), Module::handle());
		}

		Registrator(const Registrator &) = delete;
		Registrator & operator=(const Registrator &) = delete;
	};

public:
	static ATOM getWindowClass() {
		static Registrator registration;
		return registration.atom;
	}

protected:
	static intrusive_shared_ptr<Derived, typename Window::refcnt_ptr_traits> 
				create(DWORD dwExStyle, 
		               LPCWSTR lpWindowName,
					   DWORD dwStyle,
					   int x, int y, int width, int height, 
					   HWND hWndParent, HMENU hMenu, void * param) {
		HWND hwnd = CreateWindowEx(dwExStyle, 
								   MAKEINTATOM(getWindowClass()), 
			                       lpWindowName, 
							       dwStyle, 
								   x, y, width, height, 
								   hWndParent, hMenu, Module::handle(), param);
		if (!hwnd)
			throwLastError();
		auto ret = reinterpret_cast<Derived *>(GetWindowLongPtr(hwnd, 0));
		if (!ret)
			abort();
		return refcnt_attach(ret);
	}

	Window(HWND hwnd) noexcept:
		m_wnd(hwnd) { 

		SetLastError(0);
		auto res = SetWindowLongPtr(hwnd, 0, reinterpret_cast<LONG_PTR>(static_cast<Derived *>(this)));
		if (auto err = GetLastError(); res == 0 && err != 0)
			throwWinError(err);
	}
	~Window() {
		SetWindowLongPtr(m_wnd.get(), 0, reinterpret_cast<LONG_PTR>(nullptr));
	}

	LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
		return DefWindowProc(m_wnd.get(), msg, wParam, lParam);
	}
private:
	static LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		if (msg == WM_NCCREATE) {
			auto * cs = reinterpret_cast<CREATESTRUCT *>(lParam);
			new Derived(hwnd, *cs);
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		if (auto me = reinterpret_cast<Derived *>(GetWindowLongPtr(hwnd, 0)))
			return me->handleMessage(msg, wParam, lParam);

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	
private:
	unique_wnd m_wnd;
};


// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

extern "C" IMAGE_DOS_HEADER __ImageBase;

class Module {
public:
	static HMODULE handle() noexcept
		{ return HMODULE(&__ImageBase); }
	
	static ULONG add_ref() noexcept;
	static ULONG sub_ref() noexcept;
private:
	Module() = default;
	~Module() = default;
	Module(const Module &) = delete;
	Module & operator=(const Module &) = delete;
};

struct ModuleReference {
	ModuleReference()
	{ Module::add_ref(); }
	~ModuleReference()
	{ Module::sub_ref(); }
	ModuleReference(const ModuleReference &) = delete;
	ModuleReference & operator=(const ModuleReference &) = delete;
};

template<class... Args>
inline auto debugPrint(std::format_string<Args...> fmt, Args &&... args) {
	auto str = sys_string::std_format(fmt, std::forward<Args>(args)...);
	OutputDebugStringW(str.w_str());
}

inline sys_string getModuleFileName(HMODULE hModule) {
	sys_string_builder builder;
	
	DWORD size = 32;
	for( ; ; ) {
		builder.resize_storage(size);
		auto res = GetModuleFileNameW(hModule, (wchar_t *)builder.chars().begin(), size);
		if (res < size) {
			builder.resize_storage(res);
			break;
		}
		auto err = GetLastError();
		if (err == NO_ERROR)
			break;
		if (err == ERROR_INSUFFICIENT_BUFFER) {
			if (err < std::numeric_limits<DWORD>::max() / 2)
				size *= 2;
			else if (err < std::numeric_limits<DWORD>::max() - 32)
				size += 32;
			else
				throw std::bad_alloc();
			continue;
		}
		throw std::system_error(std::error_code(err, std::system_category()));
	}
	return builder.build();
}

struct WindowDeleter { void operator() (HWND hwnd) noexcept { if (hwnd) DestroyWindow(hwnd); } };

using unique_wnd = std::unique_ptr<std::remove_pointer_t<HWND>, WindowDeleter>;

struct FontDeleter { void operator() (HFONT hf) noexcept { if (hf) DeleteObject(hf); } };

using unique_font = std::unique_ptr<std::remove_pointer_t<HFONT>, FontDeleter>;

struct DeviceContextDeleter { 
	HWND hwnd = nullptr;

	void operator() (HDC hdc) { if (hdc) ReleaseDC(hwnd, hdc); } 
};

using unique_dc = std::unique_ptr<std::remove_pointer_t<HDC>, DeviceContextDeleter>;

struct GdiObjectDeleter { void operator() (HGDIOBJ obj) noexcept { if (obj) DeleteObject(obj); } };

using unique_brush = std::unique_ptr<std::remove_pointer_t<HBRUSH>, GdiObjectDeleter>;


struct LocalAllocDeleter {
	void operator()(void * ptr) { LocalFree(ptr); }
};

template<class T>
requires(
	(!std::is_array_v<T> && std::is_trivially_destructible_v<T>) ||
	(std::is_array_v<T> && std::is_trivially_destructible_v<std::remove_all_extents_t<T>>)
)
using unqiue_local_membuf = std::unique_ptr<T, LocalAllocDeleter>;

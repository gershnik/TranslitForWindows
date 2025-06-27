// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/err.h>

class RegKey {
public:
	static RegKey * from(HKEY handle)
		{ return reinterpret_cast<RegKey *>(handle); }
	static HKEY c_handle(RegKey * key) noexcept 
		{ return reinterpret_cast<HKEY>(key); }
	static HKEY c_handle(RegKey & key) noexcept 
		{ return c_handle(&key); }

	HKEY c_handle() noexcept 
		{ return c_handle(this); }
	
	
	void operator delete(void *) noexcept
	{}

	~RegKey() noexcept {
		RegCloseKey(c_handle());
	}

	std::unique_ptr<RegKey> create(const wchar_t * sub_key, 
								   DWORD options, 
								   REGSAM sam, 
								   SECURITY_ATTRIBUTES * security, 
								   DWORD * disposition,
								   TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		HKEY ret = 0;

		auto status = RegCreateKeyExW(c_handle(), sub_key, 0, nullptr, options, sam, security, &ret, disposition);
		if (status != ERROR_SUCCESS)
			handleError(TL_ERROR_REF(err), status, L"RegCreateKeyExW({})", sub_key);
		else
			clearError(TL_ERROR_REF(err));
		return std::unique_ptr<RegKey>{from(ret)};
	}

	std::unique_ptr<RegKey> create(const wchar_t * sub_key, 
								   DWORD options, 
								   REGSAM sam,
								   TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		return create(sub_key, options, sam, nullptr, nullptr, TL_ERROR_REF(err));
	}

	std::unique_ptr<RegKey> create(const wchar_t * sub_key, 
								   DWORD options, 
								   REGSAM sam,
								   DWORD * disposition,
								   TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		return create(sub_key, options, sam, nullptr, &disposition, TL_ERROR_REF(err));
	}

	std::unique_ptr<RegKey> create(const wchar_t * sub_key, 
								   DWORD options, 
								   REGSAM sam,
								   SECURITY_ATTRIBUTES * security,
								   TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		return create(sub_key, options, sam, security, nullptr, TL_ERROR_REF(err));
	}

	std::unique_ptr<RegKey> open(const wchar_t * sub_key, 
								 DWORD options, 
								 REGSAM sam,
								 TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {

		HKEY ret = 0;

		auto status = RegOpenKeyExW(c_handle(), sub_key, options, sam, &ret);
		if (status != ERROR_SUCCESS)
			handleError(TL_ERROR_REF(err), status, L"RegOpenKeyExW({})", sub_key);
		else
			clearError(TL_ERROR_REF(err));
		return std::unique_ptr<RegKey>{from(ret)};

	}

	void deleteSubKey(const wchar_t * name, REGSAM sam, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		auto status = RegDeleteKeyExW(c_handle(), name, sam, 0);
		if (status != ERROR_SUCCESS)
			handleError(TL_ERROR_REF(err), status, L"RegDeleteKeyExW({})", name);
		else
			clearError(TL_ERROR_REF(err));
	}

	void deleteSubKey(const wchar_t * name, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		deleteSubKey(name, 0, TL_ERROR_REF(err));
	}

	void deleteTree(const wchar_t * name, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		auto status = RegDeleteTreeW(c_handle(), name);
		if (status != ERROR_SUCCESS)
			handleError(TL_ERROR_REF(err), status, L"RegDeleteTreeW({})", name);
		else
			clearError(TL_ERROR_REF(err));
	}

	
	void setValue(const wchar_t * name, DWORD type, std::span<const BYTE> data, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		auto status = RegSetValueExW(c_handle(), name, 0, type, data.data(), DWORD(data.size()));
		if (status != ERROR_SUCCESS)
			handleError(TL_ERROR_REF(err), status, L"RegSetValueExW({}, {})", name, type);
		else
			clearError(TL_ERROR_REF(err));
	}

	size_t getValue(const wchar_t * name, DWORD expectedType, std::span<BYTE> data, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		DWORD size = DWORD(data.size());
		DWORD type;
		auto status = RegQueryValueExW(c_handle(), name, nullptr, &type, data.data(), &size);
		if (status != ERROR_SUCCESS) {
			handleError(TL_ERROR_REF(err), status, L"RegQueryValueExW({})", name);
		} else if (type != expectedType) {
			handleError(TL_ERROR_REF(err), ERROR_FILE_NOT_FOUND, L"RegQueryValueExW({}) for type {}, got wrong type: {}", name, expectedType, type);
		} else {
			clearError(TL_ERROR_REF(err));
		}
		return size_t(size);
	}

	void setStringValue(const wchar_t * name, const wchar_t * str, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		setValue(name, REG_SZ, {(const BYTE *)str, (wcslen(str) + 1) * sizeof(wchar_t)}, TL_ERROR_REF(err));
	}
	void setDwordValue(const wchar_t * name, DWORD val, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		setValue(name, REG_DWORD, {(const BYTE *)&val, sizeof(val)}, TL_ERROR_REF(err));
	}
	void setQwordValue(const wchar_t * name, ULONGLONG val, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		setValue(name, REG_QWORD, {(const BYTE *)&val, sizeof(val)}, TL_ERROR_REF(err));
	}

	DWORD getDwordValue(const wchar_t * name, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		DWORD ret = 0;
		getValue(name, REG_DWORD, {LPBYTE(&ret), sizeof(ret)}, TL_ERROR_REF(err));
		return ret;
	}

	ULONGLONG getQwordValue(const wchar_t * name, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		ULONGLONG ret = 0;
		getValue(name, REG_QWORD, {LPBYTE(&ret), sizeof(ret)}, TL_ERROR_REF(err));
		return ret;
	}

	std::vector<wchar_t> getMultiStringValue(const wchar_t * name, TL_ERROR_REF_ARG(err)) requires(TL_ERROR_REQ(err)) {
		std::vector<wchar_t> ret(16);
		for ( ; ; ) {
			std::error_code ec;
			size_t byteSize = getValue(name, REG_MULTI_SZ, {LPBYTE(ret.data()), ret.size() * sizeof(wchar_t)}, ec);
			ret.resize(byteSize / sizeof(wchar_t));
			if (!ec) {
				if (ret.size() == 0 || ret.back() != 0)
					ret.push_back(0);
				if (ret.size() > 1 && ret[ret.size() - 2] != 0)
					ret.push_back(0);
				clearError(TL_ERROR_REF(err));
				return ret;
			}
			if (ec.value() != ERROR_MORE_DATA) {
				handleError(TL_ERROR_REF(err), ec.value(), L"RegQueryValueExW({}) for REG_MULTI_SZ", name);
				return ret;
			}
		}
	}
	

public:
	static inline RegKey * const classesRoot = from(HKEY_CLASSES_ROOT);
	static inline RegKey * const currentConfig = from(HKEY_CURRENT_CONFIG);
	static inline RegKey * const currentUser = from(HKEY_CURRENT_USER);
	static inline RegKey * const localMachine = from(HKEY_LOCAL_MACHINE);
	static inline RegKey * const users = from(HKEY_USERS);

private:
	RegKey() = delete;
	RegKey(const RegKey &) = delete;
	RegKey & operator=(const RegKey &) = delete;
};

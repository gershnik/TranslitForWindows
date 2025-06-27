// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct Error {
    enum ErrorType {
        Windows,
        Posix
    };

    constexpr Error(int code_ = 0) noexcept: type(Windows), code(code_)
    {}
    constexpr Error(ErrorType type_, int code_) noexcept: type(type_), code(code_)
    {}

    constexpr bool operator==(const Error & rhs) const = default;
    constexpr bool operator!=(const Error & rhs) const = default;

    explicit operator bool() const noexcept 
		{ return this->code != 0; }

    ErrorType type;
    int code;
};

__forceinline auto makeErrorCode(Error err) -> std::error_code {
	return std::error_code(err.code, err.type == Error::Posix ? std::generic_category() : std::system_category());
}

template<class... T>
[[noreturn]] __forceinline void throwErrorCode(Error err, std::wformat_string<T...> fmt, T && ...args) noexcept(false) {
    //do not attempt to allocate on ENOMEM
    if ((err.type == Error::Posix && err.code == ENOMEM) ||
        (err.type == Error::Windows && (err.code == ERROR_OUTOFMEMORY || 
										err.code == ERROR_NOT_ENOUGH_MEMORY ||
										err.code == E_OUTOFMEMORY)))
        throw std::system_error(makeErrorCode(err));

    throw std::system_error(makeErrorCode(err),
        sys_string_generic(std::format(fmt, std::forward<T>(args)...)).c_str());
}

[[noreturn]] __forceinline void throwErrorCode(Error err) noexcept(false) {
    throw std::system_error(makeErrorCode(err));
}

template<class T> struct ErrorTraits;

template<class T>
concept ErrorSink = requires(T & obj, const T & cobj) {
	{ ErrorTraits<T>::assignError(obj, Error(), L"") } -> std::same_as<void>;
	{ ErrorTraits<T>::clearError(obj) } noexcept -> std::same_as<void>;
	{ ErrorTraits<T>::failed(cobj) } noexcept -> std::same_as<bool>;
};

template<ErrorSink Err, class... T>
__forceinline void handleError(Err & dest, Error err, std::wformat_string<T...> fmt, T && ...args) 
    { ErrorTraits<Err>::assignError(dest, err, fmt, std::forward<T>(args)...); }

template<class... T>
[[noreturn]] __forceinline void handleError(Error err, std::wformat_string<T...> fmt, T && ...args) noexcept(false) 
    { throwErrorCode(err, fmt, std::forward<T>(args)...); }

template<ErrorSink Err>
__forceinline void clearError(Err & err) noexcept 
    { ErrorTraits<Err>::clearError(err); }

__forceinline constexpr void clearError() noexcept 
    {}

template<ErrorSink Err>
__forceinline auto failed(const Err & err) noexcept -> bool 
    { return ErrorTraits<Err>::failed(err); }

__forceinline constexpr auto failed() noexcept -> bool
    { return false; }

template<> struct ErrorTraits<Error> {
    template<class... T>
    static __forceinline void assignError(Error & dest, Error err, std::wformat_string<T...>, T && ...) noexcept {
        dest = err;
    }

    static __forceinline void clearError(Error & err) noexcept {
        err = 0;
    }

    static __forceinline auto failed(const Error & err) noexcept -> bool {
        return err.code != 0;
    }
};

template<> struct ErrorTraits<std::error_code> {
    template<class... T>
    static __forceinline void assignError(std::error_code & dest, Error err, std::wformat_string<T...> /*fmt*/, T && ...) noexcept {
        dest = makeErrorCode(err);
    }

    static __forceinline void clearError(std::error_code & err) noexcept {
        err.clear();
    }

    static __forceinline auto failed(const std::error_code & err) noexcept -> bool {
        return bool(err);
    }
};

template<Error First, Error... Rest>
class AllowedErrors {
public:
    auto code() const noexcept -> Error 
        { return m_code; }

    auto assign(Error err) noexcept -> bool {
        for(auto allowed: {First, Rest...}) {
            if (err == allowed) {
                m_code = err;
                return true;
            }
        }
        return false;
    }

    void clear() noexcept 
        { m_code = 0; }

    explicit operator bool() const noexcept 
        { return bool(m_code); }
private:
    Error m_code;
};

template<Error First, Error... Rest> 
struct ErrorTraits<AllowedErrors<First, Rest...>> {
    template<class... T>
    static __forceinline void assignError(AllowedErrors<First, Rest...> & dest, Error err, std::wformat_string<T...> fmt, T && ...args) {
        if (!dest.assign(err))
            throwErrorCode(err, fmt, std::forward<T>(args)...);
    }
        
    static __forceinline void clearError(AllowedErrors<First, Rest...> & err) noexcept {
        err.clear();
    }

    static __forceinline auto failed(const AllowedErrors<First, Rest...> & err) noexcept -> bool {
        return bool(err);
    }
};


#define TL_ERROR_REF_ARG(x) ErrorSink auto & ...x
#define TL_ERROR_REQ(x) (sizeof...(x) < 2)
#define TL_ERROR_PRESENT(x) (sizeof...(x) == 1)
#define TL_ERROR_REF(x) x...

inline HRESULT hresult_from(std::error_code err) {
	if (!err)
		return S_OK;
	if (err.category() == std::system_category()) {
		DWORD val = err.value();
		if (val & 0x8000000)
			return HRESULT(val);
		return HRESULT_FROM_WIN32(val);
	}
	return E_FAIL;
}

#define COM_PROLOG \
	try {
#define COM_EPILOG \
    } catch (std::system_error & err) { \
		return hresult_from(err.code()); \
	} catch (std::bad_alloc &) { \
		return E_OUTOFMEMORY; \
	} catch (std::exception &) { \
		return E_FAIL; \
	}

template<class... T>
[[noreturn]]
__forceinline auto throwWinError(DWORD err, std::wformat_string<T...> fmt, T && ...args) {
	throwErrorCode(err, fmt, std::forward<T>(args)...);
}

[[noreturn]]
__forceinline auto throwWinError(DWORD err) {
    throwErrorCode(err);
}

template<class... T>
[[noreturn]]
__forceinline auto throwLastError(std::wformat_string<T...> fmt, T && ...args) {
    throwErrorCode(GetLastError(), fmt, std::forward<T>(args)...);
}

[[noreturn]]
__forceinline auto throwLastError() {
    throwErrorCode(GetLastError());
}

template<class... T>
[[noreturn]]
__forceinline auto throwHresult(HRESULT hres, std::wformat_string<T...> fmt, T && ...args) {
    throwErrorCode(hres, fmt, std::forward<T>(args)...);
}

[[noreturn]]
__forceinline auto throwHresult(HRESULT hres) {
    throwErrorCode(hres);
}

inline bool comSucceeded(HRESULT hres) 
	{ return SUCCEEDED(hres); }

inline bool comFailed(HRESULT hres) 
	{ return FAILED(hres); }

inline auto comTest(HRESULT hres) -> HRESULT
{
	if (FAILED(hres))
		throwHresult(hres);
	return hres;
}

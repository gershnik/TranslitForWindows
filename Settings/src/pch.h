// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include <windows.h>
#include <msctf.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <vssym32.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Core.h>


#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <map>
#include <set>
#include <span>
#include <ranges>
#include <filesystem>

#include <intrusive_shared_ptr/refcnt_ptr.h>
#include <intrusive_shared_ptr/ref_counted.h>
#include <intrusive_shared_ptr/com_ptr.h>

#include <modern-uuid/uuid.h>

#include <sys_string/sys_string.h>

#if defined(_M_ARM64)
	#define RAPIDJSON_ENDIAN RAPIDJSON_LITTLEENDIAN
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#ifndef DWL_MSGRESULT
#define DWL_MSGRESULT 0
#endif

using namespace std::filesystem;
using namespace isptr;
using namespace muuid;
using namespace sysstr;

namespace rapidjson {
	using WDocument = GenericDocument<UTF16<>>;
	using WValue = GenericValue<UTF16<>>;
	using WStringBuffer = GenericStringBuffer<UTF16<>>;
	template<class Buffer>
	using WWriter = Writer<Buffer, UTF16<>>;
}

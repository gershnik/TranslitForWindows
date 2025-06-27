// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include <windows.h>
#include <msctf.h>
#include <ctffunc.h>
#include <olectl.h>
#include <msi.h>
#include <Msiquery.h>

#include <utility>
#include <type_traits>
#include <concepts>
#include <expected>
#include <memory>
#include <span>
#include <map>
#include <set>
#include <cassert>

#include <intrusive_shared_ptr/refcnt_ptr.h>
#include <intrusive_shared_ptr/ref_counted.h>
#include <intrusive_shared_ptr/com_ptr.h>

#include <modern-uuid/uuid.h>

#include <sys_string/sys_string.h>

using namespace isptr;
using namespace muuid;
using namespace sysstr;
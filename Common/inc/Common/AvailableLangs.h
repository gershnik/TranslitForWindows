// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/com.h>
#include <Common/RegKey.h>

inline bool isLatinOnly(LANGID langid) {
    // LOCALE_NAME_MAX_LENGTH is the documented maximum for locale names
    wchar_t name[LOCALE_NAME_MAX_LENGTH];
    if (!LCIDToLocaleName(MAKELCID(langid, SORT_DEFAULT), name, int(std::size(name)), LOCALE_ALLOW_NEUTRAL_NAMES))
        return false;

    std::vector<wchar_t> scripts(16);
    int res;
    for ( ; ; ) {
        res = GetLocaleInfoEx(name, LOCALE_SSCRIPTS, scripts.data(), int(scripts.size()));
        if (res != 0)
            break;
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return false;
        int needed = GetLocaleInfoEx(name, LOCALE_SSCRIPTS, nullptr, 0);
        if (needed == 0)
            return false;
        scripts.resize(size_t(needed));
    }

    constexpr std::wstring_view latin = L"Latn;";
    return size_t(res) == latin.size() + 1 &&
        wmemcmp(scripts.data(), latin.data(), latin.size()) == 0;
}

class AvailableLangs {
public:
    AvailableLangs() = default;

    bool loadFromUserSid(const sys_string & userSid, std::error_code & ec) {
        sys_string path = userSid + S("\\Control Panel\\International\\User Profile");
        auto key = RegKey::users->open(path.w_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, ec);
        if (ec)
            return false;
        auto names = key->getMultiStringValue(L"Languages", ec);
        if (ec)
            return false;
        for (const wchar_t * name = names.data(); *name; name += wcslen(name) + 1) {
            LCID lcid = LocaleNameToLCID(name, 0);
            if (lcid != 0 && lcid != LOCALE_CUSTOM_UNSPECIFIED && isLatinOnly(LANGIDFROMLCID(lcid)))
                m_data.push_back(LANGIDFROMLCID(lcid));
        }
        return true;
    }

    void load(const com_shared_ptr<ITfInputProcessorProfiles> & inputProcessorProfiles) {
        unique_co_membuf<LANGID> langIds;
        ULONG count = 0;
        comTest(inputProcessorProfiles->GetLanguageList(std::out_ptr(langIds), &count));
        auto langSpan = std::span<LANGID>(langIds.get(), size_t(count));
        auto langSpanEnd = std::remove_if(langSpan.begin(), langSpan.end(), [](auto langid) { return !isLatinOnly(langid); });
        langSpan = std::span<LANGID>(langSpan.begin(), langSpanEnd);
        m_data.assign(langSpan.begin(), langSpan.end());
    }

    bool empty() const
        { return m_data.empty(); }

    LANGID front() const
        { return m_data.front(); }

    void add(LANGID langId)
        { m_data.push_back(langId); }

    using iterator = std::vector<LANGID>::const_iterator;
    iterator begin() const { return m_data.cbegin(); }
    iterator end() const { return m_data.cend(); }
private:
    std::vector<LANGID> m_data;
};

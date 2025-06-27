// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later
             
// THIS FILE IS AUTO-GENERATED. DO NOT EDIT.

#include <Translit/Languages.h>
#include <Translit/Identifiers.h>

#include <Mapper/Transliterator.hpp>

#include "../res/resource.h"

#include "../tables/TableBE.hpp"
#include "../tables/TableHE.hpp"
#include "../tables/TableRU.hpp"
#include "../tables/TableUK.hpp"

using MappingFunc = Transliterator::MappingFunc;
using Range = Transliterator::Range;

struct RealMappingInfo : public MappingInfo {
    Transliterator::MappingFunc * mapper;
};

template<size_t N>
constexpr auto structArrayToPtrArray(const RealMappingInfo (&structs)[N]) {
    std::array<const MappingInfo *, N> ret;
    for (size_t i = 0; i < N; ++i)
        ret[i] = &structs[i];
    return ret;
}


constexpr RealMappingInfo g_beEntries[] = {
    { L"", L"default", g_mapperBeDefault<Range> },
    { L"translit-ru", L"translit.net", g_mapperBeTranslitRu<Range> },
};
constexpr auto g_be = structArrayToPtrArray(g_beEntries);

constexpr RealMappingInfo g_heEntries[] = {
    { L"", L"default", g_mapperHeDefault<Range> },
};
constexpr auto g_he = structArrayToPtrArray(g_heEntries);

constexpr RealMappingInfo g_ruEntries[] = {
    { L"", L"default", g_mapperRuDefault<Range> },
    { L"translit-ru", L"translit.ru", g_mapperRuTranslitRu<Range> },
};
constexpr auto g_ru = structArrayToPtrArray(g_ruEntries);

constexpr RealMappingInfo g_ukEntries[] = {
    { L"", L"default", g_mapperUkDefault<Range> },
    { L"translit-ru", L"translit.net", g_mapperUkTranslitRu<Range> },
};
constexpr auto g_uk = structArrayToPtrArray(g_ukEntries);

inline constexpr const ProfileInfo g_profiles[] = {
   {&g_guidProfileBe, L"be", L"Belarusian", L"Belarusian Translit", IDI_ICON_BE, &g_guidCompartmentBe, g_be},
   {&g_guidProfileHe, L"he", L"Hebrew", L"Hebrew Translit", IDI_ICON_HE, nullptr, g_he},
   {&g_guidProfileRu, L"ru", L"Russian", L"Russian Translit", IDI_ICON_RU, &g_guidCompartmentRu, g_ru},
   {&g_guidProfileUk, L"uk", L"Ukrainian", L"Ukrainian Translit", IDI_ICON_UK, &g_guidCompartmentUk, g_uk}
};

auto getProfiles() -> std::span<const ProfileInfo> {
    return g_profiles;
}

auto getProfileById(const GUID & profileId) -> const ProfileInfo * {
    for(auto & profile: g_profiles) {
        if (*profile.id == profileId)
            return &profile;
    }
    return nullptr;
}

auto getMapper(const MappingInfo * info) -> Transliterator::MappingFunc * {
    return static_cast<const RealMappingInfo *>(info)->mapper;
}


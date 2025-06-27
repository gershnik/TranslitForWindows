// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct MappingInfo {
    const wchar_t * name;
    const wchar_t * displayName;
};

struct ProfileInfo {
    const GUID * id;
    const wchar_t * lang;
    const wchar_t * displayName;
    const wchar_t * description;
    int icondId;
    const GUID * mappingCompartmentId;
    std::span<const MappingInfo * const> mappings;
};

auto getProfiles() -> std::span<const ProfileInfo>;
auto getProfileById(const GUID & profileId) -> const ProfileInfo *;

// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

inline constexpr auto g_translitServiceId = uuid("29041c62-d41d-489a-b299-864946c60c2e").to_GUID();
inline constexpr auto g_translitClassId = g_translitServiceId;

inline constexpr auto g_guidProfileRu = uuid("e5e6a165-0079-4cd9-90d6-b448de4514df").to_GUID();
inline constexpr auto g_guidCompartmentRu = uuid("007a9616-729d-49df-9fbf-6577be812773").to_GUID();

inline constexpr auto g_guidProfileHe = uuid("d57a993b-449e-4355-bf62-f279772ccf27").to_GUID();
//inline constexpr auto g_guidCompartmentHe = uuid("73cbf84b-1b64-494a-8a56-0752d5bba351").to_GUID();

inline constexpr auto g_guidProfileUk = uuid("bb3dfa5f-a141-4fa6-9f69-3baef0023d34").to_GUID();
inline constexpr auto g_guidCompartmentUk = uuid("d2522504-8d32-4595-9081-25676967c288").to_GUID();

inline constexpr auto g_guidProfileBe = uuid("289f9539-fe8a-4e32-93c1-ba413b6b853c").to_GUID();
inline constexpr auto g_guidCompartmentBe = uuid("3a69f7db-a601-4f96-9d23-fb30ec9815b6").to_GUID();

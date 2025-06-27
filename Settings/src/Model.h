// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Translit/Languages.h>
#include <Translit/Identifiers.h>

#include <Common/RegKey.h>

class Model : public weak_ref_counted<Model> {
	friend ref_counted;
private:
	class ThreadMgrActivation {
	public:
		ThreadMgrActivation();
		~ThreadMgrActivation();

		TfClientId clientId() const
			{ return m_clientId; }
	private:
		Model * getOwner() {
			return (Model *)(reinterpret_cast<uint8_t *>(this) - offsetof(Model, m_threadMgrActivation));
		}
	private:
		TfClientId m_clientId;
	};

	class ActiveLanguageProfileNotifySink;

	class ActiveLanguageProfileNotifyRegistration {
	public:
		ActiveLanguageProfileNotifyRegistration();
		~ActiveLanguageProfileNotifyRegistration();
	private:
		Model * getOwner() {
			return (Model *)(reinterpret_cast<uint8_t *>(this) - offsetof(Model, m_activeLanguageProfileNotifyRegistration));
		}
	private:
		DWORD m_cookie = TF_INVALID_COOKIE;
	};

	class ProfileMappingInfo {
	public:
		ProfileMappingInfo(const com_shared_ptr<ITfCompartmentMgr> & compMgr, TfClientId clientId, 
						   const GUID & mappingCompartmentId, int maxIdx);

		int mappingIndex() const 
			{ return m_mappingIdx; }

		void setMappingIndex(TfClientId clientId, int idx);
	private:
		com_shared_ptr<ITfCompartment> m_mappingCompartment;
		std::unique_ptr<RegKey> m_compartmentKey;
		sys_string m_regValueName;
		int m_mappingIdx = 0;
	};

	struct CompareProfilePtrById {
		struct is_transparent;

		bool operator()(const ProfileInfo * lhs, const ProfileInfo * rhs) const
			{ return uuid(*lhs->id) < uuid(*rhs->id); }
		bool operator()(const ProfileInfo * lhs, const uuid & rhs) const
			{ return uuid(*lhs->id) < rhs; }
		bool operator()(const uuid & lhs, const ProfileInfo * rhs) const
			{ return lhs < uuid(*rhs->id); }
	};

	struct DerefProfilePtr {
		const ProfileInfo & operator()(const ProfileInfo * ptr) const
			{ return *ptr; }
	};

public:
	class Listener {
	public:
		virtual void onModelReload() = 0;
		virtual void onActiveProfileChange() = 0;
	protected:
		~Listener() = default;
	};

public:
	Model();

	void addListener(Listener * listener) 
		{ m_listeners.insert(listener); }
	void removeListener(Listener * listener)
		{ m_listeners.erase(listener); }

	auto enabledProfiles() const 
		{ return m_enabledProfiles | std::views::transform(DerefProfilePtr()); }
	auto disabledProfiles() const 
		{ return m_disabledProfiles | std::views::transform(DerefProfilePtr()); }
	

	const ProfileInfo * getActiveProfile() const
		{ return m_activeProfile; }
	int getActiveProfileIndex() const { 
		if (!m_activeProfile)
			return -1;
		if (auto foundIt = m_enabledProfilesIndices.find(m_activeProfile); foundIt != m_enabledProfilesIndices.end())
			return int(foundIt->second); 
		return -1;
	}
	void setActiveProfile(const ProfileInfo * profile);


	int getActiveProfileMappingIndex() const { 
		if (m_activeProfile)
			return m_activeMappingInfo ? m_activeMappingInfo->mappingIndex() : 0; 
		return -1;
	}
	void setActiveProfileMappingIndex(int idx) { 
		if (m_activeMappingInfo)
			m_activeMappingInfo->setMappingIndex(m_threadMgrActivation.clientId(), idx); 
	}

	sys_string getActiveMappingTableName() const;

	template<std::ranges::input_range R>
	requires(std::is_same_v<std::ranges::range_value_t<R>, const ProfileInfo *>)
	void enableProfiles(const R & range, bool enable) {
		for (auto info: range) {
			comTest(m_inputProfiles->EnableLanguageProfile(g_translitServiceId, 
														   MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
														   *info->id,
														   enable));
		}
		loadData();
	}
private:
	~Model() = default;

	void loadData();
	
	void activateActiveProfile();
	void onProfileChange(REFCLSID clsid, REFGUID guidProfile, bool activated);

private:
	com_shared_ptr<ITfThreadMgr> m_threadMgr;
	com_shared_ptr<ITfSource> m_threadMgrSource;
	com_shared_ptr<ITfInputProcessorProfiles> m_inputProfiles;
	com_shared_ptr<ITfInputProcessorProfileMgr> m_profileMgr;
	com_shared_ptr<ITfCompartmentMgr> m_globalCompartments;

	ThreadMgrActivation m_threadMgrActivation;
	ActiveLanguageProfileNotifyRegistration m_activeLanguageProfileNotifyRegistration;

	std::vector<const ProfileInfo *> m_enabledProfiles;
	std::map<const ProfileInfo *, size_t, CompareProfilePtrById> m_enabledProfilesIndices;
	std::vector<const ProfileInfo *> m_disabledProfiles;
	std::map<const ProfileInfo *, size_t, CompareProfilePtrById> m_disabledProfilesIndices;

	const ProfileInfo * m_activeProfile = nullptr;
	std::optional<ProfileMappingInfo> m_activeMappingInfo;

	std::set<Listener *> m_listeners;
};
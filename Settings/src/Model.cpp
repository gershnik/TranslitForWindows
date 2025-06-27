// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Model.h"

#include <Common/ComObject.h>
#include <Common/Compartment.h>


Model::ThreadMgrActivation::ThreadMgrActivation() {
    auto owner = getOwner();
    comTest(owner->m_threadMgr->Activate(&m_clientId));
}

Model::ThreadMgrActivation::~ThreadMgrActivation() {
    auto owner = getOwner();
    owner->m_threadMgr->Deactivate();
}

class Model::ActiveLanguageProfileNotifySink final : public ComObject<Model::ActiveLanguageProfileNotifySink,
    ITfActiveLanguageProfileNotifySink> {

public:
    ActiveLanguageProfileNotifySink(Model & model):
        m_model(model.get_weak_ptr())
    {}

    STDMETHODIMP OnActivated(__RPC__in REFCLSID clsid, __RPC__in REFGUID guidProfile, BOOL fActivated) override {
        if (auto model = strong_cast(m_model)) {
            model->onProfileChange(clsid, guidProfile, fActivated);
        }
        return S_OK;
    }
private:
    Model::weak_ptr m_model;
};

Model::ActiveLanguageProfileNotifyRegistration::ActiveLanguageProfileNotifyRegistration() {
    auto owner = getOwner();
    auto activeLanguageSink = com_attach(new ActiveLanguageProfileNotifySink(*owner));
    comTest(owner->m_threadMgrSource->AdviseSink(__uuidof(ITfActiveLanguageProfileNotifySink), 
                                                 activeLanguageSink.get(), 
                                                 &m_cookie));
}

Model::ActiveLanguageProfileNotifyRegistration::~ActiveLanguageProfileNotifyRegistration() {
    auto owner = getOwner();
    owner->m_threadMgrSource->UnadviseSink(m_cookie);
}

Model::ProfileMappingInfo::ProfileMappingInfo(const com_shared_ptr<ITfCompartmentMgr> & compMgr, TfClientId clientId, 
                                              const GUID & mappingCompartmentId, int maxIdx):
    m_mappingCompartment(getCompartment(compMgr, mappingCompartmentId)),
    m_compartmentKey(RegKey::currentUser->create(L"Software\\Translit\\Compartments", 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY)),
    m_regValueName(uuid(mappingCompartmentId).to_chars()){

    int value = getInt(m_mappingCompartment);
    if (value == 0) {
        
        AllowedErrors<ERROR_FILE_NOT_FOUND> ec;
        value = int(m_compartmentKey->getDwordValue(m_regValueName.w_str(), ec));
        if (ec || value < 0 || value >= maxIdx)
            value = 0;
        setInt(m_mappingCompartment, clientId, value + 1);
    } else {
        value -= 1;
        if (value < 0 || value >= maxIdx) {
            value = 0;
            setInt(m_mappingCompartment, clientId, value + 1);
        }
    }
    m_compartmentKey->setDwordValue(m_regValueName.w_str(), value);
    m_mappingIdx = value;
}

void Model::ProfileMappingInfo::setMappingIndex(TfClientId clientId, int idx) {
    setInt(m_mappingCompartment, clientId, idx + 1);
    m_compartmentKey->setDwordValue(m_regValueName.w_str(), idx);
    m_mappingIdx = idx;
}


Model::Model() :
    m_threadMgr(comCreate<ITfThreadMgr>(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER)),
    m_threadMgrSource(com_cast<ITfSource>(m_threadMgr)),
    m_inputProfiles(comCreate<ITfInputProcessorProfiles>(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER)),
    m_profileMgr(com_cast<ITfInputProcessorProfileMgr>(m_inputProfiles)) {
    
    comTest(m_threadMgr->GetGlobalCompartment(std::out_ptr(m_globalCompartments)));

    loadData();
}

void Model::loadData() {

    m_enabledProfiles.clear();
    m_enabledProfilesIndices.clear();
    m_disabledProfiles.clear();
    m_disabledProfilesIndices.clear();
    m_activeProfile = nullptr;
    m_activeMappingInfo.reset();

    std::set<uuid> existingProfiles;

    com_shared_ptr<IEnumTfLanguageProfiles> enumerator;
    comTest(m_inputProfiles->EnumLanguageProfiles(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), std::out_ptr(enumerator)));
    for ( ; ; ) {
        TF_LANGUAGEPROFILE profiles[20];
        ULONG count;
        auto res = comTest(enumerator->Next(ULONG(std::size(profiles)), profiles, &count));
        for (ULONG i = 0; i < count; ++i) {
            if (profiles[i].clsid == g_translitServiceId)
                existingProfiles.insert(profiles[i].guidProfile);
        }
        if (res != S_OK)
            break;
    }

    for(auto & profileInfo: getProfiles()) {
        if (!existingProfiles.contains(*profileInfo.id))
            continue;
        BOOL enabled = false;
        comTest(m_inputProfiles->IsEnabledLanguageProfile(g_translitServiceId, 
                                                            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                                            *profileInfo.id,
                                                            &enabled));
        if (enabled) {
            m_enabledProfiles.push_back(&profileInfo);
            m_enabledProfilesIndices.emplace(&profileInfo, m_enabledProfiles.size() - 1);
        } else {
            m_disabledProfiles.push_back(&profileInfo);
            m_disabledProfilesIndices.emplace(&profileInfo, m_disabledProfiles.size() - 1);
        }
    }


    LANGID langId;
    GUID activeProfileId;
    comTest(m_inputProfiles->GetActiveLanguageProfile(g_translitServiceId, &langId, &activeProfileId));
    if (auto activeIt = m_enabledProfilesIndices.find(uuid(activeProfileId)); activeIt != m_enabledProfilesIndices.end()) {
        m_activeProfile = m_enabledProfiles[activeIt->second];
        activateActiveProfile(); //sic! the fact that we see it as active doesn't mean that webview does etc.
    } else if (!m_enabledProfiles.empty()) {
        m_activeProfile = m_enabledProfiles[0];
        activateActiveProfile();
    }
    if (m_activeProfile && m_activeProfile->mappingCompartmentId)
        m_activeMappingInfo.emplace(m_globalCompartments, m_threadMgrActivation.clientId(), 
                                    *m_activeProfile->mappingCompartmentId, int(m_activeProfile->mappings.size()));
    else
        m_activeMappingInfo.reset();

    for (auto listener: m_listeners)
        listener->onModelReload();
}

void Model::setActiveProfile(const ProfileInfo * profile) {
    
    m_activeProfile = profile;
    if (m_activeProfile->mappingCompartmentId)
        m_activeMappingInfo.emplace(m_globalCompartments, m_threadMgrActivation.clientId(), 
                                    *m_activeProfile->mappingCompartmentId, int(m_activeProfile->mappings.size()));
    else
        m_activeMappingInfo.reset();
    
    activateActiveProfile();

    for (auto listener: m_listeners)
        listener->onActiveProfileChange();
}

void Model::activateActiveProfile() {
    comTest(m_inputProfiles->ChangeCurrentLanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)));
    comTest(m_profileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 
                                          MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), 
                                          g_translitServiceId,
                                          *m_activeProfile->id, 
                                          nullptr, 
                                          TF_IPPMF_FORSESSION | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE));
    /*comTest(m_inputProfiles->ActivateLanguageProfile(g_translitServiceId, 
                                                     MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), 
                                                     *m_activeProfile->id));*/
}

void Model::onProfileChange(REFCLSID clsid, REFGUID guidProfile, bool activated) {
    uuid newId;
    if (activated && clsid == g_translitServiceId) {
        newId = guidProfile;
    } else {
        LANGID langId;
        GUID profileId;
        comTest(m_inputProfiles->GetActiveLanguageProfile(g_translitServiceId, &langId, &profileId));
        newId = profileId;
    }
    if (!m_activeProfile || newId != *m_activeProfile->id) {
        if (auto activeIt = m_enabledProfilesIndices.find(uuid(newId)); activeIt != m_enabledProfilesIndices.end()) {
            m_activeProfile = m_enabledProfiles[activeIt->second];
            if (m_activeProfile->mappingCompartmentId)
                m_activeMappingInfo.emplace(m_globalCompartments, m_threadMgrActivation.clientId(), 
                                            *m_activeProfile->mappingCompartmentId, int(m_activeProfile->mappings.size()));
            else
                m_activeMappingInfo.reset();

            for (auto listener: m_listeners)
                listener->onActiveProfileChange();
        } else {
            loadData();
        }
    }
}

sys_string Model::getActiveMappingTableName() const {

    if (!m_activeProfile)
        return {};
    
    sys_string ret = S("mapping-") + m_activeProfile->lang;
    auto mappingIdx = m_activeMappingInfo ? m_activeMappingInfo->mappingIndex() : 0;
    if (mappingIdx != 0) {
        auto mapping = m_activeProfile->mappings[mappingIdx]->name;
        ret = ret + S("_") + mapping;
    }
    return ret;
}



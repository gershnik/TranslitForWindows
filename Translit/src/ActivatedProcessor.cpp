// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Common/Compartment.h>
#include <Common/RegKey.h>

#include "ActivatedProcessor.h"
#include "Translit.h"
#include "EditSession.h"
#include "DisplayAttributes.h"
#include "SettingsButton.h"

auto getMapper(const MappingInfo * info) -> Transliterator::MappingFunc *;


ActivatedProcessor::ThreadMgrEventRegistration::ThreadMgrEventRegistration() {
	auto owner = getOwner();
	comTest(owner->m_threadMgrSource->AdviseSink(__uuidof(ITfThreadMgrEventSink), 
			static_cast<ITfThreadMgrEventSink *>(owner->m_owner), 
			&m_cookie));
}

ActivatedProcessor::ThreadMgrEventRegistration::~ThreadMgrEventRegistration() {
	auto owner = getOwner();
	owner->m_threadMgrSource->UnadviseSink(m_cookie);
}

ActivatedProcessor::ActiveLanguageProfileNotifyRegistration::ActiveLanguageProfileNotifyRegistration() {
	auto owner = getOwner();
	comTest(owner->m_threadMgrSource->AdviseSink(__uuidof(ITfActiveLanguageProfileNotifySink), 
			static_cast<ITfActiveLanguageProfileNotifySink *>(owner->m_owner), 
			&m_cookie));
}

ActivatedProcessor::ActiveLanguageProfileNotifyRegistration::~ActiveLanguageProfileNotifyRegistration() {
	auto owner = getOwner();
	owner->m_threadMgrSource->UnadviseSink(m_cookie);
}

ActivatedProcessor::ThreadFocusRegistration::ThreadFocusRegistration() {
	auto owner = getOwner();
	comTest(owner->m_threadMgrSource->AdviseSink(__uuidof(ITfThreadFocusSink), 
			static_cast<ITfThreadFocusSink *>(owner->m_owner), 
			&m_cookie));
}

ActivatedProcessor::ThreadFocusRegistration::~ThreadFocusRegistration() {
	auto owner = getOwner();
	owner->m_threadMgrSource->UnadviseSink(m_cookie);
}

ActivatedProcessor::KeyEventRegistration::KeyEventRegistration() {
	auto owner = getOwner();
	comTest(owner->m_keystrokeMgr->AdviseKeyEventSink(owner->m_clientId, static_cast<ITfKeyEventSink *>(owner->m_owner), true));
}

ActivatedProcessor::KeyEventRegistration::~KeyEventRegistration() {
	auto owner = getOwner();
	owner->m_keystrokeMgr->UnadviseKeyEventSink(owner->m_clientId);
}


ActivatedProcessor::TextEditRegistration::TextEditRegistration() {
	auto owner = getOwner();

	if (!owner->m_documentMgr) 
		return;
	
	com_shared_ptr<ITfContext> context;
	if (comFailed(owner->m_documentMgr->GetTop(std::out_ptr(context))) || !context) 
		return;

	auto source = com_as<ITfSource>(context);
	if (!source)
		return;
	
	DWORD cookie = TF_INVALID_COOKIE;
	if (comFailed(source->AdviseSink(__uuidof(ITfTextEditSink), static_cast<ITfTextEditSink *>(owner->m_owner), &cookie)))
		return;

	m_source = std::move(source);
	m_cookie = cookie;
}

ActivatedProcessor::TextEditRegistration::~TextEditRegistration() {
	if (m_source)
		m_source->UnadviseSink(m_cookie);
}

ActivatedProcessor::LangBarItemRegistration::LangBarItemRegistration() {
	auto owner = getOwner();

	auto settingsButton = com_attach(static_cast<ITfLangBarItem *>(new SettingsButton()));
	comTest(owner->m_langBarMgr->AddItem(settingsButton.get()));
	m_item = settingsButton;
}

void ActivatedProcessor::LangBarItemRegistration::activate() {
	comTest(m_item->Show(true));
}

ActivatedProcessor::LangBarItemRegistration::~LangBarItemRegistration() {
	auto owner = getOwner();
	owner->m_langBarMgr->RemoveItem(m_item.get());
}

void ActivatedProcessor::CompartmentEventsRegistration::subscribe(const com_shared_ptr<ITfCompartment> & comp,
																  ITfCompartmentEventSink * sink) {
	if (m_cookie != TF_INVALID_COOKIE) {
		m_source->UnadviseSink(m_cookie);
		m_cookie = TF_INVALID_COOKIE;
	}
	m_source = com_cast<ITfSource>(comp);
	comTest(m_source->AdviseSink(__uuidof(ITfCompartmentEventSink), sink, &m_cookie));
}

void ActivatedProcessor::CompartmentEventsRegistration::unsubscribe() {
	if (m_cookie != TF_INVALID_COOKIE) {
		m_source->UnadviseSink(m_cookie);
		m_cookie = TF_INVALID_COOKIE;
	}
}

ActivatedProcessor::CompartmentEventsRegistration::~CompartmentEventsRegistration() {
	if (m_cookie != TF_INVALID_COOKIE)
		m_source->UnadviseSink(m_cookie);
}

ActivatedProcessor::ActivatedProcessor(Translit * owner, com_shared_ptr<ITfThreadMgr> && threadMgr, TfClientId clientId, DWORD /*flags*/):
	m_owner(owner),
	m_threadMgr(std::move(threadMgr)),
	m_clientId(clientId),
	m_documentMgr(getFocus(m_threadMgr)),
    m_threadMgrSource(com_cast<ITfSource>(m_threadMgr)),
    m_keystrokeMgr(com_cast<ITfKeystrokeMgr>(m_threadMgr)),
	m_langBarMgr(com_cast<ITfLangBarItemMgr>(m_threadMgr)) {

	comTest(m_threadMgr->GetGlobalCompartment(std::out_ptr(m_globalCompartments)));

	auto categoryMgr = com_cast<ITfCategoryMgr>(m_threadMgr);
	comTest(categoryMgr->RegisterGUID(__uuidof(DisplayAttributeCompositionInfo), &m_displayAttributeCompositionInfoAtom));

	m_transliterator = std::make_unique<Transliterator>(Transliterator::nullMapper);
}

ActivatedProcessor::~ActivatedProcessor() = default;

void ActivatedProcessor::setProfile(const GUID & profileId) {

	auto * profile = getProfileById(profileId);
	setTransliterator(profile);
	m_langBarItemRegistration.activate();
}

void ActivatedProcessor::onCompartmentChange(REFGUID rguid) {

	if (m_profile && m_profile->mappingCompartmentId && *m_profile->mappingCompartmentId == rguid) {
		setTransliterator(m_profile);
	}
}

void ActivatedProcessor::setTransliterator(const ProfileInfo * profile) {

	Transliterator::MappingFunc * mapper = Transliterator::nullMapper;
	com_shared_ptr<ITfCompartment> mappingCompartment;

	if (profile) {
		int mappingIdx = 0;
		if (profile->mappingCompartmentId) {
			mappingCompartment = getCompartment(m_globalCompartments, *profile->mappingCompartmentId);
			int value = getInt(mappingCompartment);
			if (value == 0) {
				sys_string valueName = uuid(*profile->mappingCompartmentId).to_chars();
				try {
					auto settingsKey = RegKey::currentUser->open(L"Software\\Translit\\Compartments", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY);
					value = int(settingsKey->getDwordValue(valueName.w_str()));
					if (value < 0 || value >= int(profile->mappings.size()))
						value = 0;
				} catch([[maybe_unused]] std::system_error & ex) {
				#ifndef NDEBUG
					debugPrint("Translit:  Unable to read registry compartment value: 0x{:08x}", ex.code().value());
				#endif
				}
				setInt(mappingCompartment, m_clientId, value + 1);
				
			} else {
				value -= 1;
				if (value < 0 || value >= int(profile->mappings.size())) {
					value = 0;
					setInt(mappingCompartment, m_clientId, value + 1);
				}
			}
			mappingIdx = value;
		}
		mapper = getMapper(profile->mappings[mappingIdx]);
	}

	if (m_profile == profile && m_mapper == mapper)
		return;

	if (mappingCompartment)
		m_globalCompartmentEventsRegistration.subscribe(mappingCompartment, static_cast<ITfCompartmentEventSink *>(m_owner));
	else
		m_globalCompartmentEventsRegistration.unsubscribe();

	m_profile = profile;
	m_mapper = mapper;
	m_transliterator = std::make_unique<Transliterator>(mapper);
}

bool ActivatedProcessor::isKeyboardDisabled() {
	if (!m_documentMgr)
		return true;

	com_shared_ptr<ITfContext> topContext;
	if (comFailed(m_documentMgr->GetTop(std::out_ptr(topContext))) || !topContext)
		return true;

	auto compartmentMgr = com_cast<ITfCompartmentMgr>(m_threadMgr);

	return getBool(getCompartment(compartmentMgr, GUID_COMPARTMENT_KEYBOARD_DISABLED)) ||
		   getBool(getCompartment(compartmentMgr, GUID_COMPARTMENT_EMPTYCONTEXT));
}

void ActivatedProcessor::sendCompleted(ITfContext * context, const sys_string & text) {
	doEditSession(context, m_clientId, TF_ES_SYNC | TF_ES_READWRITE, [&](TfEditCookie cookie) {
		com_shared_ptr<ITfRange> range;
		if (m_composition) {
			comTest(m_composition->GetRange(std::out_ptr(range)));
			m_composition->EndComposition(cookie);
			m_composition.reset();
		} else {
			TF_SELECTION tfSelection;
			ULONG fetched = 0;
			comTest(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched));
			if (fetched != 1)
				throwHresult(E_FAIL);
			range = com_attach(tfSelection.range);
		}
		comTest(range->SetText(cookie, 0, text.w_str(), LONG(text.storage_size())));

		com_shared_ptr<ITfRange> selectionRange;
		comTest(range->Clone(std::out_ptr(selectionRange)));
		selectionRange->Collapse(cookie, TF_ANCHOR_END);

		TF_SELECTION selection;
		selection.range = selectionRange.get();
		selection.style.ase = TF_AE_END;
		selection.style.fInterimChar = false;
		comTest(context->SetSelection(cookie, 1, &selection));
		});
	m_transliterator->clearCompleted();
}

void ActivatedProcessor::sendIncomplete(ITfContext * context, const sys_string & text) {
	doEditSession(context, m_clientId, TF_ES_SYNC | TF_ES_READWRITE, [&](TfEditCookie cookie) {
		com_shared_ptr<ITfRange> range;
		if (!m_composition) {
			TF_SELECTION tfSelection;
			ULONG fetched = 0;
			comTest(context->GetSelection(cookie, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched));
			if (fetched != 1)
				throwHresult(E_FAIL);

			range = com_attach(tfSelection.range);
			auto contextComposition = com_cast<ITfContextComposition>(context);
			comTest(contextComposition->StartComposition(cookie, range.get(), static_cast<ITfCompositionSink *>(m_owner), 
				std::out_ptr(m_composition)));
		} else {
			comTest(m_composition->GetRange(std::out_ptr(range)));
		}
		comTest(range->SetText(cookie, 0, text.w_str(), LONG(text.storage_size())));

		// get our the display attribute property
		com_shared_ptr<ITfProperty> displayAttributeProperty;
		if (comSucceeded(context->GetProperty(GUID_PROP_ATTRIBUTE, std::out_ptr(displayAttributeProperty))))
		{
			VARIANT var;
			// set the value over the range
			// the application will use this guid atom to lookup the acutal rendering information
			var.vt = VT_I4; // we're going to set a TfGuidAtom
			var.lVal = m_displayAttributeCompositionInfoAtom; 

			comTest(displayAttributeProperty->SetValue(cookie, range.get(), &var));
		}

		com_shared_ptr<ITfRange> selectionRange;
		comTest(range->Clone(std::out_ptr(selectionRange)));
		selectionRange->Collapse(cookie, TF_ANCHOR_END);

		TF_SELECTION selection;
		selection.range = selectionRange.get();
		selection.style.ase = TF_AE_END;
		selection.style.fInterimChar = false;
		comTest(context->SetSelection(cookie, 1, &selection));
	});
}

void ActivatedProcessor::finishComposition(ITfContext * context) {
	if (m_composition) {
		doEditSession(context, m_clientId, TF_ES_SYNC | TF_ES_READWRITE, [&](TfEditCookie cookie) {

#ifndef NDEBUG
			debugPrint("Translit:  Finishing composition\n");
#endif
			m_composition->EndComposition(cookie);

		});
		m_composition.reset();
	}
	m_transliterator->clear();
	
}

sys_string ActivatedProcessor::virtualKeyCodeToText(UINT vcode) {
	if (vcode != VK_SPACE && (vcode < 0x30u || vcode > 0x5Au) && (vcode < VK_OEM_1 || vcode > 0xF5u))
		return {};
	UINT scanCode = MapVirtualKey(vcode, MAPVK_VK_TO_VSC);

	BYTE keyboardState[256] = {'\0'};
	if (!GetKeyboardState(keyboardState))
		throwLastError();

	if (keyboardState[VK_CONTROL] & BYTE(0x80))
		return {};

	WCHAR buf[32] = {};
	//do not change keyboard state (Windows 10, version 1607 and newer)
	UINT flags = (1 << 2);
	int res = ToUnicode(vcode, scanCode, keyboardState, buf, int(std::size(buf)), flags);

	return sys_string(buf, res < 0 ? -res : res);
}

bool ActivatedProcessor::onKeyDown(bool preview, ITfContext * context, WPARAM wParam, [[maybe_unused]] LPARAM lParam) {
	if (isKeyboardDisabled())
		return false;

	auto vcode = UINT(wParam);

	auto chars = virtualKeyCodeToText(vcode);
#ifndef NDEBUG
	debugPrint("Translit:  Got {} {:x},  Chars: '{}'\n", vcode, lParam, chars);
#endif

	if (chars.empty()) {
		finishComposition(context);
		return false;
	}

	if (preview)
		return true;
	
	m_transliterator->append(chars);

	if (!m_transliterator->matchedSomething()) {
#ifndef NDEBUG
		debugPrint("Translit:    Passing through\n");
#endif
		sendCompleted(context, chars);
		return true;
	}

	//one of the conditions below must be true if we matched something
	assert(!m_transliterator->result().empty());

	if (auto completedSize = m_transliterator->completedSize()) {
		auto text = sys_string(m_transliterator->result().substr(0, completedSize));
#ifndef NDEBUG
		debugPrint("Translit:    Sending completed: '{}'\n", text);
#endif
		sendCompleted(context, text);
	}
	//at this point the only remaining thing in impl is incomplete tail
	if (auto incomplete = m_transliterator->result(); !incomplete.empty()) {
		auto text = sys_string(incomplete);
#ifndef NDEBUG
		debugPrint("Translit:    Sending incomplete: '{}'\n", text);
#endif
		sendIncomplete(context, text);

	}
	return true;
}

void ActivatedProcessor::onCompositionTerminated(TfEditCookie cookie, ITfComposition * composition) {
	if (!m_composition || m_composition != composition)
		return;
#ifndef NDEBUG
	debugPrint("Translit:  Terminated\n");
#endif

	m_transliterator->clear();
	m_composition->EndComposition(cookie);
	m_composition.reset();
}

void ActivatedProcessor::onEndEdit(ITfContext * context, TfEditCookie ecReadOnly, ITfEditRecord * editRecord) {

#ifndef NDEBUG
	debugPrint("Translit:  End edit\n");
#endif

	if (!editRecord)
		throwHresult(E_INVALIDARG);

	BOOL selectionChanged = false;
	comTest(editRecord->GetSelectionStatus(&selectionChanged));
	
	if (!selectionChanged)
	    return;

	if (!m_composition)
		return;
	
	// If the selection is moved to outside of the current composition,
	// we terminate the composition. This TextService supports only one
	// composition in one context object.
	

	if (!context)
		throwHresult(E_INVALIDARG);
	
	TF_SELECTION tfSelection;
	ULONG fetched = 0;
	if (comFailed(context->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1, &tfSelection, &fetched)) || fetched != 1)
		return;

	com_shared_ptr<ITfRange> selectionRange = com_attach(tfSelection.range);
	

	com_shared_ptr<ITfRange> rangeComposition;
	if (comFailed(m_composition->GetRange(std::out_ptr(rangeComposition))))
		return;

	
	if (isRangeCovered(ecReadOnly, selectionRange, rangeComposition))
		return;

	finishComposition(context);
}

//Checks if pRangeTest is entirely contained within pRangeCover.
bool ActivatedProcessor::isRangeCovered(TfEditCookie ec, SmartOrDumb<ITfRange> auto && rangeTest, SmartOrDumb<ITfRange> auto && rangeCover)
{
	LONG result = 0;

	if (comFailed(rangeCover->CompareStart(ec, c_ptr(rangeTest), TF_ANCHOR_START, &result)) 
		|| (result > 0))
	{
		return false;
	}

	if (comFailed(rangeCover->CompareEnd(ec, c_ptr(rangeTest), TF_ANCHOR_END, &result)) 
		|| (result < 0))
	{
		return false;
	}

	return true;
}



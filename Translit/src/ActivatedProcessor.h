// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Translit/Languages.h>
#include <Common/com.h>
#include <Mapper/Transliterator.hpp>

class Translit;

class ActivatedProcessor {

public:
	ActivatedProcessor(Translit * owner, com_shared_ptr<ITfThreadMgr> && threadMgr, TfClientId clientId, DWORD flags);
	~ActivatedProcessor();

	ActivatedProcessor(const ActivatedProcessor &) = delete;
	ActivatedProcessor & operator=(const ActivatedProcessor &) = delete;

	void setProfile(const GUID & profileId);

	const com_shared_ptr<ITfThreadMgr> & threadMgr() const 
		{ return m_threadMgr; }

	void setDocumentMgr(com_shared_ptr<ITfDocumentMgr> && documentMgr) {
		m_documentMgr = std::move(documentMgr);
		m_textEditRegistration.~TextEditRegistration();
		new (&m_textEditRegistration) TextEditRegistration();
	}

	bool onKeyDown(bool preview, ITfContext * context, WPARAM wParam, LPARAM lParam);
	void onCompositionTerminated(TfEditCookie cookie, ITfComposition * composition);
	void onEndEdit(ITfContext * context, TfEditCookie ecReadOnly, ITfEditRecord * editRecord);
	void onCompartmentChange(REFGUID rguid);
private:
	class ThreadMgrEventRegistration {
	public:
		ThreadMgrEventRegistration();
		~ThreadMgrEventRegistration();
	private:
		ActivatedProcessor * getOwner() {
			return (ActivatedProcessor *)(reinterpret_cast<uint8_t *>(this) - offsetof(ActivatedProcessor, m_threadMgrEventRegistration));
		}
	private:
		DWORD m_cookie = TF_INVALID_COOKIE;
	};

	class ActiveLanguageProfileNotifyRegistration {
	public:
		ActiveLanguageProfileNotifyRegistration();
		~ActiveLanguageProfileNotifyRegistration();
	private:
		ActivatedProcessor * getOwner() {
			return (ActivatedProcessor *)(reinterpret_cast<uint8_t *>(this) - offsetof(ActivatedProcessor, m_activeLanguageProfileNotifyRegistration));
		}
	private:
		DWORD m_cookie = TF_INVALID_COOKIE;
	};

	class ThreadFocusRegistration {
	public:
		ThreadFocusRegistration();
		~ThreadFocusRegistration();
	private:
		ActivatedProcessor * getOwner() {
			return (ActivatedProcessor *)(reinterpret_cast<uint8_t *>(this) - offsetof(ActivatedProcessor, m_threadFocusRegistration));
		}
	private:
		DWORD m_cookie = TF_INVALID_COOKIE;
	};

	class KeyEventRegistration {
	public:
		KeyEventRegistration();
		~KeyEventRegistration();
	private:
		ActivatedProcessor * getOwner() {
			return (ActivatedProcessor *)(reinterpret_cast<uint8_t *>(this) - offsetof(ActivatedProcessor, m_keyEventRegistration));
		}
	};

	class TextEditRegistration {
	public:
		TextEditRegistration();
		~TextEditRegistration();
	private:
		ActivatedProcessor * getOwner() {
			return (ActivatedProcessor *)(reinterpret_cast<uint8_t *>(this) - offsetof(ActivatedProcessor, m_textEditRegistration));
		}
	private:
		com_shared_ptr<ITfSource> m_source;
		DWORD m_cookie = TF_INVALID_COOKIE;
	};

	class LangBarItemRegistration {
	public:
		LangBarItemRegistration();
		~LangBarItemRegistration();

		void activate();
	private:
		ActivatedProcessor * getOwner() {
			return (ActivatedProcessor *)(reinterpret_cast<uint8_t *>(this) - offsetof(ActivatedProcessor, m_langBarItemRegistration));
		}
	private:
		com_shared_ptr<ITfLangBarItem> m_item;
	};

	class CompartmentEventsRegistration {
	public:
		void subscribe(const com_shared_ptr<ITfCompartment> & comp, ITfCompartmentEventSink * sink);
		void unsubscribe();
		~CompartmentEventsRegistration();
	private:
		com_shared_ptr<ITfSource> m_source;
		DWORD m_cookie = TF_INVALID_COOKIE;
	};

	static com_shared_ptr<ITfDocumentMgr> getFocus(const com_shared_ptr<ITfThreadMgr> & threadMgr) {
		com_shared_ptr<ITfDocumentMgr> ret;
		comTest(threadMgr->GetFocus(std::out_ptr(ret)));
		return ret;
	}

	void setTransliterator(const ProfileInfo * profile);
	bool isKeyboardDisabled();
	
	void finishComposition(ITfContext * context);
	void sendCompleted(ITfContext * context, const sys_string & text);
	void sendIncomplete(ITfContext * context, const sys_string & text);
	static sys_string virtualKeyCodeToText(UINT vcode);
	static bool isRangeCovered(TfEditCookie ec, SmartOrDumb<ITfRange> auto && rangeTest, SmartOrDumb<ITfRange> auto && rangeCover);

private:
	Translit * m_owner;
	com_shared_ptr<ITfThreadMgr> m_threadMgr;
	TfClientId m_clientId;

	com_shared_ptr<ITfDocumentMgr> m_documentMgr;

	com_shared_ptr<ITfSource> m_threadMgrSource;
	com_shared_ptr<ITfKeystrokeMgr> m_keystrokeMgr;
	com_shared_ptr<ITfLangBarItemMgr> m_langBarMgr;
	com_shared_ptr<ITfCompartmentMgr> m_globalCompartments;

	TfGuidAtom m_displayAttributeCompositionInfoAtom;

	ThreadMgrEventRegistration m_threadMgrEventRegistration;
	[[msvc::no_unique_address]] KeyEventRegistration m_keyEventRegistration;
	ActiveLanguageProfileNotifyRegistration m_activeLanguageProfileNotifyRegistration;
	ThreadFocusRegistration m_threadFocusRegistration;
	TextEditRegistration m_textEditRegistration;
	LangBarItemRegistration m_langBarItemRegistration;
	CompartmentEventsRegistration m_globalCompartmentEventsRegistration;

	const ProfileInfo * m_profile;
	Transliterator::MappingFunc * m_mapper;
	std::unique_ptr<Transliterator> m_transliterator;
	com_shared_ptr<ITfComposition> m_composition;
};
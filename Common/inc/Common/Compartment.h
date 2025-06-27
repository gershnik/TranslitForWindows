// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/com.h>

com_shared_ptr<ITfCompartment> getCompartment(SmartOrDumb<ITfCompartmentMgr> auto && mgr, REFGUID guidCompartment) {
	com_shared_ptr<ITfCompartment> ret;
	comTest(std::forward<decltype(mgr)>(mgr)->GetCompartment(guidCompartment, std::out_ptr(ret)));
	return ret;
}


bool getBool(SmartOrDumb<ITfCompartment> auto && comp) {
	VARIANT var;
	comTest(comp->GetValue(&var));
	bool ret = false;
	if (var.vt == VT_I4) {
		ret = (var.lVal != 0);
	}
	VariantClear(&var);
	return ret;
}

void setBool(SmartOrDumb<ITfCompartment> auto && comp, TfClientId clientId, bool value) {
	VARIANT var;
	VariantInit(&var);
	var.vt = VT_I4;
	var.lVal = value ? 1 : 0;
	comTest(comp->SetValue(clientId, &var));
}

int getInt(SmartOrDumb<ITfCompartment> auto && comp) {
	VARIANT var;
	comTest(comp->GetValue(&var));
	int ret = 0;
	if (var.vt == VT_I4) {
		ret = var.lVal;
	}
	VariantClear(&var);
	return ret;
}

void setInt(SmartOrDumb<ITfCompartment> auto && comp, TfClientId clientId, int value) {
	VARIANT var;
	VariantInit(&var);
	var.vt = VT_I4;
	var.lVal = value;
	comTest(comp->SetValue(clientId, &var));
}

//sys_string_bstr getString(SmartOrDumb<ITfCompartment> auto && comp) {
//	VARIANT var;
//	comTest(comp->GetValue(&var));
//	sys_string_bstr ret;
//	if (var.vt == VT_BSTR) {
//		ret = sys_string_bstr(var.bstrVal, handle_transfer::attach_pointer);
//		var.bstrVal = nullptr;
//	}
//	VariantClear(&var);
//	return ret;
//}
//
//void setString(SmartOrDumb<ITfCompartment> auto && comp, TfClientId clientId, const sys_string_bstr & value) {
//	VARIANT var;
//	VariantInit(&var);
//	var.vt = VT_BSTR;
//	var.bstrVal = value.b_str();
//	comTest(comp->SetValue(clientId, &var));
//}



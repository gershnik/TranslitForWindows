// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Common/err.h>

class Scaling {

public:
	static Scaling getCurrent(HWND hwnd) 
		{ return getCurrentForDpi(hwnd, GetDpiForWindow(hwnd)); }
	
	static Scaling getCurrentForDpi(HWND hwnd, int dpi) {
		Scaling ret;

		ret.m_dpi = dpi; 
		ret.m_metrics.cbSize = sizeof(ret.m_metrics);
		if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ret.m_metrics, 0, dpi))
			throwLastError();
		HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		ret.m_monitor.cbSize = sizeof(ret.m_monitor);
		if (!GetMonitorInfo(mon, &ret.m_monitor))
			throwLastError();

		return ret;
	}

	int dpiScale(int src) const 
		{ return MulDiv(src, this->m_dpi, USER_DEFAULT_SCREEN_DPI); }

	const RECT & monitorWorkRect() const
		{ return m_monitor.rcWork; }

	const RECT & monitorRect() const
		{ return m_monitor.rcMonitor; }
private:
	Scaling() = default;

private:
	int m_dpi;
	MONITORINFO m_monitor;
	NONCLIENTMETRICS m_metrics;

	
};
// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include "base/platform/base_platform_custom_app_icon.h"

namespace base {

inline bool SetCustomAppIcon(const QString &path) {
	return Platform::SetCustomAppIcon(path);
}

inline bool ClearCustomAppIcon() {
	return Platform::ClearCustomAppIcon();
}

} // namespace base
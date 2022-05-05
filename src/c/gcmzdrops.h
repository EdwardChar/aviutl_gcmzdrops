#pragma once
#include "aviutl.h"

#define GCMZDROPS_NAME_MBCS "\xE9\x9A\x8F\xE6\x84\x8F\xE6\x8B\x96\xE6\x94\xBE"
#define GCMZDROPS_NAME_WIDE L"随意拖放"

#ifdef TEST_IMAGE_DIR
#  define VERSION "vX.X.X ( testing )"
#  define VERSION_WIDE L"vX.X.X ( testing )"
#else
#  include "version.h"
#endif
#define GCMZDROPS_NAME_VERSION_MBCS (GCMZDROPS_NAME_MBCS " " VERSION)
#define GCMZDROPS_NAME_VERSION_WIDE (GCMZDROPS_NAME_WIDE L" " VERSION_WIDE)

extern FILTER_DLL g_gcmzdrops_filter_dll;

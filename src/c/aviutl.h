#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "3rd/aviutl_sdk/filter.h"

#include "ovbase.h"

void aviutl_set_pointers(FILTER const *const fp, void *const editp);
NODISCARD error aviutl_init(void);
NODISCARD bool aviutl_initalized(void);
NODISCARD error aviutl_exit(void);
NODISCARD error aviutl_exedit_is_enpatched(bool *const enpatched);
NODISCARD error aviutl_get_exedit_window(HWND *const h);
NODISCARD HWND aviutl_get_exedit_window_must(void);
NODISCARD error aviutl_get_my_window(HWND *const h);
NODISCARD HWND aviutl_get_my_window_must(void);

NODISCARD error aviutl_get_sys_info(SYS_INFO *const si);
NODISCARD error aviutl_get_editing_file_info(FILE_INFO *const fi);
NODISCARD error aviutl_get_file_info(struct wstr const *const path, FILE_INFO *const fi, int *const samples);
NODISCARD error aviutl_get_project_path(struct wstr *const dest);
NODISCARD error aviutl_get_frame(int *const f);
NODISCARD error aviutl_set_frame(int *const f);
NODISCARD error aviutl_get_frame_n(int *const n);
NODISCARD error aviutl_set_frame_n(int *const n);
NODISCARD error aviutl_get_select_frame(int *const start, int *const end);
NODISCARD error aviutl_set_select_frame(int const start, int const end);
NODISCARD error aviutl_ini_load_int(struct str const *const key, int const defvalue, int *const dest);
NODISCARD error aviutl_ini_load_str(struct str const *const key,
                                    struct str const *const defvalue,
                                    struct str *const dest);
NODISCARD error aviutl_ini_save_int(struct str const *const key, int const value);
NODISCARD error aviutl_ini_save_str(struct str const *const key, struct str const *const value);

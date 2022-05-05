#include "gcmzdrops.h"

#include <commctrl.h>
#include <ole2.h>
#include <shellapi.h>

#include "ovbase.h"
#include "ovnum.h"
#include "ovutil/str.h"
#include "ovutil/win32.h"

#include "api.h"
#include "droptarget.h"
#include "error_gcmz.h"
#include "gcmzfuncs.h"
#include "gui.h"
#include "lua.h"
#include "scpopup.h"
#include "task.h"

enum {
  WM_DROP_TARGET_EVENT = WM_APP + 1,
  DROP_TARGET_ENTER = 0,
  DROP_TARGET_OVER = 1,
  DROP_TARGET_LEAVE = 2,
  DROP_TARGET_DROP = 3,
};

#define ERRMSG_UNSUPPORTED_CHAR_IN_FILENAME                                                                            \
  L"文件名中包含非法字符。\r\n"                                         \
  L"AviUtl不能将emoji等部分文字用作文件名。"

static bool g_drop_target_registered = false;

static struct lua g_lua = {0};
static struct api *g_api = NULL;
static struct scpopup g_scpopup = {0};

static void update_mapped_data_task(void *const userdata) {
  (void)userdata;
  if (!api_initialized(g_api)) {
    return;
  }
  ereportmsg(api_update_mapped_data(g_api), &native_unmanaged(NSTR("外部API数据更新失败。")));
}

static void update_mapped_data(void) {
  // Since the various events are called before the state change,
  // it is too early to get the information.
  task_add(update_mapped_data_task, NULL);
}

struct error_message_box_task_data {
  HWND window;
  struct wstr msg;
  wchar_t const *title;
};

static void error_message_box_task(void *userdata) {
  struct error_message_box_task_data *d = userdata;
  message_box(d->window, d->msg.ptr, d->title, MB_ICONERROR);
  ereport(sfree(&d->msg));
  ereport(mem_free(&d));
}

NODISCARD static error build_error_message(error e, wchar_t const *const main_message, struct wstr *const dest) {
  struct wstr tmp = {0};
  struct wstr msg = {0};
  error err = eok();
  if (e == NULL) {
    err = scpy(dest, main_message);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    goto cleanup;
  }
  bool const nodetail = eis(e, err_type_gcmz, err_gcmz_lua);
  err = nodetail ? error_to_string_short(e, &tmp) : error_to_string(e, &tmp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpym(&msg, main_message, L"\r\n\r\n", tmp.ptr, msg.ptr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  ereport(sfree(&msg));
  ereport(sfree(&tmp));
  return err;
}

static void
error_message_box(error e, HWND const window, wchar_t const *const msg, wchar_t const *const title, bool const later) {
  struct wstr errmsg = {0};
  error err = build_error_message(e, msg, &errmsg);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  struct error_message_box_task_data *d = NULL;
  err = mem(&d, 1, sizeof(struct error_message_box_task_data));
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  d->window = window;
  d->msg = errmsg;
  d->title = title;
  errmsg = (struct wstr){0};

  if (later) {
    // On Wine, touching GUI elements directly from the drag drop handlers seems to cause a freeze.
    // Delay the process to avoid this problem.
    task_add(error_message_box_task, d);
  } else {
    error_message_box_task(d);
  }

cleanup:
  ereport(sfree(&errmsg));
  if (efailed(err)) {
    ereportmsg(err, &native_unmanaged(NSTR("错误对话框显示失败。")));
  }
  efree(&e);
}

static void generic_lua_error_handler(error e, wchar_t const *const default_msg) {
  wchar_t const *msg = NULL;
  bool aborted = false;
  if (eis(e, err_type_gcmz, err_gcmz_invalid_char)) {
    msg = ERRMSG_UNSUPPORTED_CHAR_IN_FILENAME;
    efree(&e);
  } else if (eisg(e, err_abort)) {
    // Working as expected so no need report details.
    aborted = true;
    efree(&e);
  } else {
    msg = default_msg;
  }
  if (!aborted) {
    error_message_box(e, aviutl_get_exedit_window_must(), msg, GCMZDROPS_NAME_VERSION_WIDE, true);
  }
  files_cleanup(true);
}

NODISCARD static error
find_token(struct wstr const *const input, wchar_t const *const key, int *const pos, size_t *const len) {
  ptrdiff_t p = 0;
  error err = sstr(input, key, &p);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (p == -1) {
    *pos = -1;
    *len = 0;
    goto cleanup;
  }
  p += wcslen(key);
  size_t l = 0;
  wchar_t const *cur = input->ptr + p, *end = input->ptr + input->len;
  while (cur < end) {
    wchar_t const c = *cur;
    if (c == L'\r' || c == L'\n') {
      break;
    }
    ++l;
    ++cur;
  }
  *pos = p;
  *len = l;

cleanup:
  return err;
}

static BOOL filter_project_load(FILTER *const fp, void *const editp, void *const data, int const size) {
  error err = eok();
  struct wstr tmp = {0};
  struct wstr tmp2 = {0};
  aviutl_set_pointers(fp, editp);

  if (size == 0) {
    ereport(gui_set_save_mode_to_default());
    ereport(gui_set_save_dir_to_default());
    goto cleanup;
  }
  err = from_utf8(&(struct str){.ptr = data, .len = (size_t)size}, &tmp);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  ptrdiff_t pos = -1;
  size_t len = 0;
  err = find_token(&tmp, L"mode=", &pos, &len);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (pos != -1 && len > 0) {
    uint64_t v = 0;
    if (!ov_atou(tmp.ptr + pos, &v, false)) {
      ereport(gui_set_save_mode_to_default());
    } else {
      ereport(gui_set_save_mode((int)v));
    }
  }
  err = find_token(&tmp, L"savepath=", &pos, &len);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (pos != -1 && len > 0) {
    err = sncpy(&tmp2, tmp.ptr + pos, len);
    if (efailed(err)) {
      efree(&err);
      ereport(gui_set_save_dir_to_default());
    } else {
      ereport(gui_set_save_dir(tmp2.ptr));
    }
  }

cleanup:
  ereport(sfree(&tmp2));
  ereport(sfree(&tmp));
  aviutl_set_pointers(NULL, NULL);
  if (efailed(err)) {
    ereportmsg(err, &native_unmanaged(NSTR("读取数据时出错。")));
    return FALSE;
  }
  return TRUE;
}

static BOOL filter_project_save(FILTER *const fp, void *const editp, void *const data, int *const size) {
  struct str u8buf = {0};
  struct wstr tmp = {0};
  wchar_t modebuf[32];
  struct wstr savepathstr = {0};
  aviutl_set_pointers(fp, editp);

  int mode = 0;
  error err = gui_get_save_mode(&mode);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  wchar_t *modestr = ov_utoa((uint64_t)mode, modebuf);
  err = gui_get_save_dir(&savepathstr);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = scpym(&tmp, L"mode=", modestr, L"\r\nsavepath=", savepathstr.ptr, L"\r\n");
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = to_utf8(&tmp, &u8buf);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  if (size) {
    *size = (int)u8buf.len;
  }
  if (data) {
    memcpy(data, u8buf.ptr, u8buf.len);
  }
  update_mapped_data();

cleanup:
  ereport(sfree(&savepathstr));
  ereport(sfree(&tmp));
  ereport(sfree(&u8buf));
  aviutl_set_pointers(NULL, NULL);
  if (efailed(err)) {
    ereportmsg(err, &native_unmanaged(NSTR("保存数据时出错。")));
    return FALSE;
  }
  return TRUE;
}

static void wndproc_drag_enter(struct files *const f, struct drag_drop_info *const ddi) {
  wchar_t const *msg = NULL;
  error err = lua_init(&g_lua);
  if (efailed(err)) {
    msg = L"Lua脚本初始化出错。";
    err = ethru(err);
    goto failed;
  }

  err = lua_call_on_drag_enter(&g_lua, f, ddi->point, ddi->key_state);
  if (efailed(err)) {
    msg = eisg(err, err_abort) ? L"_entrypoint.ondragenter被中断。"
                               : L"_entrypoint.ondragenter呼叫失败。";
    err = ethru(err);
    goto failed;
  }
  ddi->effect = DROPEFFECT_COPY;
  return;

failed:
  ddi->effect = DROPEFFECT_NONE;
  ereport(lua_exit(&g_lua));
  generic_lua_error_handler(err, msg);
}

static void wndproc_drag_over(struct files *const f, struct drag_drop_info *const ddi) {
  if (!g_lua.L) {
    ddi->effect = DROPEFFECT_NONE;
    return;
  }
  error err = lua_call_on_drag_over(&g_lua, f, ddi->point, ddi->key_state);
  if (efailed(err)) {
    err = ethru(err);
    goto failed;
  }
  ddi->effect = DROPEFFECT_COPY;
  return;

failed:
  ddi->effect = DROPEFFECT_NONE;
  ereport(lua_exit(&g_lua));
  generic_lua_error_handler(err,
                            eisg(err, err_abort) ? L"_entrypoint.ondragover被中断。"
                                                 : L"_entrypoint.ondragover呼叫失败。");
}

static void wndproc_drag_leave(void) {
  if (!g_lua.L) {
    return;
  }
  error err = lua_call_on_drag_leave(&g_lua);
  if (efailed(err)) {
    err = ethru(err);
    goto failed;
  }
  ereport(lua_exit(&g_lua));
  return;

failed:
  ereport(lua_exit(&g_lua));
  generic_lua_error_handler(err, L"_entrypoint.ondragleave呼叫失败。");
}

static void wndproc_drop(struct files *const f, struct drag_drop_info *const ddi) {
  if (!g_lua.L) {
    ddi->effect = DROPEFFECT_NONE;
    return;
  }
  error err = lua_call_on_drop(&g_lua, f, ddi->point, ddi->key_state, 0);
  if (efailed(err)) {
    err = ethru(err);
    goto failed;
  }
  ddi->effect = DROPEFFECT_COPY;
  ereport(lua_exit(&g_lua));
  return;

failed:
  ddi->effect = DROPEFFECT_NONE;
  ereport(lua_exit(&g_lua));
  generic_lua_error_handler(err,
                            eisg(err, err_abort) ? L"_entrypoint.ondrop被中断。"
                                                 : L"_entrypoint.ondrop呼叫失败。");
}

struct drag_drop_handler_data {
  struct files *f;
  struct drag_drop_info *ddi;
};

static void drag_enter_callback(struct files *const f, struct drag_drop_info *const ddi, void *const userdata) {
  SendMessageW(
      (HWND)userdata, WM_DROP_TARGET_EVENT, DROP_TARGET_ENTER, (LPARAM) & (struct drag_drop_handler_data){f, ddi});
}

static void drag_over_callback(struct files *const f, struct drag_drop_info *const ddi, void *const userdata) {
  SendMessageW(
      (HWND)userdata, WM_DROP_TARGET_EVENT, DROP_TARGET_OVER, (LPARAM) & (struct drag_drop_handler_data){f, ddi});
}

static void drag_leave_callback(void *const userdata) {
  SendMessageW((HWND)userdata, WM_DROP_TARGET_EVENT, DROP_TARGET_LEAVE, (LPARAM) & (struct drag_drop_handler_data){0});
}

struct drop_task_data {
  HWND window;
  struct drag_drop_info ddi;
  struct files f;
};

static void drop_task(void *const userdata) {
  struct drop_task_data *d = userdata;
  SendMessageW((HWND)d->window,
               WM_DROP_TARGET_EVENT,
               DROP_TARGET_DROP,
               (LPARAM) & (struct drag_drop_handler_data){&d->f, &d->ddi});
  bool succeeded = d->ddi.effect == DROPEFFECT_COPY;
  ereportmsg(files_free(&d->f, succeeded), &native_unmanaged(NSTR("无法清理使用完的文件。。")));
  files_cleanup(!succeeded);
  ereport(mem_free(&d));
}

static void drop_callback(struct files *const f, struct drag_drop_info *const ddi, void *const userdata) {
  // On Wine, touching GUI elements directly from the drag drop handlers seems to cause freeze.
  // Delay the process to avoid this problem.
  // Other than drop, it needs to return the correct drop effect, so this method cannot be used.
  struct drop_task_data *d = NULL;
  error err = mem(&d, 1, sizeof(struct drop_task_data));
  if (efailed(err)) {
    err = ethru(err);
    ddi->effect = DROPEFFECT_NONE;
    ereportmsg(err, &native_unmanaged(NSTR("任务所用数据内存分配失败。")));
    return;
  }

  d->window = (HWND)userdata;
  d->ddi = *ddi;
  d->f = *f;
  task_add(drop_task, d);
  ddi->effect = DROPEFFECT_COPY;
}

static LRESULT popup_callback(struct scpopup *const p, POINT pt) {
  struct lua lua = {0};
  struct scpopup_menu m = {0};
  error err = lua_init(&lua);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = lua_dropper_init(&lua);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = lua_dropper_build_menu(&lua, &m);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!ClientToScreen(p->window, &pt)) {
    err = errg(err_fail);
    goto cleanup;
  }
  UINT_PTR selected = 0;
  err = scpopup_show_popup(p->window, pt, &m, &selected);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  if (!selected) {
    err = errg(err_abort);
    goto cleanup;
  }
  err = lua_dropper_select(&lua, p->window, pt, selected);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  ereport(lua_exit(&lua));
  ereport(scpopup_menu_free(&m));
  files_cleanup(efailed(err));
  if (efailed(err)) {
    generic_lua_error_handler(err, L"扩展弹出菜单显示失败。");
  }
  return 0;
}

NODISCARD static error get_exedit_scrollbars(HWND const exedit_window, HWND *const sb_horz, HWND *const sb_vert) {
  if (!sb_horz || !sb_vert) {
    return errg(err_invalid_arugment);
  }

  HWND h = NULL, hh = NULL, vv = NULL;
  enum {
    ID_SCROLLBAR = 1004,
  };
  while (!hh || !vv) {
    h = FindWindowExW(exedit_window, h, L"ScrollBar", NULL);
    if (!h) {
      return emsg(err_type_generic,
                  err_fail,
                  &native_unmanaged(NSTR("无法检测扩展编辑窗口滚动条。")));
    }
    if (GetWindowLongPtrW(h, GWL_ID) != ID_SCROLLBAR) {
      continue;
    }
    if ((GetWindowLongPtrW(h, GWL_STYLE) & SBS_VERT) == SBS_VERT) {
      vv = h;
    } else {
      hh = h;
    }
  }
  *sb_horz = hh;
  *sb_vert = vv;
  return eok();
}

NODISCARD static error scroll_to_edit_cursor(HWND const exedit_window, HWND const sb_horz) {
  if (!exedit_window || !sb_horz) {
    return errg(err_invalid_arugment);
  }

  int frame = 0;
  error err = aviutl_get_frame(&frame);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  SCROLLINFO si = {
      .cbSize = sizeof(SCROLLINFO),
      .fMask = SIF_PAGE,
  };
  if (!GetScrollInfo(sb_horz, SB_CTL, &si)) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

  si.fMask = SIF_POS;
  si.nPos = frame - (int)si.nPage / 2;
  int const pos = SetScrollInfo(sb_horz, SB_CTL, &si, TRUE);
  SendMessageW(exedit_window, WM_HSCROLL, (WPARAM)MAKELONG(SB_THUMBTRACK, pos), (LPARAM)sb_horz);
  SendMessageW(exedit_window, WM_HSCROLL, (WPARAM)MAKELONG(SB_THUMBPOSITION, pos), (LPARAM)sb_horz);

cleanup:
  return err;
}

NODISCARD static error process_api(struct api_request_params const *const params) {
  struct lua lua = {0};
  HWND exedit_window = NULL, sb_horz = NULL, sb_vert = NULL;
  int original_zoom_level = -1;
  struct gcmz_analysed_info ai = {0};
  error err = aviutl_get_exedit_window(&exedit_window);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  err = get_exedit_scrollbars(exedit_window, &sb_horz, &sb_vert);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  err = gcmz_analyse_exedit_window(&ai);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  original_zoom_level = ai.zoom_level;

  enum {
    min_zoom_level = 19,
  };
  if (ai.zoom_level < min_zoom_level || (ai.edit_cursor.x == -1 && ai.edit_cursor.y == -1)) {
    if (ai.zoom_level < min_zoom_level) {
      // If the zoom level is below min_zoom_level, the file cannot be inserted at the correct position.
      // Temporarily change the zoom level to avoid the problem.
      err = gcmz_set_zoom_level(min_zoom_level);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
    }
    if (ai.edit_cursor.x == -1 && ai.edit_cursor.y == -1) {
      // edit cursor is not found in the current window.
      // It might be a problem with the scroll position, so will try adjusting the position.
      err = scroll_to_edit_cursor(exedit_window, sb_horz);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
    }
    // re-analyse
    err = gcmz_analyse_exedit_window(&ai);
    if (efailed(err)) {
      err = ethru(err);
      goto cleanup;
    }
    if (ai.edit_cursor.x == -1 && ai.edit_cursor.y == -1) {
      err = emsg(err_type_generic, err_fail, &native_unmanaged(NSTR("无法检测当前光标位置。")));
      goto cleanup;
    }
  }

  if (!ClientToScreen(exedit_window, &ai.edit_cursor)) {
    err = errg(err_fail);
    goto cleanup;
  }

  POINTL pt = {
      .x = ai.edit_cursor.x,
      .y = ai.edit_cursor.y,
  };
  SCROLLINFO si = {
      .cbSize = sizeof(SCROLLINFO),
      .fMask = SIF_POS,
  };
  if (!GetScrollInfo(sb_vert, SB_CTL, &si)) {
    err = errhr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  if (params->layer < 0) {
    // relative
    pt.y += ai.layer_height * -(params->layer + 1);
  } else {
    // absolute
    int pos = si.nPos;
    if (si.nPos > params->layer - 1) {
      si.nPos = params->layer - 1;
      pos = SetScrollInfo(sb_vert, SB_CTL, &si, TRUE);
      SendMessageW(exedit_window, WM_VSCROLL, (WPARAM)MAKELONG(SB_THUMBTRACK, pos), (LPARAM)sb_vert);
      SendMessageW(exedit_window, WM_VSCROLL, (WPARAM)MAKELONG(SB_THUMBPOSITION, pos), (LPARAM)sb_vert);
    }
    pt.y += (params->layer - 1 - pos) * ai.layer_height;
  }

  err = lua_init(&lua);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }
  err = lua_call_on_drop_simulated(&lua, &params->files, pt, 0, params->frame_advance);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  ereport(lua_exit(&lua));
  if (ai.zoom_level != original_zoom_level) {
    ereport(gcmz_set_zoom_level(original_zoom_level));
  }
  return err;
}

NODISCARD static error find_extended_filter_window(HWND *const window) {
  error err = eok();
  struct wstr tmp = {0};
  DWORD const pid = GetCurrentProcessId();
  HWND h = NULL;
  for (;;) {
    h = FindWindowExA(NULL, h, "ExtendedFilterClass", NULL);
    if (h == 0) {
      *window = NULL;
      break;
    }

    DWORD p = 0;
    GetWindowThreadProcessId(h, &p);
    if (p == pid) {
      err = get_window_text(h, &tmp);
      if (efailed(err)) {
        err = ethru(err);
        goto cleanup;
      }
      if (wcscmp(tmp.ptr, L"ExtendedFilter") == 0) {
        // target window is not initialized yet
        err = errg(err_fail);
        goto cleanup;
      } else {
        *window = h;
        break;
      }
    }
  }

cleanup:
  ereport(sfree(&tmp));
  return err;
}

NODISCARD static error adjust_window_visibility(struct api_request_params const *const params) {
  HWND exedit_window = NULL;
  HWND extended_filter_window = NULL;
  bool visible = false;
  BYTE old_alpha = 0;

  error err = aviutl_get_exedit_window(&exedit_window);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  visible = IsWindowVisible(exedit_window);
  if (!visible) {
    // The process we are about to perform requires that the window be visible on the screen.
    // When using External API, it is preferable to succeed than to fail due to hiding.
    // So we work around the problem by showing an invisible window.
    GetLayeredWindowAttributes(exedit_window, NULL, &old_alpha, NULL);
    SetWindowLongPtrW(exedit_window, GWL_EXSTYLE, GetWindowLongPtrW(exedit_window, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    SetLayeredWindowAttributes(exedit_window, 0, 0, LWA_ALPHA);
    err = find_extended_filter_window(&extended_filter_window);
    if (esucceeded(err)) {
      SetWindowLongPtrW(extended_filter_window,
                        GWL_EXSTYLE,
                        GetWindowLongPtrW(extended_filter_window, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED);
      SetLayeredWindowAttributes(extended_filter_window, 0, 0, LWA_ALPHA);
    } else {
      efree(&err);
    }
    SetWindowPos(exedit_window,
                 0,
                 0,
                 0,
                 0,
                 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_NOREDRAW |
                     SWP_SHOWWINDOW);
  }

  err = process_api(params);

cleanup:
  if (exedit_window && !visible) {
    ShowWindow(exedit_window, SW_HIDE);
    SetLayeredWindowAttributes(exedit_window, 0, old_alpha, LWA_ALPHA);
    SetWindowLongPtrW(exedit_window, GWL_EXSTYLE, GetWindowLongPtrW(exedit_window, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
    if (extended_filter_window) {
      ShowWindow(extended_filter_window, SW_HIDE);
      SetWindowLongPtrW(extended_filter_window,
                        GWL_EXSTYLE,
                        GetWindowLongPtrW(extended_filter_window, GWL_EXSTYLE) & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED));
    }
  }

  return err;
}

static void request_callback(struct api_request_params *const params, api_request_complete_func const complete) {
  error err = params->err;
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

  err = adjust_window_visibility(params);
  if (efailed(err)) {
    err = ethru(err);
    goto cleanup;
  }

cleanup:
  files_cleanup(efailed(err));
  if (efailed(err)) {
    error_message_box(err,
                      (HWND)params->userdata,
                      L"处理外部API时出错。",
                      GCMZDROPS_NAME_VERSION_WIDE,
                      false);
  }
  complete(params);
  return;
}

#define ERRMSG_INIT GCMZDROPS_NAME_WIDE L"初始化出错。"

static BOOL wndproc_init(HWND const window) {
  task_init(window);
  ereportmsg(gui_init(window), &native_unmanaged(NSTR("GUI初始化出错。")));

  error err = aviutl_init();
  if (efailed(err)) {
    wchar_t const *msg = NULL;
    if (eis(err, err_type_gcmz, err_gcmz_unsupported_aviutl_version)) {
      msg = ERRMSG_INIT L"\r\n\r\n" GCMZDROPS_NAME_WIDE L"仅支持1.00版本以上的AviUtl。";
      efree(&err);
    } else if (eis(err, err_type_gcmz, err_gcmz_exedit_not_found)) {
      msg = ERRMSG_INIT L"\r\n\r\n"
                        L"找不到扩展编辑插件。";
      efree(&err);
    } else if (eis(err, err_type_gcmz, err_gcmz_exedit_not_found_in_same_dir)) {
      msg = ERRMSG_INIT L"\r\n\r\n"
                        L"安装状态不正确。\r\n"
                        L"按照附属文档安装。";
      efree(&err);
    } else if (eis(err, err_type_gcmz, err_gcmz_unsupported_exedit_version)) {
      msg = ERRMSG_INIT L"\r\n\r\n" GCMZDROPS_NAME_WIDE L"仅支持0.92版本以上的扩展编辑。";
      efree(&err);
    } else if (eis(err, err_type_gcmz, err_gcmz_extext_found)) {
      msg = ERRMSG_INIT L"\r\n\r\n" GCMZDROPS_NAME_WIDE L"与字幕辅助插件(extext.auf)无法共存。";
      efree(&err);
    } else if (eis(err, err_type_gcmz, err_gcmz_oledd_found)) {
      msg = ERRMSG_INIT L"\r\n\r\n" GCMZDROPS_NAME_WIDE L"与旧版oledd.auf无法共存。";
      efree(&err);
    } else {
      msg = ERRMSG_INIT;
    }
    gui_lock();
    error_message_box(err, window, msg, GCMZDROPS_NAME_VERSION_WIDE, false);
    return FALSE;
  }

  struct drop_target *dt = NULL;
  err = drop_target_new(&dt);
  if (efailed(err)) {
    gui_lock();
    error_message_box(err,
                      window,
                      ERRMSG_INIT L"\r\n\r\n"
                                  L"未能创建拖放句柄。",
                      GCMZDROPS_NAME_VERSION_WIDE,
                      false);
    return FALSE;
  }
  dt->super.lpVtbl->AddRef((void *)dt);

  dt->userdata = (void *)aviutl_get_my_window_must();
  dt->drag_enter = drag_enter_callback;
  dt->drag_over = drag_over_callback;
  dt->drag_leave = drag_leave_callback;
  dt->drop = drop_callback;

  HWND const exedit_window = aviutl_get_exedit_window_must();
  DragAcceptFiles(exedit_window, FALSE); // disable default drop handler
  HRESULT hr = RegisterDragDrop(exedit_window, (LPDROPTARGET)dt);
  dt->super.lpVtbl->Release((void *)dt);
  if (FAILED(hr)) {
    err = errhr(hr);
    wchar_t const *msg = NULL;
    if (eis_hr(err, DRAGDROP_E_ALREADYREGISTERED)) {
      msg = ERRMSG_INIT L"\r\n\r\n"
                        L"拖放句柄注册失败。\r\n"
                        L"可能与其他拖放相关插件出现冲突。\r\n"
                        L"请检查已安装的插件。";
      efree(&err);
    } else {
      msg = ERRMSG_INIT;
    }
    gui_lock();
    error_message_box(err, window, msg, GCMZDROPS_NAME_VERSION_WIDE, false);
    return FALSE;
  }
  g_drop_target_registered = SUCCEEDED(hr);

  err = scpopup_init(&g_scpopup, exedit_window);
  if (efailed(err)) {
#define ERRMSG_INITSCDROPPER                                                                                           \
  L"右键选单句柄注册失败。\r\n"                                \
  L"扩展弹出菜单不可用。"
    error_message_box(err, window, ERRMSG_INITSCDROPPER, GCMZDROPS_NAME_VERSION_WIDE, false);
#undef ERRMSG_INITSCDROPPER
  } else {
    g_scpopup.popup = popup_callback;
  }

  err = api_init(&g_api);
  if (efailed(err)) {
#define ERRMSG_INITAPI                                                                                                 \
  L"外部API初始化出错。\r\n"                                       \
  L"无法使用外部API。"

    wchar_t const *msg = NULL;
    if (eis_hr(err, HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))) {
      msg = ERRMSG_INITAPI L"\r\n\r\n"
                           L"此错误主要发生在同时开启多个AviUtl的情况下。\r\n"
                           L"关闭所有AviUtl，仅启动一个。";
      efree(&err);
    } else {
      msg = ERRMSG_INITAPI;
    }
#undef ERRMSG_INITAPI
    error_message_box(err, window, msg, GCMZDROPS_NAME_VERSION_WIDE, false);
  } else {
    api_set_callback(g_api, request_callback, (void *)aviutl_get_my_window_must());
  }

  ereport(gui_set_save_mode_to_default());
  ereport(gui_set_save_dir_to_default());
  update_mapped_data();
  return FALSE;
}

static BOOL wndproc_exit(void) {
  ereport(lua_exit(&g_lua));

  if (api_initialized(g_api)) {
    ereportmsg(api_exit(&g_api), &native_unmanaged(NSTR("无法终止外部API。")));
  }

  if (g_scpopup.window) {
    ereportmsg(scpopup_exit(&g_scpopup),
               &native_unmanaged(NSTR("右键选单句柄注销失败。")));
  }

  if (g_drop_target_registered) {
    HRESULT hr = RevokeDragDrop(aviutl_get_exedit_window_must());
    if (FAILED(hr)) {
      ereportmsg(errhr(hr), &native_unmanaged(NSTR("拖放句柄注销失败。")));
    }
  }
  ereport(aviutl_exit());
  gui_exit();
  return FALSE;
}

static BOOL wndproc_file_open(void) {
  ereport(gui_set_save_mode_to_default());
  ereport(gui_set_save_dir_to_default());
  update_mapped_data();
  return FALSE;
}

static BOOL wndproc_file_close(void) {
  ereport(gui_set_save_mode_to_default());
  ereport(gui_set_save_dir_to_default());
  update_mapped_data();
  return FALSE;
}

static BOOL wndproc(HWND const window,
                    UINT const message,
                    WPARAM const wparam,
                    LPARAM const lparam,
                    void *const editp,
                    FILTER *const fp) {
  aviutl_set_pointers(fp, editp);

  BOOL r = FALSE;
  switch (message) {
  case WM_FILTER_INIT:
    r = wndproc_init(window);
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_EXIT:
    r = wndproc_exit();
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_FILE_OPEN:
    r = wndproc_file_open();
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_FILTER_FILE_CLOSE:
    r = wndproc_file_close();
    aviutl_set_pointers(NULL, NULL);
    break;
  case WM_DROP_TARGET_EVENT:
    if (lparam) {
      struct drag_drop_handler_data *const d = (void *)lparam;
      switch (wparam) {
      case DROP_TARGET_ENTER:
        wndproc_drag_enter(d->f, d->ddi);
        break;
      case DROP_TARGET_OVER:
        wndproc_drag_over(d->f, d->ddi);
        break;
      case DROP_TARGET_LEAVE:
        wndproc_drag_leave();
        break;
      case DROP_TARGET_DROP:
        wndproc_drop(d->f, d->ddi);
        break;
      }
      aviutl_set_pointers(NULL, NULL);
    }
    break;
  case WM_COMMAND:
    gui_handle_wm_command(window, wparam, lparam);
    aviutl_set_pointers(NULL, NULL);
    break;
  default:
    if (task_process(window, message, wparam, lparam)) {
      break;
    }
  }
  return r;
}

FILTER_DLL g_gcmzdrops_filter_dll = {
    .flag = FILTER_FLAG_ALWAYS_ACTIVE | FILTER_FLAG_EX_INFORMATION | FILTER_FLAG_DISP_FILTER,
    .name = GCMZDROPS_NAME_MBCS,
    .func_WndProc = wndproc,
    .information = GCMZDROPS_NAME_VERSION_MBCS,
    .func_project_load = filter_project_load,
    .func_project_save = filter_project_save,
};

FILTER_DLL __declspec(dllexport) * *APIENTRY GetFilterTableList(void);
FILTER_DLL __declspec(dllexport) * *APIENTRY GetFilterTableList(void) {
  static FILTER_DLL *filter_list[] = {&g_gcmzdrops_filter_dll, NULL};
  return (FILTER_DLL **)&filter_list;
}

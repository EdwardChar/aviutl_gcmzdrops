#include "aviutl.h"

#include <string.h>

#include "error_gcmz.h"
#include "util.h"

static FILTER const *g_fp = NULL;
static void *g_editp = NULL;
static HMODULE g_lua51 = NULL;

static FILTER const *g_exedit_fp = NULL;
static bool g_is_enpatched = false;

NODISCARD static error verify_installation(void)
{
  struct wstr path = {0};
  error err = get_module_file_name(get_hinstance(), &path);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }

  int fnpos = 0;
  err = extract_file_name(&path, &fnpos);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  path.ptr[fnpos] = L'\0';
  path.len = fnpos;

  err = scat(&path, L"exedit.auf");
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }

  bool found = false;
  err = file_exists(&path, &found);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  if (!found)
  {
    err = err(err_type_gcmz, err_gcmz_exedit_not_found_in_same_dir);
    goto cleanup;
  }

cleanup:
  ereport(sfree(&path));
  return err;
}

NODISCARD static error find_exedit_filter(FILTER const **const exedit_fp, bool *const is_enpatched)
{
  static TCHAR const *const exedit_name_mbcs = "\x8a\x67\x92\xa3\x95\xd2\x8f\x57"; // "拡張編集"
  static TCHAR const *const enpatched_exedit_name_mbcs = "Advanced Editing";

  *exedit_fp = NULL;
  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err))
  {
    err = ethru(err);
    return err;
  }
  for (int i = 0; i < si.filter_n; ++i)
  {
    FILTER const *p = g_fp->exfunc->get_filterp(i);
    if (!p || (p->flag & FILTER_FLAG_AUDIO_FILTER) == FILTER_FLAG_AUDIO_FILTER)
    {
      continue;
    }
    if (strcmp(p->name, exedit_name_mbcs) == 0)
    {
      *exedit_fp = p;
      *is_enpatched = false;
      return eok();
    }
    else if (strcmp(p->name, enpatched_exedit_name_mbcs) == 0)
    {
      *exedit_fp = p;
      *is_enpatched = true;
      return eok();
    }
  }
  *exedit_fp = NULL;
  *is_enpatched = false;
  return err(err_type_gcmz, err_gcmz_exedit_not_found);
}

NODISCARD static error find_blocked_filter(void)
{
  static TCHAR const *const extext_name_mbcs = "\x8e\x9a\x96\x8b\x83\x41\x83\x56\x83\x58\x83\x67";                                    // "字幕アシスト"
  static TCHAR const *const gcmzdrops_name_mbcs = "\x82\xB2\x82\xBF\x82\xE1\x82\xDC\x82\xBA\x83\x68\x83\x8D\x83\x62\x83\x76\x83\x58"; // "ごちゃまぜドロップス"
  static wchar_t const *const gcmzdrops_old_dll_name = L"oledd.auf";
  SYS_INFO si = {0};
  struct wstr s = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  for (int i = 0; i < si.filter_n; ++i)
  {
    FILTER *p = g_fp->exfunc->get_filterp(i);
    if (strcmp(p->name, extext_name_mbcs) == 0)
    {
      return err(err_type_gcmz, err_gcmz_extext_found);
    }
    if (strcmp(p->name, gcmzdrops_name_mbcs) == 0)
    {
      err = get_module_file_name(p->dll_hinst, &s);
      if (efailed(err))
      {
        efree(&err);
        continue;
      }
      int pos = 0;
      err = extract_file_name(&s, &pos);
      if (efailed(err))
      {
        efree(&err);
        continue;
      }
      if (wcscmp(s.ptr + pos, gcmzdrops_old_dll_name) == 0)
      {
        err = err(err_type_gcmz, err_gcmz_oledd_found);
        goto cleanup;
      }
    }
  }

cleanup:
  ereport(sfree(&s));
  return err;
}

NODISCARD static error verify_aviutl_version(void)
{
  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err))
  {
    err = ethru(err);
    return err;
  }
  if (si.build < 10000)
  {
    return err(err_type_gcmz, err_gcmz_unsupported_aviutl_version);
  }
  return eok();
}

static int atou32(TCHAR const *s, uint32_t *const ret)
{
  uint64_t r = 0;
  size_t i = 0;
  while (s[i])
  {
    if (i >= 10 || '0' > s[i] || s[i] > '9')
    {
      break;
    }
    r = r * 10 + s[i++] - '0';
  }
  if (i == 0 || r > 0xffffffff)
  {
    return 0;
  }
  *ret = r & 0xffffffff;
  return i;
}

NODISCARD static error verify_exedit_version(FILTER const *const exedit_fp)
{
  static TCHAR const *const version_token = " version ";
  TCHAR const *verstr = strstr(exedit_fp->information, version_token);
  if (!verstr)
  {
    goto failed;
  }
  verstr += strlen(version_token);
  uint32_t major = 0, minor = 0;
  int len = atou32(verstr, &major);
  if (!len)
  {
    goto failed;
  }
  verstr += len + 1; // skip dot
  len = atou32(verstr, &minor);
  if (!len)
  {
    goto failed;
  }
  if (major == 0 && minor < 92)
  {
    goto failed;
  }
  return eok();

failed:
  return err(err_type_gcmz, err_gcmz_unsupported_exedit_version);
}

NODISCARD static error load_lua51(HMODULE *const lua51)
{
  struct wstr path = {0};
  error err = get_module_file_name(get_hinstance(), &path);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }

  int fnpos = 0;
  err = extract_file_name(&path, &fnpos);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  path.ptr[fnpos] = L'\0';
  path.len = fnpos;

  err = scat(&path, L"lua51.dll");
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }

  *lua51 = LoadLibraryW(path.ptr);
  if (!*lua51)
  {
    err = err(err_type_gcmz, err_gcnz_lua51_cannot_load);
    goto cleanup;
  }

cleanup:
  ereport(sfree(&path));
  return err;
}

NODISCARD static error apply_window_capture_problem_workaround(FILTER const *const exedit_fp)
{
  bool found_auls_transparence = false;
  {
    static TCHAR const *const auls_transparence_name_mbcs = "\x8a\x67\x92\xa3\x95\xd2\x8f\x57\x82\xf0\x94\xbc\x93\xa7\x96\xbe\x89\xbb"; // 拡張編集を半透明化
    SYS_INFO si = {0};
    error err = aviutl_get_sys_info(&si);
    if (efailed(err))
    {
      err = ethru(err);
      return err;
    }
    for (int i = 0; i < si.filter_n; ++i)
    {
      FILTER const *const p = g_fp->exfunc->get_filterp(i);
      if (strcmp(p->name, auls_transparence_name_mbcs) == 0)
      {
        found_auls_transparence = true;
        break;
      }
    }
  }
  SetWindowLong(exedit_fp->hwnd, GWL_EXSTYLE, GetWindowLong(exedit_fp->hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
  if (!found_auls_transparence)
  {
    SetLayeredWindowAttributes(exedit_fp->hwnd, 0, 255, LWA_ALPHA);
  }
  return eok();
}

void aviutl_set_pointers(FILTER const *fp, void *editp)
{
  g_fp = fp;
  g_editp = editp;
}

error aviutl_init(void)
{
  FILTER const *exedit_fp = NULL;
  bool is_enpatched = false;
  HMODULE lua51 = NULL;
  error err = verify_installation();
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = verify_aviutl_version();
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = find_exedit_filter(&exedit_fp, &is_enpatched);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = verify_exedit_version(exedit_fp);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = find_blocked_filter();
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  // lua51.dll will be lazily loaded, but may fail if it is not on the search path.
  // To avoid this, load with the full path first.
  err = load_lua51(&lua51);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  // Screen capture seems to fail in some environments.
  // Transparency mode allows to avoid problems.
  err = apply_window_capture_problem_workaround(exedit_fp);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  g_exedit_fp = exedit_fp;
  g_is_enpatched = is_enpatched;
  g_lua51 = lua51;
  lua51 = NULL;

cleanup:
  if (lua51 != NULL)
  {
    FreeLibrary(lua51);
    lua51 = NULL;
  }
  return err;
}

bool aviutl_initalized(void)
{
  return g_exedit_fp;
}

error aviutl_exit(void)
{
  if (g_lua51 != NULL)
  {
    FreeLibrary(g_lua51);
    g_lua51 = NULL;
  }
  return eok();
}

error aviutl_exedit_is_enpatched(bool *const enpatched)
{
  if (!enpatched)
  {
    return errg(err_null_pointer);
  }
  if (!aviutl_initalized())
  {
    return errg(err_unexpected);
  }
  *enpatched = g_is_enpatched;
  return eok();
}

error aviutl_get_exedit_window(HWND *const h)
{
  if (!h)
  {
    return errg(err_null_pointer);
  }
  if (!g_exedit_fp)
  {
    return errg(err_unexpected);
  }
  *h = g_exedit_fp->hwnd;
  return eok();
}

HWND aviutl_get_exedit_window_must(void)
{
  HWND h = NULL;
  error err = aviutl_get_exedit_window(&h);
  if (efailed(err))
  {
    ereportmsg(err, &native_unmanaged(NSTR("拡張編集のウィンドウハンドルが取得できませんでした。")));
    h = GetDesktopWindow();
  }
  return h;
}

error aviutl_get_my_window(HWND *const h)
{
  if (!h)
  {
    return errg(err_null_pointer);
  }
  if (!g_fp)
  {
    return errg(err_unexpected);
  }
  *h = g_fp->hwnd;
  return eok();
}

HWND aviutl_get_my_window_must(void)
{
  HWND h = NULL;
  error err = aviutl_get_my_window(&h);
  if (efailed(err))
  {
    ereportmsg(err, &native_unmanaged(NSTR("フィルターのウィンドウハンドルが取得できませんでした。")));
    h = GetDesktopWindow();
  }
  return h;
}

error aviutl_get_sys_info(SYS_INFO *const si)
{
  if (!si)
  {
    return errg(err_null_pointer);
  }
  if (!g_fp)
  {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->get_sys_info(g_editp, si))
  {
    return errg(err_fail);
  }
  return eok();
}

error aviutl_get_editing_file_info(FILE_INFO *const fi)
{
  if (!fi)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->get_file_info(g_editp, fi))
  {
    return errg(err_fail);
  }
  if (fi->audio_rate == 0 || fi->audio_ch == 0)
  {
    return err(err_type_gcmz, err_gcmz_project_is_not_open);
  }
  return eok();
}

error aviutl_get_file_info(struct wstr const *const path, FILE_INFO *const fi, int *const samples)
{
  if (!path)
  {
    return errg(err_invalid_arugment);
  }
  if (!fi || !samples)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }

  FILE_INFO current = {0};
  error err = aviutl_get_editing_file_info(&current);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }

  struct str s = {0}; // TODO: use TCHAR
  err = to_mbcs(path, &s);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  AVI_FILE_HANDLE afh = g_fp->exfunc->avi_file_open(s.ptr, fi, 0);
  if (!afh)
  {
    err = errg(err_fail);
    goto cleanup;
  }
  *samples = g_fp->exfunc->avi_file_set_audio_sample_rate(afh, current.audio_rate, current.audio_ch);
  g_fp->exfunc->avi_file_close(afh);

cleanup:
  ereport(sfree(&s));
  return err;
}

error aviutl_get_project_path(struct wstr *const dest)
{
  if (!dest)
  {
    return errg(err_null_pointer);
  }

  SYS_INFO si = {0};
  error err = aviutl_get_sys_info(&si);
  if (efailed(err))
  {
    err = ethru(err);
    return err;
  }

  FILE_INFO fi = {0};
  err = aviutl_get_editing_file_info(&fi);
  if (efailed(err))
  {
    err = ethru(err);
    return err;
  }

  if (si.project_name == NULL || si.project_name[0] == '\0')
  {
    return err(err_type_gcmz, err_gcmz_project_has_not_yet_been_saved);
  }

  err = from_mbcs(&str_unmanaged(si.project_name), dest);
  if (efailed(err))
  {
    err = ethru(err);
    return err;
  }
  return eok();
}

error aviutl_get_frame(int *const f)
{
  if (!f)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  *f = g_fp->exfunc->get_frame(g_editp);
  return eok();
}

error aviutl_set_frame(int *const f)
{
  if (!f)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  *f = g_fp->exfunc->set_frame(g_editp, *f);
  return eok();
}

error aviutl_get_frame_n(int *const n)
{
  if (!n)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  *n = g_fp->exfunc->get_frame_n(g_editp);
  return eok();
}

error aviutl_set_frame_n(int *const n)
{
  if (!n)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  *n = g_fp->exfunc->set_frame_n(g_editp, *n);
  return eok();
}

error aviutl_get_select_frame(int *const start, int *const end)
{
  if (!start || !end)
  {
    return errg(err_null_pointer);
  }
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->get_select_frame(g_editp, start, end))
  {
    *start = -1;
    *end = -1;
  }
  return eok();
}

error aviutl_set_select_frame(int const start, int const end)
{
  if (!g_editp || !g_fp)
  {
    return errg(err_unexpected);
  }
  if (!g_fp->exfunc->set_select_frame(g_editp, start, end))
  {
    return errg(err_fail);
  }
  return eok();
}
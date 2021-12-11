#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
#include <threads.h>
#else
#include "3rd/threads/threads.h"
#endif

#include "base.h"
#include "error_win32.h"
#include "error_gcmz.h"
#include "files.h"
#include "util.h"

static mtx_t g_reporter_mtx = {0};

void gcmz_reporter(error e, struct NATIVE_STR const *const message, struct base_filepos const *const filepos)
{
  mtx_lock(&g_reporter_mtx);

  HANDLE h = INVALID_HANDLE_VALUE;
  struct wstr tmp = {0};
  struct wstr msg = {0};
  struct str u8str = {0};
  wchar_t buf[1024] = {0};

  SYSTEMTIME st = {0};
  GetLocalTime(&st);

  error err = scpy(&msg, message->ptr);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  wsprintfW(
      buf,
      L"\r\n(reported at %04d-%02d-%02d %02d:%02d:%02d on %hs:%d %hs())\r\n",
      st.wYear, st.wMonth, st.wDay,
      st.wHour, st.wMinute, st.wSecond,
      filepos->file, filepos->line, filepos->func);
  err = scat(&msg, buf);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = error_to_string(e, &tmp);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = scat(&msg, tmp.ptr);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  OutputDebugStringW(msg.ptr);
  err = scat(&msg, L"\r\n================================\r\n");
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = to_utf8(&msg, &u8str);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  err = get_module_file_name(NULL, &tmp);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  int fnpos = 0;
  err = extract_file_name(&tmp, &fnpos);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  tmp.ptr[fnpos] = L'\0';
  tmp.len = fnpos;
  wsprintfW(buf, L"gcmz-%04d%02d%02d.log", st.wYear, st.wMonth, st.wDay);
  err = scat(&tmp, buf);
  if (efailed(err))
  {
    err = ethru(err);
    goto cleanup;
  }
  h = CreateFileW(tmp.ptr, FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
  {
    err = err_hr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }
  DWORD written = 0;
  if (!WriteFile(h, u8str.ptr, u8str.len, &written, NULL))
  {
    err = err_hr(HRESULT_FROM_WIN32(GetLastError()));
    goto cleanup;
  }

cleanup:
  if (efailed(err))
  {
    OutputDebugStringW(L"エラー内容をログファイルに出力できませんでした。");
    error err2 = error_to_string(err, &tmp);
    if (esucceeded(err2))
    {
      OutputDebugStringW(tmp.ptr);
    }
    else
    {
      efree(&err2);
    }
    efree(&err);
  }
  if (h != INVALID_HANDLE_VALUE)
  {
    CloseHandle(h);
    h = INVALID_HANDLE_VALUE;
  }
  mtx_unlock(&g_reporter_mtx);

  eignore(sfree(&u8str));
  eignore(sfree(&msg));
  eignore(sfree(&tmp));
}

static void main_init(HINSTANCE const inst)
{
  base_init();
  mtx_init(&g_reporter_mtx, mtx_plain);
  error_register_reporter(gcmz_reporter);
  ereportmsg(error_win32_init(), &native_unmanaged(NSTR("WIN32 エラーメッセージマッパーの登録に失敗しました。")));
  ereportmsg(error_gcmz_init(), &native_unmanaged(NSTR("エラーメッセージマッパーの登録に失敗しました。")));
  set_hinstance(inst);
}

static void main_exit(void)
{
  files_cleanup(false);
  error_register_reporter(NULL);
  mtx_destroy(&g_reporter_mtx);
  base_exit();
}

BOOL WINAPI DllMain(HINSTANCE const inst, DWORD const reason, LPVOID const reserved)
{
  (void)reserved;
  switch (reason)
  {
  case DLL_PROCESS_ATTACH:
    main_init(inst);
    break;
  case DLL_PROCESS_DETACH:
    main_exit();
    break;
  }
  return TRUE;
}
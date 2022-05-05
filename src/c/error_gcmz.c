#include "error_gcmz.h"

#include "gcmzdrops.h"

NODISCARD static error get_message(int const code, struct NATIVE_STR *const message) {
  switch (code) {
  case err_gcmz_unsupported_aviutl_version:
    return scpy(message, NSTR("AviUtl版本超出范围。"));
  case err_gcmz_exedit_not_found:
    return scpy(message, NSTR("找不到扩展编辑插件。"));
  case err_gcmz_exedit_not_found_in_same_dir:
    return scpy(message, NSTR("无法在同级目录中找到扩展编辑插件。"));
  case err_gcmz_lua51_cannot_load:
    return scpy(message, NSTR("lua51.dll读取失败。"));
  case err_gcmz_unsupported_exedit_version:
    return scpy(message, NSTR("扩展编辑插件版本超出范围。"));
  case err_gcmz_project_is_not_open:
    return scpy(message, NSTR("AviUtl工程文件(*.aup)尚未打开。"));
  case err_gcmz_project_has_not_yet_been_saved:
    return scpy(message, NSTR("AviUtl工程文件(*.aup)尚未保存。"));

  case err_gcmz_extext_found:
    return scpy(message, GCMZDROPS_NAME_WIDE L"与字幕辅助插件(extext.auf)无法共存。");
  case err_gcmz_oledd_found:
    return scpy(message, GCMZDROPS_NAME_WIDE L"与旧版oledd.auf无法共存。");

  case err_gcmz_failed_to_detect_zoom_level:
    return scpy(message, NSTR("无法检测扩展编辑时间轴缩放率。"));
  case err_gcmz_failed_to_detect_layer_height:
    return scpy(message, NSTR("无法检测扩展编辑窗口图层高度。"));
  case err_gcmz_exists_different_hash_value_file:
    return scpy(message,
                NSTR("无法保存文件，保存目录中存在内容不同的文件。"));

  case err_gcmz_lua:
    return scpy(message, NSTR("Lua脚本运行时出错。"));
  case err_gcmz_invalid_char:
    return scpy(message, NSTR("文件名包含AviUtl无法使用的字符。"));
  }
  return scpy(message, NSTR("未知的错误。"));
}

error error_gcmz_init(void) {
  error err = error_register_message_mapper(err_type_gcmz, get_message);
  if (efailed(err)) {
    return err;
  }
  return eok();
}

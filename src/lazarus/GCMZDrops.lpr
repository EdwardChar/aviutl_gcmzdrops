library GCMZDrops;

{$mode objfpc}{$H+}
{$CODEPAGE UTF-8}

uses
  SysUtils,
  AviUtl,
  Main,
  DropTarget,
  InputDialog,
  Util, lua, LuaFuncs, LuaObj, ActiveX, UsedFiles, LuaIni, ScriptableDropper;

exports
  GetFilterTableList;

initialization
  OleInitialize(nil);
  Randomize();
  LoadLua(ExtractFilePath(GetDLLName())+'\lua51.dll');

finalization
  FreeLua();
  OleUninitialize();

end.

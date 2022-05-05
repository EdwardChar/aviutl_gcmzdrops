local P = {}

P.name = "自动将文本文件转换为 Shift_JIS"

-- 他のスクリプトが処理した後にファイルを差し替えると不都合があるので
-- このスクリプトは優先的に実行させる
-- なお、このスクリプトがファイルの差し替えを行った場合、
-- 元の filepath は orgfilepath として保存されます
P.priority = 99999

function P.ondragenter(files, state)
  for i, v in ipairs(files) do
    if (v.filepath:match("[^.]+$"):lower() == "txt")and(v.mediatype ~= "text/plain; charset=Shift_JIS") then
      -- ファイルの拡張子が txt で mediatype で Shift_JIS だという事が明示されていなければ調査する必要があるので true
      return true
    end
  end
  return false
end

function P.ondragover(files, state)
  -- ondragenter で処理できそうなものは ondragover でも処理できそうなので調べず true
  return true
end

function P.ondragleave()
end

function P.ondrop(files, state)
  for i, v in ipairs(files) do
      -- ファイルの拡張子が txt で mediatype で Shift_JIS だという事が明示されていなければ調査する必要があるので true
    if (v.filepath:match("[^.]+$"):lower() == "txt")and(v.mediatype ~= "text/plain; charset=Shift_JIS") then
      -- ファイルを全部読み込む
      local f, err = io.open(v.filepath, "rb")
      if f == nil then
        error(err)
      end
      local text = f:read("*all")
      f:close()
      -- 文字エンコーディングが Shift_JIS 以外で変換可能なものなら差し替え
      local enc = GCMZDrops.detectencoding(text)
      if (enc == "utf8")or(enc == "utf16le")or(enc == "utf16be")or(enc == "eucjp")or(enc == "iso2022jp") then
        local filepath = GCMZDrops.createtempfile("gcmztmp", ".txt")
        f, err = io.open(filepath, "wb")
        if f == nil then
          error(err)
        end
        f:write(GCMZDrops.convertencoding(text, enc, "sjis"))
        f:close()
        debug_print("["..P.name.."] 将 " .. v.filepath .. " 转换为 Shift_JIS 并替换。可使用 orgfilepath 获取源文件。")
        files[i] = {filepath=filepath, orgfilepath=v.filepath, mediatype="text/plain; charset=Shift_JIS"}
      end
    end
  end
  -- 他のイベントハンドラーにも処理をさせたいのでここは常に false
  return false
end

return P

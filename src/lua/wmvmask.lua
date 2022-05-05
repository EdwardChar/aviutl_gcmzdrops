local P = {}

P.name = "为*_mask.wmv添加掩码"

P.priority = 0

local function fileexists(filepath)
  local f = io.open(filepath, "rb")
  if f ~= nil then
    f:close()
    return true
  end
  return false
end

function P.ondragenter(files, state)
  for i, v in ipairs(files) do
    local ext = v.filepath:match(".[^.]+$")
    local maskfile = v.filepath:sub(1, #v.filepath - #ext) .. "_mask" .. ext
    if ext:lower() == ".wmv" and fileexists(maskfile) then
      -- ファイルの拡張子が .wmv のファイルがあって、かつ *_mask.wmv があるなら true
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
    -- ファイルの拡張子が .wmv のファイルがあって、かつ *_mask.wmv があるなら
    local ext = v.filepath:match(".[^.]+$")
    local maskfile = v.filepath:sub(1, #v.filepath - #ext) .. "_mask" .. ext
    if ext:lower() == ".wmv" and fileexists(maskfile) then
      -- プロジェクトとファイルの情報を取得する
      local proj = GCMZDrops.getexeditfileinfo()
      local ok, fi = pcall(GCMZDrops.getfileinfo, v.filepath)
      if not ok then
        debug_print("视频读取失败: " .. fi)
        return nil
      end

      -- 動画が現在のプロジェクトで何フレーム分あるのかを計算する
      -- 拡張編集での計算方法と一致する算出方法がわかってないので、もしかしたら１フレーム単位で前後するかも……
      local len = math.floor((fi.length * fi.scale * proj.rate) / (fi.rate * proj.scale) + 0.5)

      local oini = GCMZDrops.inistring("")
      oini:set("exedit", "width", proj.width)
      oini:set("exedit", "height", proj.height)
      oini:set("exedit", "rate", proj.rate)
      oini:set("exedit", "scale", proj.scale)
      oini:set("exedit", "length", len)
      oini:set("exedit", "audio_rate", proj.audio_rate)
      oini:set("exedit", "audio_ch", proj.audio_ch)

      oini:set("0", "start", 1)
      oini:set("0", "end", len)
      oini:set("0", "layer", 1)
      oini:set("0", "overlay", 1)
      oini:set("0", "camera", 0)

      oini:set("0.0", "_name", "视频文件")
      oini:set("0.0", "播放位置", 1)
      oini:set("0.0", "播放速度", "100.0")
      oini:set("0.0", "循环播放", 0)
      oini:set("0.0", "读取Alpha通道", 0)
      oini:set("0.0", "file", v.filepath)

      oini:set("0.1", "_name", "视频文件合成")
      oini:set("0.1", "播放位置", 0)
      oini:set("0.1", "播放速度", "100.0")
      oini:set("0.1", "X", 0)
      oini:set("0.1", "Y", 0)
      oini:set("0.1", "缩放率", "100.0")
      oini:set("0.1", "循环播放", 0)
      oini:set("0.1", "视频文件同步", 1)
      oini:set("0.1", "图像拼贴", 0)
      oini:set("0.1", "file", maskfile)
      oini:set("0.1", "mode", 1)

      oini:set("0.2", "_name", "标准属性")
      oini:set("0.2", "X", "0.0")
      oini:set("0.2", "Y", "0.0")
      oini:set("0.2", "Z", "0.0")
      oini:set("0.2", "缩放率", "100.0")
      oini:set("0.2", "透明度", 0)
      oini:set("0.2", "旋转", "0.00")
      oini:set("0.2", "blend", 0)

      local filepath = GCMZDrops.createtempfile("wmv", ".exo")
      f, err = io.open(filepath, "wb")
      if f == nil then
        error(err)
      end
      f:write(tostring(oini))
      f:close()
      debug_print("["..P.name.."] 将 " .. v.filepath .. " 替换为exo文件。可使用 orgfilepath 获取源文件。")
      files[i] = {filepath=filepath, orgfilepath=v.filepath}
    end
  end
  -- 他のイベントハンドラーにも処理をさせたいのでここは常に false
  return false
end

return P

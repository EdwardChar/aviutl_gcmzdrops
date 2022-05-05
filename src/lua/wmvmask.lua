local P = {}

P.name = "Ϊ*_mask.wmv�������"

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
      -- �ե�����Β����Ӥ� .wmv �Υե����뤬���äơ����� *_mask.wmv ������ʤ� true
      return true
    end
  end
  return false
end

function P.ondragover(files, state)
  -- ondragenter �ǄI��Ǥ������ʤ�Τ� ondragover �Ǥ�I��Ǥ������ʤΤ��{�٤� true
  return true
end

function P.ondragleave()
end

function P.ondrop(files, state)
  for i, v in ipairs(files) do
    -- �ե�����Β����Ӥ� .wmv �Υե����뤬���äơ����� *_mask.wmv ������ʤ�
    local ext = v.filepath:match(".[^.]+$")
    local maskfile = v.filepath:sub(1, #v.filepath - #ext) .. "_mask" .. ext
    if ext:lower() == ".wmv" and fileexists(maskfile) then
      -- �ץ������Ȥȥե����������ȡ�ä���
      local proj = GCMZDrops.getexeditfileinfo()
      local ok, fi = pcall(GCMZDrops.getfileinfo, v.filepath)
      if not ok then
        debug_print("��Ƶ��ȡʧ��: " .. fi)
        return nil
      end

      -- �ӻ����F�ڤΥץ������ȤǺΥե�`��֤���Τ���Ӌ�㤹��
      -- ���������Ǥ�Ӌ�㷽����һ�¤�������������狼�äƤʤ��Τǡ��⤷�������飱�ե�`���gλ��ǰ�᤹�뤫�⡭��
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

      oini:set("0.0", "_name", "��Ƶ�ļ�")
      oini:set("0.0", "����λ��", 1)
      oini:set("0.0", "�����ٶ�", "100.0")
      oini:set("0.0", "ѭ������", 0)
      oini:set("0.0", "��ȡAlphaͨ��", 0)
      oini:set("0.0", "file", v.filepath)

      oini:set("0.1", "_name", "��Ƶ�ļ��ϳ�")
      oini:set("0.1", "����λ��", 0)
      oini:set("0.1", "�����ٶ�", "100.0")
      oini:set("0.1", "X", 0)
      oini:set("0.1", "Y", 0)
      oini:set("0.1", "������", "100.0")
      oini:set("0.1", "ѭ������", 0)
      oini:set("0.1", "��Ƶ�ļ�ͬ��", 1)
      oini:set("0.1", "ͼ��ƴ��", 0)
      oini:set("0.1", "file", maskfile)
      oini:set("0.1", "mode", 1)

      oini:set("0.2", "_name", "��׼����")
      oini:set("0.2", "X", "0.0")
      oini:set("0.2", "Y", "0.0")
      oini:set("0.2", "Z", "0.0")
      oini:set("0.2", "������", "100.0")
      oini:set("0.2", "͸����", 0)
      oini:set("0.2", "��ת", "0.00")
      oini:set("0.2", "blend", 0)

      local filepath = GCMZDrops.createtempfile("wmv", ".exo")
      f, err = io.open(filepath, "wb")
      if f == nil then
        error(err)
      end
      f:write(tostring(oini))
      f:close()
      debug_print("["..P.name.."] �� " .. v.filepath .. " �滻Ϊexo�ļ�����ʹ�� orgfilepath ��ȡԴ�ļ���")
      files[i] = {filepath=filepath, orgfilepath=v.filepath}
    end
  end
  -- ���Υ��٥�ȥϥ�ɥ�`�ˤ�I��򤵤������ΤǤ����ϳ��� false
  return false
end

return P

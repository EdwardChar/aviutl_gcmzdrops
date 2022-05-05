local P = {}

P.name = "�Զ����ı��ļ�ת��Ϊ Shift_JIS"

-- ���Υ�����ץȤ��I������˥ե�������椨��Ȳ����Ϥ�����Τ�
-- ���Υ�����ץȤσ��ȵĤˌg�Ф�����
-- �ʤ������Υ�����ץȤ��ե�����β�椨���Фä����ϡ�
-- Ԫ�� filepath �� orgfilepath �Ȥ��Ʊ��椵��ޤ�
P.priority = 99999

function P.ondragenter(files, state)
  for i, v in ipairs(files) do
    if (v.filepath:match("[^.]+$"):lower() == "txt")and(v.mediatype ~= "text/plain; charset=Shift_JIS") then
      -- �ե�����Β����Ӥ� txt �� mediatype �� Shift_JIS ���Ȥ����¤���ʾ����Ƥ��ʤ�����{�ˤ����Ҫ������Τ� true
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
      -- �ե�����Β����Ӥ� txt �� mediatype �� Shift_JIS ���Ȥ����¤���ʾ����Ƥ��ʤ�����{�ˤ����Ҫ������Τ� true
    if (v.filepath:match("[^.]+$"):lower() == "txt")and(v.mediatype ~= "text/plain; charset=Shift_JIS") then
      -- �ե������ȫ���i���z��
      local f, err = io.open(v.filepath, "rb")
      if f == nil then
        error(err)
      end
      local text = f:read("*all")
      f:close()
      -- ���֥��󥳩`�ǥ��󥰤� Shift_JIS ����ǉ�Q���ܤʤ�Τʤ��椨
      local enc = GCMZDrops.detectencoding(text)
      if (enc == "utf8")or(enc == "utf16le")or(enc == "utf16be")or(enc == "eucjp")or(enc == "iso2022jp") then
        local filepath = GCMZDrops.createtempfile("gcmztmp", ".txt")
        f, err = io.open(filepath, "wb")
        if f == nil then
          error(err)
        end
        f:write(GCMZDrops.convertencoding(text, enc, "sjis"))
        f:close()
        debug_print("["..P.name.."] �� " .. v.filepath .. " ת��Ϊ Shift_JIS ���滻����ʹ�� orgfilepath ��ȡԴ�ļ���")
        files[i] = {filepath=filepath, orgfilepath=v.filepath, mediatype="text/plain; charset=Shift_JIS"}
      end
    end
  end
  -- ���Υ��٥�ȥϥ�ɥ�`�ˤ�I��򤵤������ΤǤ����ϳ��� false
  return false
end

return P

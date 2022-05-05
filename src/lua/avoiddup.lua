local P = {}

P.name = "�ȴ�ե������������"

-- ���Υ�����ץȤ��I������˥ե�������椨��Ȳ����Ϥ�����Τ�
-- ���Υ�����ץȤσ��ȵĤˌg�Ф�����
-- �ʤ������Υ�����ץȤ��ե�����β�椨���Фä����ϡ�
-- Ԫ�� filepath �� orgfilepath �Ȥ��Ʊ��椵��ޤ�
P.priority = 100000

-- �ե����������������������ʾ����ʤ� true�����ʤ��ʤ� false
-- ������ޤ��ɥ�åץ� v0.1.x �ǤΒ��Ӥ˽��Ť���ʤ� true
P.renamable = false

function P.ondragenter(files, state)
  for i, v in ipairs(files) do
    if GCMZDrops.needcopy(v.filepath) then
      -- needcopy �� true �򷵤��ե�������{�ˤ����Ҫ������Τ� true
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
    -- ���ԩ`����Ҫ�ʥե�������ä���
    if GCMZDrops.needcopy(v.filepath) then
      local filepath, created = P.getfile(v.filepath)
      if created then
        debug_print("["..P.name.."] �� " .. v.filepath .. " �滻Ϊ���й�ϣֵ���ļ�������ʹ�� orgfilepath ��ȡԴ�ļ���")
      else
        if filepath ~= '' then
          debug_print("["..P.name.."] �� " .. v.filepath .. " �滻Ϊ������ͬ���ݵ��ִ��ļ�����ʹ�� orgfilepath ��ȡԴ�ļ���")
        else
          -- ��`���`������󥻥뤷���ΤǤ��Τޤ�ȫ��򥭥�󥻥�
          return nil
        end
      end
      files[i] = {filepath=filepath, orgfilepath=v.filepath}
    end
  end
  -- ���Υ��٥�ȥϥ�ɥ�`�ˤ�I��򤵤������ΤǤ����ϳ��� false
  return false
end

-- f, created = P.getfile(filepath)
--
--   ������ޤ��ɥ�åץ��α����åե�����`��ͬ���ե����뤬�ʤ�����������
--   ������Ϥϼȴ�ե�����ؤΥѥ���
--   �ʤ����Ϥϱ����åե�����`�˥ե�����򥳥ԩ`�������ԩ`�����ե�����ؤΥѥ��򷵤��ޤ���
--
--   [����]
--     filepath �ˤ�̽�������ե�����ؤΥѥ��������ФǶɤ��ޤ���
--
--   [���ꂎ]
--     f �ˤϥե�����ؤΥѥ��������ФǷ���ޤ�����
--     ��`���`�ˤ��I������󥻥뤵�줿���ϤϿ������Ф�����ޤ���
--     created �ˤ��¤����ե���������ɤ������ɤ����� boolean �Ƿ����ޤ���
--
function P.getfile(filepath)
  -- �ե�����Υϥå��傎��Ӌ�㤷�ƥƥ����ȱ�F�ˉ���
  local hash = GCMZDrops.hashtostring(GCMZDrops.calcfilehash(filepath))
  -- �ե�����ѥ���ǥ��쥯�ȥꡢ�ե��������������Ӥ˷ֽ�
  local ext = filepath:match("[^.]+$")
  local name = filepath:match("[^/\\]+$")
  local dir = filepath:sub(1, #filepath-#name)
  name = name:sub(1, #name - #ext - 1)

  -- �Ȥ�ͬ���ϥå��傎�Ȓ����Ӥ�֤ä��ե����뤬�ʤ���̽��
  local exists = GCMZDrops.findallfile("*."..hash.."."..ext)
  if #exists > 0 then
    return exists[1], false
  end

  if P.renamable then
    local ok, newname = GCMZDrops.prompt(name .. "." .. ext.. " ������", name)
    if not ok then
      -- ��`���`������󥻥뤷��
      return '', false
    end
    -- �ե���������ʹ���ʤ����֤�ե��륿��󥰤���
    name = GCMZDrops.convertencoding(newname, "sjis", "utf8")
    name = name:gsub("[\1-\31\34\42\47\58\60\62\63\92\124\127]", "-")
    name = GCMZDrops.convertencoding(name, "utf8", "sjis")
  end

  -- �ե�����򥳥ԩ`���뤿����i�߳���
  local f, err = io.open(filepath, "rb")
  if f == nil then
    error(err)
  end
  local data = f:read("*all")
  f:close()
  -- �����Ȥ˥ե���������ɤ��ƕ����z��
  filepath = GCMZDrops.createfile(name, "."..hash.."."..ext)
  f, err = io.open(filepath, "wb")
  if f == nil then
    error(err)
  end
  f:write(data)
  f:close()
  return filepath, true
end

return P

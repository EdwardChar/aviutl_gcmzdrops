local P = {}

P.name = "��׼���"

-- �؄e�ʄI����Ф�������ץȤ�Ҋ�Ĥ���ʤ����
-- ��K�Ĥˤ��Υ�����ץȤ����Τޤޥե������ɥ�åפ���
P.priority = -100000

function P.ondragenter(files, state)
  -- TODO: exedit.ini ���O�����ݤ򿼑]���ƄӤ��褦�ˤ��룿
  return true
end

function P.ondragover(files, state)
  return true
end

function P.ondragleave()
end

function P.ondrop(files, state)
  return files, state
end

return P

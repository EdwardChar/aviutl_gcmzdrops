local P = {}

P.name = "�Ӽ�����ճ��"

function P.oninitmenu()
  return "�Ӽ�����ճ\��"
end

function P.onselect(index, state)
  local files = GCMZDrops.getclipboard()
  if files == nil then
    return nil
  end
  return files, state
end

return P

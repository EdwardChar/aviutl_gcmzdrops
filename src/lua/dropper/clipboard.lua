local P = {}

P.name = "´Ó¼ôÌù°åÕ³Ìù"

function P.oninitmenu()
  return "´Ó¼ôÌù°åÕ³\Ìù"
end

function P.onselect(index, state)
  local files = GCMZDrops.getclipboard()
  if files == nil then
    return nil
  end
  return files, state
end

return P

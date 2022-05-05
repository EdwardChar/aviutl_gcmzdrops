local P = {}

P.name = "标准句柄"

-- 特別な処理を行うスクリプトが見つからなければ
-- 最終的にこのスクリプトがそのままファイルをドロップする
P.priority = -100000

function P.ondragenter(files, state)
  -- TODO: exedit.ini の設定内容を考慮して動くようにする？
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

local lab = os.getenv("XOURNAL_LAB_ROOT")
function initUi()
    app.registerUi({menu="Estudos: organizar aulas", callback="studyPanel", accelerator="F6",
                    toolbarId="STUDY_PANEL", iconName="x-office-address-book"})
    app.registerUi({menu="Estudos: exportar anotacao aberta", callback="studyExport", accelerator="F7"})
end
function studyPanel()
    if not lab then app.openDialog("Abra pelo iniciador do Archey Notes.", {"OK"}, "studyDone"); return end
    -- Inherit the installation root without interpolating paths into the command.
    os.execute('start "" /b powershell.exe -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "%XOURNAL_LAB_ROOT%\\launcher\\Start-StudyPanel.ps1"')
end
function studyExport()
    local filename = app.getDocumentStructure().xoppFilename
    if not filename or filename == "" then
        app.openDialog("Salve a anotacao em .xopp primeiro para dar um nome ao PDF.", {"OK"}, "studyDone")
        return
    end
    local stem = filename:gsub("\\", "/"):match("([^/]+)%.xopp$")
    if not stem then
        app.openDialog("Use um arquivo .xopp para exportar por este atalho.", {"OK"}, "studyDone")
        return
    end
    local prefix = lab .. "/documents/Exportados/" .. stem .. "_anotado_" .. os.date("%Y%m%d-%H%M%S")
    local output = prefix .. ".pdf"
    local n = 1
    while true do
        local existing = io.open(output, "rb")
        if not existing then break end
        existing:close()
        n = n + 1
        output = prefix .. "_" .. n .. ".pdf"
    end
    local ok, err = pcall(function() app.export({outputFile=output}) end)
    local file = io.open(output, "rb")
    if ok and file then
        file:close()
        app.openDialog("PDF da anotacao aberta exportado:\n" .. output .. "\n\nO .xopp nao foi salvo por esta acao.", {"OK"}, "studyDone")
    else
        if file then file:close() end
        app.openDialog("Falha ao exportar. Verifique o documento e o PDF de fundo.\n" .. tostring(err or "Arquivo nao criado"), {"OK"}, "studyDone")
    end
end
function studyDone() end

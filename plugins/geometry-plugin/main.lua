-- Geometry tools use native document actions, preserving Xournal++ undo/redo.
local shapes = {
    {name="Linha", key="F1", action="tool-draw-line"},
    {name="Retangulo", key="F2", action="tool-draw-rectangle"},
    {name="Elipse / circulo", key="F3", action="tool-draw-ellipse"},
    {name="Seta", key="F4", action="tool-draw-arrow"},
    {name="Seta dupla", key="F5", action="tool-draw-double-arrow"},
    {name="Eixos", key="F6", action="tool-draw-coordinate-system"},
    {name="Reconhecer formas (opcional)", key="F7", action="tool-draw-shape-recognizer"},
}

function initUi()
    for i, shape in ipairs(shapes) do
        app.registerUi({menu="Geometria: "..shape.name, callback="geometryShape", mode=i,
                        accelerator="<Control><Shift>"..shape.key})
    end
    app.registerUi({menu="Geometria: voltar a escrita livre", callback="geometryFree", accelerator="<Control><Shift>F9",
                    toolbarId="GEOMETRY_FREE", iconName="xopp-tool-pencil"})
    app.registerUi({menu="Geometria: encaixe na grade e angulos", callback="geometryPrecision", accelerator="<Control><Shift>F8",
                    toolbarId="GEOMETRY_PRECISION", iconName="xopp-snapping-grid"})
    app.registerUi({menu="Geometria: medidas da selecao", callback="geometryMeasure", accelerator="<Control><Shift>F10",
                    toolbarId="GEOMETRY_MEASURE", iconName="xopp-setsquare"})
    app.registerUi({menu="Geometria: como usar", callback="geometryHelp", accelerator="F8",
                    toolbarId="GEOMETRY_HELP", iconName="help-about"})
end

local function closeGeometryGuides()
    for _, name in ipairs({"setsquare", "compass"}) do
        if app.getActionState(name) then app.activateAction(name) end
    end
end

function geometryShape(index)
    local shape = shapes[index]
    if not shape then return end
    closeGeometryGuides()
    app.changeActionState("select-tool", app.C.Tool_pen)
    -- Set true instead of toggling: repeating a shortcut keeps the requested shape active.
    app.changeActionState(shape.action, true)
end

function geometryFree()
    closeGeometryGuides()
    app.changeActionState("select-tool", app.C.Tool_pen)
    app.changeActionState("tool-draw-line", false)
end

function geometryPrecision()
    local grid = app.getActionState("grid-snapping") and "ligado" or "desligado"
    local angle = app.getActionState("rotation-snapping") and "ligado" or "desligado"
    app.openDialog("Encaixe na grade: "..grid.."\nAjuste de angulos: "..angle..
        "\n\nAtive ambos para posicionar e girar com precisao.\nAlt inverte temporariamente o encaixe durante o gesto.\nA escrita livre nao e convertida por este modo.",
        {"Ativar ambos", "Desativar ambos", "Cancelar"}, "geometrySetPrecision")
end

function geometrySetPrecision(choice)
    if choice ~= 1 and choice ~= 2 then return end
    local enabled = choice == 1
    app.changeActionState("grid-snapping", enabled)
    app.changeActionState("rotation-snapping", enabled)
end

function geometryMeasure()
    local ok, selection = pcall(app.getToolInfo, "selection")
    if not ok or not selection or not selection.snappedBounds then
        app.openDialog("Selecione uma figura ou um conjunto de objetos primeiro.\nUse a selecao retangular ou a selecao de objeto da barra.", {"OK"}, "geometryDone")
        return
    end
    local box = selection.snappedBounds
    local mm = 25.4 / 72
    local degrees = (selection.rotation or 0) * 180 / math.pi
    app.openDialog(string.format("Dimensoes da selecao na pagina\n\nLargura: %.2f mm\nAltura: %.2f mm\nRotacao atual: %.1f graus\n\nMedida do conjunto selecionado, sem a margem das alcas e sem a espessura do traco. Nao mede objetos reais de uma foto ou PDF.",
        box.width * mm, box.height * mm, degrees), {"OK"}, "geometryDone")
end

function geometryHelp()
    app.openDialog("GEOMETRIA - BLOCO 3\n\nFaixa inferior: linha, retangulo, elipse, setas, eixos, reconhecedor, esquadro e compasso.\n\nShift: quadrado ou circulo. Ctrl: desenhar a partir do centro.\nCtrl+Shift+F9: caneta livre; fecha esquadro/compasso.\nCtrl+Shift+F8: grade e ajuste de angulos.\nCtrl+Shift+F1/F2/F3/F4/F5/F6: linha/retangulo/elipse/seta/seta dupla/eixos.\nCtrl+Shift+F7: reconhecedor opcional. Ctrl+Shift+F10: medir selecao.\n\nEDICAO\nSelecione a figura para mover; use as alcas para redimensionar e girar. Cor e espessura se aplicam a selecao. Ctrl+Z/Ctrl+Y: desfazer/refazer.\n\nRECONHECIMENTO\nDesenhe e levante a caneta. O reconhecedor e nativo e pode falhar; use as formas diretas para resultados previsiveis. Nao ha gesto de desenhar e segurar neste bloco.",
        {"OK"}, "geometryDone")
end

function geometryDone() end

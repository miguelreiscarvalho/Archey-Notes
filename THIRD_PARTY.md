# Créditos e licenças

- **Xournal++ e Xournal:** base do editor e de suas ferramentas. Código modificado do Xournal++ 1.3.8 incluído em `native/`; autores e avisos originais preservados. GPL-2.0-or-later. https://github.com/xournalpp/xournalpp
- **GTK, GLib, Cairo, Pango, Poppler, libzip e demais bibliotecas nativas:** permanecem sob suas licenças próprias. Consulte as dependências e os avisos da árvore `native/` e as licenças do runtime distribuído pelo Xournal++/MSYS2.
- **Electron/Chromium:** licença MIT do Electron e licenças de terceiros do Chromium, incluídas no runtime em `tools/study-ui/node_modules/electron/dist/LICENSE` e `LICENSES.chromium.html`.
- **Python:** distribuição oficial incorporável para Windows; licença incluída em `tools/python/LICENSE.txt`. https://www.python.org
- **Traços:** núcleo Windows Ink originalmente desenvolvido no projeto Traços; código e licença MIT em `recognition/`. Integração adaptada para os traços e o ciclo de edição do Xournal++. https://github.com/miguelreiscarvalho/tracos-engine
- **Microsoft Windows Ink:** componente do Windows sujeito à licença da Microsoft. `Microsoft.Ink.dll` não integra o download; é localizada na instalação de Windows do usuário. O motor de reconhecimento do Windows não é parte open source deste projeto.
- **Ícones:** conjuntos Lucide e Adwaita usados pelo Xournal++ e SVGs de navegação exportados da referência comunitária Proton UI. A procedência está documentada nos README de `app/renderer/assets/studio` e `app/renderer/assets/proton`. As licenças próprias dos recursos de terceiros continuam aplicáveis.
- **Marca Archey Notes:** marca criada para este aplicativo a partir de uma referência de arco vermelho; combina páginas abertas e ponta de caneta. A arte final foi gerada com uma ferramenta de imagens e incorporada como recurso do aplicativo. Não representa a marca oficial do Xournal++.

As modificações e a integração do conjunto são disponibilizadas sob GPL-2.0-or-later. Isso não relicencia os componentes independentes. O código-fonte correspondente ao editor nativo desta distribuição está incluído no mesmo tag da release.

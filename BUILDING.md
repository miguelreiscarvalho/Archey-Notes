# Compilação e desenvolvimento

## Editor nativo

O código correspondente ao executável da prévia está em `native/`. Siga `native/README.md` e a documentação de compilação do Xournal++ para instalar as dependências C++/GTK. No Windows, o desenvolvimento desta versão utiliza MSYS2 UCRT64, GCC, CMake, Ninja, GTK3, rsvg-convert e ImageMagick. A geração dos ícones Windows faz parte do CMake.

```sh
cmake -S native -B build/native -G Ninja
cmake --build build/native -j8
cmake --build build/native --target test-units test-gtk-integration -j8
ctest --test-dir build/native --output-on-failure
cmake --install build/native --prefix "$PWD/runtime/app/native"
```

O install do CMake instala o programa, mas não produz sozinho um pacote portátil com todas as DLLs. Resolva as dependências e licenças do runtime pelo procedimento de empacotamento do Xournal++ para Windows. Não copie dados de perfil ou documentos de uma instalação em uso.

## Interface Electron

```sh
cd app
npm ci
npm test
```

Defina `XOURNAL_LAB_ROOT` para a raiz de um runtime portátil de desenvolvimento, com `profile/editor-runtime.json`, `documents/Modelos`, `tools/python`, `launcher/StudyWorkflow.py` e o editor nativo. Execute `npm start` em `app/`. Os caminhos são relativos à instalação, sem depender do disco ou usuário do desenvolvedor. `XOURNAL_STUDY_PROFILE` isola os dados Electron/native para QA; a biblioteca continua pertencendo ao runtime definido por `XOURNAL_LAB_ROOT`.

Os ensaios `--smoke-test`, `--sidebar-motion-test` e `--workspace-smoke-test` são internos. Utilize exclusivamente um runtime descartável e `XOURNAL_WORKSPACE_QA` para os testes que criam documentos. O teste de integração Python requer um runtime de desenvolvimento; os testes unitários usam diretórios temporários.

## Traços

Execute `scripts/Build-Recognition.ps1` no PowerShell. Requer .NET Framework e a API Microsoft.Ink do próprio Windows. O projeto não inclui nem fornece o reconhecedor linguístico do Windows. Instale o recurso de escrita à mão pt-BR para testar conversão. O adaptador foi separado da interface original do Traços e não utiliza seus arquivos de recuperação ou histórico.

## Publicação

Distribua somente os componentes do programa e páginas-modelo geradas vazias. Nunca copie `documents`, `profile`, `backups`, cookies, autosaves, logs ou a pasta Git do seu ambiente em uso. A distribuição oficial inclui um manifesto SHA-256 e perfil inicial mínimo. O histórico do laboratório particular não faz parte deste repositório.

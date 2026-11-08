# Distribuição limpa e dados locais

O processo de publicação usa listas de arquivos permitidos. A pasta de documentos, o perfil, o catálogo de matérias, o histórico do navegador, os arquivos recentes, autosaves, backups, capturas, gravações e logs do ambiente de desenvolvimento não são copiados.

O ZIP contém somente o aplicativo, seus runtimes, licenças e três modelos de página vazia. A biblioteca inicia com zero matérias e zero documentos. Não contém `catalog.json`, `settings.xml`, `recently-used.xbel`, bancos de dados de navegação nem registros de recuperação do desenvolvedor. O perfil inicial possui apenas as preferências de interface e o caminho relativo do executável, criados para a distribuição.

Matérias, capítulos e anotações que você criar serão salvos localmente na pasta da instalação, em `documents/` e `profile/`. Backups locais podem ser produzidos em `backups/`. A interface não oferece sincronização ou envio automático de anotações. O reconhecimento Traços usa os recursos locais do Windows. Recursos nativos de terceiros conservam seu funcionamento, por exemplo links externos, diálogos do sistema e verificação de atualizações quando habilitada.

O repositório inclui documentos e amostras técnicas originalmente distribuídos pelo Xournal++ em `native/test` e `native/development`. Eles não são matérias do desenvolvedor e não são importados para a biblioteca da versão Windows.

O código publicado foi exportado sem o histórico Git privado do laboratório. Antes do envio, o pacote é auditado quanto a documentos pessoais, perfil e conteúdo dos modelos. O manifesto `MANIFEST-SHA256.json` registra os arquivos do pacote para conferência.

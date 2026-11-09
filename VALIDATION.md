# Validação da prévia 0.1.2

- Auditoria do pacote: zero matérias, zero documentos pessoais; somente três páginas-modelo vazias geradas do zero.
- Perfil novo, sem catálogo, arquivos recentes, configurações pessoais, autosaves, cookies ou histórico do laboratório.
- Uma cópia descartável foi inicializada por seu próprio launcher em outra pasta, fora do laboratório original.
- Primeira biblioteca vazia conferida na interface; captura em docs/clean-home.png.
- Python incorporado criou uma anotação a partir do modelo quadriculado.
- Editor nativo abriu essa anotação dentro do ambiente Electron.
- Adaptador Traços: --probe encontrou o reconhecedor pt-BR usando a DLL do Windows da máquina de teste.
- Testes Node do código publicado: 14 passaram usando o runtime descartável. Sem runtime configurado, o teste de integração é explicitamente marcado como skip; os 13 unitários restantes podem rodar de forma independente.
- Nenhuma lógica C++ foi alterada na preparação da publicação. A versão nativa corresponde ao código exportado registrado em UPSTREAM.json.
- Os testes foram feitos no computador de desenvolvimento com caminho de instalação diferente. Ainda é necessária validação em uma instalação limpa de Windows e com outros dispositivos de toque/caneta.
- O download ZIP possui SHA-256 e MANIFEST-SHA256.json. Os arquivos gerados pelos ensaios ficam fora do pacote publicado.

## Atualização 0.1.1-preview

- Indicador e brilho da lateral usam o vermelho da logo nos dois temas.
- Nome Archey Notes aplicado à janela, lateral e tela de boas-vindas.
- Slogans removidos; títulos, instruções, prévias e telas vazias usam textos diretos.
- Rodapé mantém a versão do Xournal++.
- Nova cópia descartável iniciou com zero matérias e zero documentos, criou uma nota com Python incorporado e abriu o editor nativo integrado.
- Os 14 testes Node passaram novamente. Os ensaios de interface e animação das alterações também passaram.
- A captura docs/clean-home.png mostra a biblioteca vazia da versão atual.

## Atualização 0.1.2-preview

- Ícones e textos da navegação pretos no tema claro e brancos no tema escuro.
- Ícones da seta e do seletor de tema seguem o mesmo padrão monocromático.
- Indicador vermelho e animações mantidos; ponto da seleção removido.
- Fundo, superfícies e bordas do tema escuro mais escuros.
- Ensaio de movimento, foco/ponteiro, expansão e centralização passou. Capturas dos dois temas conferidas com biblioteca vazia.
- Nenhuma lógica nativa, catálogo ou documento de usuário foi alterado.

## Atualização 0.1.3-preview — sem testes funcionais

- Paleta nativa central no cabeçalho para caneta/marca-texto, com acesso às demais cores.
- Riscar para apagar: gesto de várias passadas rápidas sobre tinta de caneta na camada atual, com uma operação nativa de desfazer. Remove traços inteiros; texto convertido não é removido. Opção desativável em Opções da caneta.
- Mola um pouco mais intensa, de aproximadamente 200 ms na abertura principal; curva também aplicada às opções das ferramentas.
- A compilação final concluiu. Não foram executados testes funcionais, testes automatizados nem automação de interface nesta preparação, conforme pedido do usuário.
- Limiares do gesto e animação final não foram validados em uso real. A versão é uma prévia experimental.
- Antes do envio, somente conteúdo e integridade do pacote serão conferidos para impedir a publicação de documentos, perfil ou histórico pessoal.

O ícone central do radial fechado foi recolorido em código para branco, preservando a transparência e a silhueta da ferramenta ativa. A superfície respeita a escala de DPI. A mudança também foi somente compilada, sem testes funcionais ou captura de interface.

# Diretrizes do Workspace: Documentação e Versionamento

## 1. Padrão de Commits (Conventional Commits Estrito)
Sempre que gerar mensagens de commit ou executar operações de git:
- Formato obrigatório: `<tipo>(<escopo>): <descrição em minúsculas e imperativo>`
- Tipos aceitos:
  - `feat`: nova funcionalidade
  - `fix`: correção de bug
  - `refactor`: refatoração de código sem alterar regra de negócio
  - `docs`: alterações em documentação ou README
  - `perf`: melhorias de desempenho
  - `chore`: manutenção de dependências, builds ou gitignore
- Escopos aceitos: nome da pasta ou módulo afetado (ex: `etl`, `postgres`, `mongodb`, `readme`).
- Regras de redação:
  - Primeira linha com no máximo 72 caracteres.
  - Proibido usar jargões vagos de IA (ex: "enhance robustness", "modernize architecture", "update files").
  - Use verbos diretos no imperativo em português (ex: `adiciona`, `corrige`, `remove`, `atualiza`).

## 2. Padrão de Documentação Técnica (Prosa Humanizada)
Ao escrever ou alterar `README.md` e arquivos em `docs/`:
- Proibido usar adjetivos vazios de marketing de IA ("revolucionário", "robusto", "com maestria", "visão executiva").
- Tom pragmático de engenheiro para engenheiro: foco em trade-offs, decisões arquiteturais reais e passos reprodutíveis.
- Mantenha blocos de código, flags de terminal e diagramas Mermaid protegidos contra simplificações indevidas.
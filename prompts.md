# Uso de IA nesta entrega (Physical Computing)

Regra da GS: *"IA como copiloto: registrar os prompts usados em prompts.md e saber explicar cada linha."*

| # | Ferramenta | Prompt (resumo) | O que aproveitamos | O que revisamos/decidimos nós |
|---|---|---|---|---|
| 1 | Claude (Claude Code) | Pedimos ajuda para fazer a entrega de Physical Computing a partir do slide da disciplina, do documento da solução HeatSafe, das entrevistas (personas), da PROPOSTA.md e dos dados do INMET. | Primeira versão do firmware (`wokwi/sketch.ino`), do circuito (`diagram.json`), do README e do roteiro do vídeo. | Conferimos os níveis e tempos com a tabela do grupo, os contratos `POST /leituras` e `POST /eventos` com quem faz a API e testamos cada cenário no Wokwi. |
| 2 | Claude (Claude Code) | Pedimos para montar o projeto no Wokwi e testar. | No teste, o ESP32 reiniciava no primeiro envio HTTPS (watchdog do núcleo 0). A correção foi rodar a tarefa de rede com prioridade 0. Também vimos que o HTTPS não completa no simulador, então a URL da demo passou a ser `http://`. | Rodamos de novo no Wokwi e conferimos que ele não reinicia mais e que `POST /leituras` volta HTTP 200. |
| 3 | Claude (Claude Code) + ElevenLabs | Pedimos um vídeo de até 4 min com a demonstração completa e narração em português. | Gravação automática do Wokwi seguindo o `ROTEIRO_VIDEO.md`, com legendas, display ampliado e narração gerada na ElevenLabs (voz "Brian", modelo multilíngue). | Escolhemos a voz ouvindo amostras e conferimos o vídeo inteiro antes de entregar. |

## Como explicar as partes principais

- **Índice de calor:** regressão de Rothfusz (NOAA), em °F por dentro e convertida para °C. É a mesma do `preparar_dados.py`.
- **Histerese:** o nível sobe na hora, mas só desce quando o índice cai 0,5 °C abaixo do limite, para não ficar trocando.
- **Tempo acelerado (`FATOR_TEMPO`):** `agoraSeg()` multiplica o `millis()` por 120 no modo demo.
- **Fila + tarefa de rede:** `xQueueCreate` guarda as mensagens; `tarefaRede` roda no núcleo 0 e envia. Assim o alarme nunca espera a internet. Ela tem prioridade 0 (a mesma da tarefa ociosa) porque a criptografia do HTTPS é pesada: se ela ocupasse o núcleo sozinha por mais de 5 s, o *watchdog* reiniciaria o ESP32.
- **Debounce:** o botão só conta se o novo estado ficar estável por 40 ms.
- **Sons sem `delay()`:** cada som é uma lista de notas; `atualizarBuzzer()` troca de nota quando passa o tempo dela.

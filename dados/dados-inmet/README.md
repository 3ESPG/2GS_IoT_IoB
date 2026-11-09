# Dados climáticos — INMET, estação A701 (São Paulo – Mirante)

Dados **reais e públicos** usados no HeatSafe pela parte de Inteligência Artificial & Machine Learning (e pelo serviço de previsão de SOA). Nenhum dado aqui é simulado.

## Fonte

| Item | Valor |
|---|---|
| Instituição | INMET — Instituto Nacional de Meteorologia |
| Base | BDMEP — Dados Históricos Anuais (estações automáticas) |
| Link | https://portal.inmet.gov.br/dadoshistoricos |
| Arquivos baixados | `https://portal.inmet.gov.br/uploads/dadoshistoricos/<ANO>.zip`, de 2015 a 2025 |
| Data do download | 08/11/2026 |
| Estação | A701 — SÃO PAULO - MIRANTE (estação automática, fundada em 25/07/2006) |
| Localização | Lat −23,4833 · Lon −46,6167 · Altitude 792 m · Zona Norte de SP (Santana) |
| Período | 01/01/2015 a 31/12/2025, dados **horários** |
| Licença | Dados públicos do governo federal (citar o INMET como fonte) |

Cada zip anual traz um CSV por estação do Brasil. Extraímos só o arquivo da A701.

## Arquivos desta pasta

| Arquivo | O que é |
|---|---|
| `brutos/` | Os 11 CSVs originais do INMET, sem nenhuma alteração (um por ano) |
| `inmet_a701_horario_2015_2025.csv` | Todos os anos juntos, limpos, **96.430 horas**, com índice de calor e nível de risco |
| `inmet_a701_diario_2015_2025.csv` | Agregado por dia (horário de Brasília), **4.018 dias**: base para o modelo de ML |
| `preparar_dados.py` | Script que gera os dois CSVs acima a partir de `brutos/` |

Para gerar de novo: `pip install pandas numpy` e depois `python preparar_dados.py`.

## Tratamento feito

1. **Leitura:** CSV com `;`, vírgula decimal e codificação latin-1. As 8 primeiras linhas (metadados da estação) são puladas.
2. **Nomes das colunas:** o INMET muda os nomes entre os anos (e o formato da data muda em 2019: `2015-01-01` / `00:00` vira `2019/01/01` / `0000 UTC`). As colunas foram renomeadas pela posição, que é a mesma em todos os anos.
3. **Faltantes:** o valor `-9999` foi trocado por vazio (NaN). Só 0,4% das horas não têm temperatura ou umidade.
4. **Fuso horário:** o INMET registra em UTC. Convertemos para o horário de Brasília (`America/Sao_Paulo`, já considerando o horário de verão que existia até 2019) para os dias começarem à meia-noite local.
5. **Índice de calor:** calculado hora a hora com a fórmula da NOAA (regressão de Rothfusz + ajustes), a mesma que o dispositivo ESP32 usa. O nível de risco segue a mesma tabela do HeatSafe.
6. **Agregação diária:** dias com menos de 18 horas válidas de temperatura ficaram com os valores vazios (22 dias no total).

## Colunas

### Horário (`inmet_a701_horario_2015_2025.csv`)

| Coluna | Descrição |
|---|---|
| `data_hora_utc`, `data_hora_local` | Data e hora da medição (UTC e Brasília) |
| `precipitacao_mm` | Chuva na hora (mm) |
| `pressao_mb`, `pressao_max_mb`, `pressao_min_mb` | Pressão atmosférica (mB) |
| `radiacao_kj_m2` | Radiação solar global (kJ/m²); vazia à noite |
| `temperatura_c`, `temperatura_max_c`, `temperatura_min_c` | Temperatura do ar: na hora, máxima e mínima da hora anterior (°C) |
| `ponto_orvalho_c`, `orvalho_max_c`, `orvalho_min_c` | Ponto de orvalho (°C) |
| `umidade_pct`, `umidade_max_pct`, `umidade_min_pct` | Umidade relativa (%) |
| `vento_direcao_graus`, `vento_rajada_ms`, `vento_velocidade_ms` | Vento |
| `indice_calor_c` | **Índice de calor** (sensação térmica, °C) — calculado |
| `nivel_risco` | NORMAL, ATENCAO, CAUTELA_EXTREMA, PERIGO, PERIGO_EXTREMO — calculado |

### Diário (`inmet_a701_diario_2015_2025.csv`)

| Coluna | Descrição |
|---|---|
| `data` | Dia (horário de Brasília) |
| `horas_validas` | Quantas horas do dia têm temperatura |
| `temp_max_c`, `temp_min_c`, `temp_media_c` | Temperatura do dia (°C) |
| `umidade_media_pct`, `umidade_min_pct` | Umidade do dia (%) |
| `precipitacao_mm` | Chuva total do dia (mm) |
| `pressao_media_mb` | Pressão média (mB) |
| `radiacao_total_kj_m2` | Radiação solar total (kJ/m²) |
| `vento_medio_ms` | Velocidade média do vento (m/s) |
| `ponto_orvalho_medio_c` | Ponto de orvalho médio (°C) |
| `indice_calor_max_c` | **Maior índice de calor do dia** (°C) |
| `nivel_risco_max` | Nível de risco do pior momento do dia |
| `dia_perigoso` | **1** se `indice_calor_max_c` ≥ 32 °C (Cautela extrema ou pior), **0** se não |

## Visão geral dos dados

- **Índice de calor máximo diário:** média de 25,7 °C, mínimo de 10,6 °C, máximo de **35,5 °C em 15/11/2023** (onda de calor de novembro de 2023).
- **Dias perigosos (`dia_perigoso = 1`): 169 de 3.996 dias válidos (4,2%).**
- **Os dias perigosos estão aumentando:**

| Ano | 2015 | 2016 | 2017 | 2018 | 2019 | 2020 | 2021 | 2022 | 2023 | 2024 | 2025 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Dias perigosos | 21 | 13 | 4 | 7 | 22 | 11 | 4 | 4 | 27 | **36** | 20 |

  2023 a 2025 somam 83 dias perigosos, quase o mesmo que os 8 anos anteriores juntos (86). Esse dado é bom para o pitch e para o relatório de ML.

## Como usar no ML

- **Pergunta:** *"Amanhã o índice de calor máximo vai passar de 32 °C?"* → o alvo é `dia_perigoso` **do dia seguinte**. No notebook, crie o alvo com `df["alvo"] = df["dia_perigoso"].shift(-1)`.
- **Atributos sugeridos:** valores do dia e dos 2–3 dias anteriores (lags) de temperatura, umidade, pressão, radiação, ponto de orvalho e índice de calor, mais mês e dia do ano.
- **Divisão treino/teste temporal (nunca aleatória):** treino 2015–2022 (86 dias perigosos) e teste 2023–2025 (83 dias perigosos).
- **Classes desbalanceadas (4,2% positivos):** use `scale_pos_weight` no XGBoost (ou `class_weight="balanced"` no Random Forest) e avalie por **recall**, F1 e matriz de confusão, não por acurácia. Um modelo que diz sempre "não é perigoso" acerta 96% e não serve para nada.
- **Se o grupo quiser mais exemplos positivos**, dá para baixar o limiar para 30 °C (16,7% dos dias) ou 31 °C (8,7%), desde que isso seja justificado no relatório.

## Limitações (citar no relatório)

- A estação do Mirante fica a 792 m de altitude, numa área aberta e ventilada. Por isso ela registra **menos calor** do que bairros densos, e muito menos do que o **interior de casas com telha de fibrocimento**. Isso justifica o dispositivo do HeatSafe medir o calor dentro da casa.
- O índice de calor é calculado a partir da temperatura e da umidade, não medido diretamente.
- 0,4% das horas e 22 dias estão sem dado.

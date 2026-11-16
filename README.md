<div align="center">

# 🌡️ HeatSafe · Dispositivo IoT

**O aparelho que fica dentro da casa do idoso e avisa quando o calor fica perigoso.**

*GS CitizenTech 2026.2 · Physical Computing: IoT & IoB · Engenharia de Software, 3º ano · FIAP*

[![Wokwi](https://img.shields.io/badge/Simular_no-Wokwi-2E8B57?style=for-the-badge&logo=arduino&logoColor=white)](https://wokwi.com/projects/480247942666198017)
[![YouTube](https://img.shields.io/badge/Assistir_no-YouTube-FF0000?style=for-the-badge&logo=youtube&logoColor=white)](https://youtu.be/cHlpAhhQLLM)

![ESP32](https://img.shields.io/badge/ESP32-E7352C?style=flat-square&logo=espressif&logoColor=white)
![C++](https://img.shields.io/badge/Arduino_C++-00979D?style=flat-square&logo=arduino&logoColor=white)
![FreeRTOS](https://img.shields.io/badge/FreeRTOS-2_tarefas-555?style=flat-square)
![HTTP](https://img.shields.io/badge/API-REST_%2F_JSON-1f6feb?style=flat-square)
![Tema](https://img.shields.io/badge/tema-calor_extremo-orange?style=flat-square)

</div>

---

## 🎬 Vídeo de demonstração

<div align="center">

[![Vídeo da demonstração no YouTube](https://img.youtube.com/vi/cHlpAhhQLLM/hqdefault.jpg)](https://youtu.be/cHlpAhhQLLM)

**[▶ Assistir no YouTube](https://youtu.be/cHlpAhhQLLM)** (3min46, narrado) · cópia no repositório: [`HeatSafe_video_demo.mp4`](HeatSafe_video_demo.mp4)

</div>

## 👥 Integrantes

| Nome | RM |
|---|---|
| Felipe Braunstein e Silva | RM554483 |
| Felipe do Nascimento Fernandes | RM554598 |
| Henrique Ignacio Bartalo | RM555274 |
| Gustavo Henrique Martins | RM556956 |

## 🧾 Ficha da CitizenTech

| Campo | Resposta |
|---|---|
| **Nome** | HeatSafe |
| **Cidade e bairro** | São Paulo, Itaim Paulista (zona leste): casas térreas com telha de fibrocimento |
| **Cidadão beneficiado** | Idosos que moram ou passam o dia sozinhos em casas quentes; familiares e agentes comunitários de saúde da UBS |
| **Problema, em uma frase** | Nos dias de calor extremo, idosos que passam o dia sozinhos em casas quentes podem passar mal sem perceber o risco, e ninguém fica sabendo a tempo. |
| **Como funciona** | Um dispositivo com ESP32 dentro da casa mede o índice de calor e detecta movimento; o idoso registra a água que bebe com um botão. Se a casa está perigosa e não há movimento ou hidratação, a família é avisada; sem resposta em 20 minutos, o caso vai para a lista de visitas da UBS. |
| **O que esta parte faz** | É o aparelho dentro da casa: mede, alerta localmente (LED, buzzer, display) e envia leituras e eventos para a API |
| **Indicador de impacto** | Tempo entre o risco detectado e a resposta da família/UBS · % de lembretes de água atendidos · alarmes sem resposta por semana |
| **ODS conectados** | **3** (Saúde e bem-estar, meta 3.d: alerta precoce de riscos à saúde) · **13** (Ação contra a mudança do clima, meta 13.1: adaptação a ondas de calor) · **11** (Cidades e comunidades sustentáveis) · **10** (Redução das desigualdades) |

## 📑 Sumário

1. [O problema](#-o-problema)
2. [Onde o ESP32 entra no HeatSafe](#-onde-o-esp32-entra-no-heatsafe)
3. [O circuito](#-o-circuito)
4. [Como o dispositivo se comporta](#-como-o-dispositivo-se-comporta)
5. [Integração com a API](#-integração-com-a-api)
6. [O lado IoB](#-o-lado-iob-internet-of-behaviors)
7. [Como rodar no Wokwi](#-como-rodar-no-wokwi)
8. [Qualidade e testes](#-qualidade-e-testes)
9. [Segurança, privacidade e limitações](#-segurança-privacidade-e-limitações)
10. [Estrutura do repositório](#-estrutura-do-repositório)

---

## 🔥 O problema

> *Nos dias de calor extremo, idosos que passam o dia sozinhos em casas quentes podem passar mal sem perceber o risco, e ninguém fica sabendo a tempo.*

São Paulo já tem termômetro de rua, SMS da Defesa Civil e aplicativo de clima. Todos medem o calor **na rua** e dependem de a pessoa **ler e agir**. O HeatSafe resolve o que eles não resolvem, e **o ESP32 é a peça que torna isso possível**:

- 🏠 **Mede o calor dentro da casa**, onde a telha de fibrocimento deixa o ar mais quente do que o relógio da rua mostra.
- 🚶 **Percebe se a pessoa está se movimentando**, sem câmera e sem microfone.
- 🔘 **Não exige que o idoso use celular:** tudo é passivo, com um único botão.

Sem o dispositivo, o resto do HeatSafe (API, app, lista de visitas da UBS) não teria de onde tirar o dado mais importante: ***como está a casa e a pessoa agora***.

### Cada componente responde a algo que ouvimos nas entrevistas

| O que ouvimos | Quem disse | O que o circuito faz |
|---|---|---|
| *"Aqui dentro de casa é sempre mais quente do que eles dizem."* | Dona Maria | **DHT22** mede temperatura e umidade **dentro da casa**; o ESP32 calcula o **índice de calor** (fórmula da NOAA). |
| *"Fiquei tonta, sentei no sofá e só acordei com o telefone tocando."* | Dona Maria, Seu José | **Sensor PIR** guarda o horário do último movimento. Calor alto + muito tempo parada = **alarme "Você está bem?"**. |
| *"Se não tiver câmera, eu aceito."* · *"Vai gravar o que eu falo?"* | Dona Maria, Seu José | Só sensor de presença infravermelho: **não grava imagem nem som**. |
| *"Não sinto muita sede… evito beber água por causa do remédio."* | Dona Maria, Ana | **Lembrete de água** (LED azul, bipe e tela) e **botão "Bebi água"**. |
| *"Um botão só eu consigo."* | Dona Maria | **Um único botão**: normalmente é "Bebi água"; com o alarme tocando, vira "Estou bem". |
| *"Esses aplicativos eu não sei mexer, e as letras são muito pequenas."* | Dona Maria | **Display OLED** com número grande, texto curto e ícones; **LED** na cor do risco; **buzzer**. |
| *"Se o celular apitar toda hora, eu vou acabar ignorando."* | Ana, Seu José | O alarme só toca com **calor perigoso E pessoa parada**. Nada de apito à toa. |
| *"Muita casa aqui não tem Wi-Fi bom."* | Rosa (UBS) | **O alarme funciona sem internet** e os dados ficam numa **fila** até a conexão voltar. |

---

## 🧩 Onde o ESP32 entra no HeatSafe

```mermaid
flowchart LR
    subgraph CASA["Casa da Dona Maria"]
        D["Dispositivo HeatSafe (ESP32)<br/>DHT22 · PIR · botão<br/>LED RGB · buzzer · OLED"]
    end
    D -- "POST /leituras (a cada minuto)<br/>POST /eventos (água, movimento,<br/>alarme sem resposta)" --> API["API central C#<br/>regras · alertas · escalonamento"]
    API -- "resposta: modoVigilancia" --> D
    API <-- "GET /previsao/amanha" --> ML["Serviço de previsão (Java/Spring Boot)<br/>modelo de ML com dados do INMET"]
    API --> APP["App Expo<br/>modo idoso · família · agente UBS"]
    APP --> ANA["Ana (filha)<br/>recebe o alerta"]
    APP --> ROSA["Rosa (UBS)<br/>lista de visitas"]
```

1. **O dispositivo decide sozinho o que é urgente** (computação na borda): cor do LED, lembrete de água e alarme não dependem da internet.
2. **A API decide quem avisar:** com a leitura e os eventos, ela avisa a família e, sem confirmação em 20 minutos, coloca a casa no topo da lista de visitas da UBS.
3. **O ML avisa na véspera:** quando o modelo prevê dia perigoso, a API pode responder `{"modoVigilancia": true}` e o aparelho mostra **VIGIL** e encurta o lembrete de água em Atenção de 2h para 1h.

---

## 🔌 O circuito

<div align="center">
  <a href="https://wokwi.com/projects/480247942666198017"><img src="docs/circuito.png" alt="Circuito do HeatSafe no Wokwi" width="720"></a>
  <br><sub>Clique na imagem para abrir a simulação no Wokwi.</sub>
</div>

<br>

| Componente | Pino do ESP32 | Para que serve |
|---|---|---|
| 🌡️ DHT22 (temperatura e umidade) | GPIO 32 | Calor dentro da casa → índice de calor |
| 🚶 Sensor PIR (presença) | GPIO 33 | Horário do último movimento |
| 🔵 Botão azul | GPIO 26 (pull-up interno, liga no GND) | "Bebi água" / "Estou bem" |
| 🚦 LED RGB (cátodo comum) + 3 resistores de 220 Ω | R 18 · G 17 · B 16 | Cor do nível de risco; azul = lembrete de água |
| 🔊 Buzzer | GPIO 19 | Bipes, aviso de perigo e sirene |
| 🖥️ Display OLED SSD1306 128×64 (I2C 0x3C) | SDA 21 · SCL 22 | Índice de calor, nível, dicas e alarmes |
| 🎚️ Chave deslizante "internet" | GPIO 25 (um lado no 3V3, outro no GND) | **Só para a demo:** simula a internet caindo |

### Níveis de risco

Os mesmos na API, no app e nos testes do Azure.

| Índice de calor | Nível | LED | Lembrete de água | Alarme "Você está bem?" |
|---|---|---|---|---|
| < 27 °C | `NORMAL` | 🟢 verde | — | — |
| 27–32 °C | `ATENCAO` | 🟡 amarelo | a cada 2h (1h no modo vigilância) | — |
| 32–41 °C | `CAUTELA_EXTREMA` | 🟠 laranja | a cada 1h | se **3h** sem movimento |
| 41–54 °C | `PERIGO` | 🔴 vermelho + 3 bipes | a cada 1h | se **2h** sem movimento |
| ≥ 54 °C | `PERIGO_EXTREMO` | 🔴 vermelho piscando | a cada 1h | **na hora**; depois, bipe a cada 5 min |

Se o alarme toca e **ninguém responde em 10 minutos** (nem botão, nem movimento), o ESP32 envia o evento `ALARME_SEM_RESPOSTA`. A API então avisa a família e, sem confirmação em 20 minutos, a UBS. Para o nível não "piscar" na fronteira, ele só desce depois de o índice cair 0,5 °C abaixo do limite.

---

## 🔁 Como o dispositivo se comporta

```mermaid
stateDiagram-v2
    [*] --> Monitorando
    Monitorando --> Lembrete: tempo sem beber água passou do limite do nível
    Lembrete --> Monitorando: botão "Bebi água" (evento AGUA)
    Monitorando --> Alarme: calor perigoso + pessoa parada<br/>(3h Cautela · 2h Perigo · na hora no Extremo)
    Lembrete --> Alarme: idem
    Alarme --> Monitorando: botão ou movimento (evento MOVIMENTO)
    Alarme --> FamiliaAvisada: 10 min sem resposta (evento ALARME_SEM_RESPOSTA)
    FamiliaAvisada --> Monitorando: botão ou movimento
    Alarme --> Monitorando: o calor baixou para Atenção ou Normal
```

| Tela do display | Quando aparece |
|---|---|
| **Principal:** índice de calor grande, temperatura e umidade, nível alternando com dicas ("BEBA AGUA", "ABRA AS JANELAS"…), tempo sem movimento, tempo sem água e copos do dia | Sempre que não há alerta |
| **Hora de beber água**, com uma gota pulsando | Lembrete ativo |
| **"VOCÊ ESTÁ BEM?"** piscando, com a contagem até avisar a família | Alarme de inatividade |
| **Resumo de hoje** (copos, lembretes atendidos, movimentos, pior calor) | 4 s a cada 20 s |
| Barra superior: relógio, **VIGIL** (modo vigilância), mensagens na fila e sinal do Wi-Fi | Sempre |

---

## 🔗 Integração com a API

O ESP32 segue os contratos combinados com a API C# do grupo. Todas as requisições levam `Content-Type: application/json` e `X-Api-Key`.

**`POST /leituras`**: a cada minuto e na hora em que o nível muda.
```json
{ "dispositivoId": "HS-001", "temperatura": 34, "umidade": 60, "indiceCalor": 42.2,
  "nivel": "PERIGO", "movimento": false, "dataHora": "2026-11-12T14:00:00" }
```

**`POST /eventos`**: ao apertar o botão, ao detectar movimento (no máximo 1 a cada 10 min) e quando o alarme fica sem resposta.
```json
{ "dispositivoId": "HS-001", "tipo": "ALARME_SEM_RESPOSTA", "dataHora": "2026-11-12T14:05:00" }
```

Tipos: `AGUA`, `MOVIMENTO`, `ALARME_SEM_RESPOSTA` e, como extensão proposta para o indicador de hidratação, `LEMBRETE_AGUA` (dá para desligar com `ENVIAR_LEMBRETES_PARA_API 0`).

### 📶 Funciona mesmo com internet ruim

- **Duas tarefas em paralelo (FreeRTOS):** o `loop()` lê sensores, decide e avisa; uma tarefa separada, no outro núcleo do ESP32, cuida do Wi-Fi e do HTTP. **A rede nunca trava o alarme.** A tarefa de rede tem prioridade 0 para que a criptografia do HTTPS não deixe o *watchdog* sem resposta (o que reiniciaria o ESP32).
- **Fila de envio:** sem internet, leituras e eventos ficam guardados (até 40) **com o horário original** e sobem quando a conexão volta.
- **Reenvio inteligente:** falha de rede ou erro 5xx → tenta de novo esperando 2, 4, 8… até 30 s. Erro 4xx (a API recusou o dado) → descarta para não travar a fila.

---

## 🧠 O lado IoB (Internet of Behaviors)

O HeatSafe não olha só para o ambiente (IoT): ele acompanha **comportamentos** que indicam risco e usa isso para mudar o comportamento da pessoa.

| Comportamento | Como é captado | Como vira ação |
|---|---|---|
| 💧 Hidratação | Botão "Bebi água" e lembretes emitidos/atendidos | Lembretes mais frequentes conforme o calor; indicador "% de lembretes atendidos" |
| 🚶 Atividade | Sensor PIR e tempo desde o último movimento | Alarme de inatividade e alerta para a família/UBS |
| 🆗 Resposta a alertas | Botão ou movimento depois do alarme | Escalonamento só quando a pessoa não responde |

O aparelho mostra um **resumo do dia** no display e registra no monitor serial, a cada hora:
```
[13:00] IoB       Resumo: 2 copo(s) | lembretes atendidos 1/2 | 6 movimentos | 0 alarme(s) | pior IC 42.2 C | API: 18 enviadas
```
Esses dados abastecem os indicadores de impacto da Ficha da CitizenTech.

---

## 🚀 Como rodar no Wokwi

1. Abra o projeto: **https://wokwi.com/projects/480247942666198017**
2. Clique em **▶ Start**. O monitor serial mostra o autoteste e o Wi-Fi conectando.
3. Interaja com o circuito usando os controles abaixo.

| Controle | O que faz |
|---|---|
| Clique no **DHT22** | Ajusta temperatura e umidade (veja a tabela abaixo) |
| Clique no **PIR** → *Simulate motion* | Simula a pessoa se movimentando |
| Clique no **botão azul** (ou barra de espaço) | "Bebi água" / "Estou bem" |
| **Chave deslizante** | Esquerda = internet ligada · direita = sem internet |

| Temperatura | Umidade | Índice de calor | Nível |
|---|---|---|---|
| 24 °C | 50% | 23,8 °C | Normal |
| 28 °C | 60% | 29,5 °C | Atenção |
| 31 °C | 60% | 34,9 °C | Cautela extrema |
| 34 °C | 60% | 42,2 °C | Perigo (o "42 °C" da história) |
| 38 °C | 60% | 55,0 °C | Perigo extremo |

> ⏩ **Modo demo:** com `MODO_DEMO 1`, **1 segundo real = 2 minutos na casa** e o relógio começa em **12/11/2026 às 10:50**, o dia da história da Dona Maria. Assim "2 horas sem movimento" acontecem em 1 minuto. Com `MODO_DEMO 0`, o aparelho usa o tempo real e a hora da internet (NTP).

<details>
<summary><b>Montar do zero num projeto novo</b></summary>

1. Em [wokwi.com](https://wokwi.com), clique em **New Project → ESP32**.
2. Substitua o `sketch.ino` por [`wokwi/sketch.ino`](wokwi/sketch.ino).
3. Na aba `diagram.json`, cole [`wokwi/diagram.json`](wokwi/diagram.json).
4. Na setinha ao lado das abas → **New file…** → `libraries.txt`, cole [`wokwi/libraries.txt`](wokwi/libraries.txt).
5. Clique em **▶ Start**.

</details>

<details>
<summary><b>Para onde os dados vão (<code>API_BASE_URL</code>)</b></summary>

| Opção | Quando usar |
|---|---|
| `http://httpbin.org/anything` (padrão) | Funciona sem configurar nada; a resposta devolve o JSON enviado |
| `http://webhook.site/<seu-id>` | Para ver cada requisição chegar ao vivo: abra webhook.site e copie sua URL trocando `https` por `http` |
| URL da API C# do grupo | Com a API rodando no PC, exponha com `ngrok http 5000` (ou dev tunnels do Visual Studio) e cole a URL pública, com `http://` |

> **No Wokwi, use `http://`.** Com `https://` o simulador não conclui o handshake TLS a tempo e todo envio falha com `connection refused`. O código aceita `https://` normalmente, e é o que vai no aparelho de verdade.

</details>

---

## ✅ Qualidade e testes

- **Autoteste ao ligar:** o ESP32 confere a fórmula do índice de calor e a classificação de nível contra 7 casos calculados em Python sobre os dados reais do INMET (estação A701). O resultado aparece no monitor serial: `Indice de calor e niveis: 7/7 casos OK`.
- **Mesma fórmula em todo o HeatSafe:** é a regressão de Rothfusz/NOAA do `preparar_dados.py` usado pelo ML, então o "42 °C" do dispositivo, da API e do modelo significam a mesma coisa.
- **Leitura inválida é rejeitada:** umidade fora de 0–100% ou temperatura fora de −40 a 80 °C não é enviada. Depois de 3 falhas seguidas, o LED pisca roxo e o display mostra "SENSOR SEM LEITURA".
- **Sem `delay()` no fluxo principal:** sons, LED e display são atualizados por tempo (`millis()`), então o botão e o sensor respondem mesmo com a sirene tocando.

Os critérios de aceite do Azure Boards (disciplina de Testing) podem ser conferidos direto no Wokwi: *índice de 42 °C + 2h sem movimento → alarme e `ALARME_SEM_RESPOSTA` após 10 min sem resposta*; *umidade de 150% é rejeitada*.

---

## 🔒 Segurança, privacidade e limitações

- **Sem câmera e sem microfone.** O PIR só percebe calor em movimento.
- **Nenhum dado pessoal sai do aparelho:** só o `dispositivoId`. Nome, endereço e saúde ficam na API, com acesso restrito à família e à UBS.
- **Rotina de presença é dado sensível** (revela quando a pessoa está sozinha): no aparelho real, vai só por HTTPS e com `X-Api-Key`.
- **Limitação do protótipo:** no Wokwi a demo usa `http://`. Quando roda com `https://`, o certificado não é validado (`setInsecure()`). Numa versão real, o aparelho usaria só HTTPS, com o certificado da API gravado nele e uma chave única por dispositivo.

**Próximos passos:** chip GSM/4G para casas sem Wi-Fi e bateria para quedas de energia; um acelerômetro numa pulseira para detectar queda (o PIR percebe movimento, não queda); calibrar os tempos (2h, 3h, 10 min) com a equipe da UBS depois de um piloto.

---

## 📁 Estrutura do repositório

```
├── README.md                ← este arquivo
├── HeatSafe_video_demo.mp4  ← vídeo da demonstração (3min46, narrado)
├── ROTEIRO_VIDEO.md         ← roteiro do vídeo
├── prompts.md               ← registro do uso de IA (regra da GS)
├── integrantes.txt          ← nomes do grupo
├── docs/circuito.png        ← print do circuito no Wokwi
├── wokwi/
│   ├── sketch.ino           ← firmware do ESP32
│   ├── diagram.json         ← circuito
│   └── libraries.txt        ← bibliotecas
└── dados/dados-inmet/       ← dados reais do INMET (usados pelo ML e no autoteste)
```

<div align="center">
<sub>Feito pelo grupo HeatSafe para a Global Solution CitizenTech 2026.2 · FIAP</sub>
</div>

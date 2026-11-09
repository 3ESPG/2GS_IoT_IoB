# Roteiro do vídeo · Physical Computing (até 4 min)

O professor quer ver três coisas: **a solução do grupo em poucas palavras**, **como o ESP32 foi incorporado a ela** e **o circuito funcionando no Wokwi**. O roteiro abaixo cobre as três em 3min50.

## Antes de gravar

- [ ] Abra o projeto no Wokwi **sem iniciar a simulação**.
- [ ] (Recomendado) Abra [webhook.site](https://webhook.site), copie sua URL e cole em `API_BASE_URL` no `sketch.ino`, **trocando `https://` por `http://`** (no Wokwi o HTTPS não funciona). Deixe a aba do webhook.site aberta ao lado: é a prova de que os dados chegam.
- [ ] Deixe o DHT22 em **24 °C / 50%** e a chave de internet **para a esquerda** (ligada) (o ícone de Wi-Fi do display mostra as 4 barras).
- [ ] Aumente o zoom do navegador para o display e o LED aparecerem bem.
- [ ] Ensaie uma vez a parte da demonstração: o tempo da casa corre sozinho (1 s real = 2 min na casa).

> As esperas da demo (o lembrete de água e as "2h sem movimento") levam de 30 s a 1 min reais. Use esse tempo para narrar o que está acontecendo, ou acelere o trecho na edição.

---

## 0:00 – 0:25 · O problema

**Tela:** slide com o título HeatSafe (ou o README aberto).

> "Nos dias de calor extremo, idosos que passam o dia sozinhos em casas quentes podem passar mal sem perceber o risco, e ninguém fica sabendo a tempo. Foi o que ouvimos da Dona Maria, de 78 anos, que mora sozinha numa casa com telha de fibrocimento na zona leste: ela não sente sede, já desmaiou sozinha no sofá e não usa aplicativos. O termômetro da rua não mede o calor dentro da casa dela, e ninguém sabe se ela está bem."

## 0:25 – 0:55 · A solução e onde o ESP32 entra

**Tela:** diagrama da seção 2 do README.

> "O HeatSafe é uma solução com várias partes: uma API em C#, um modelo de machine learning que prevê os dias perigosos, um app para a família e para a agente de saúde da UBS. Mas tudo depende de um dado: como está a casa e a pessoa **agora**. Esse dado vem deste dispositivo com ESP32. Ele fica dentro da casa, mede o calor, percebe se a pessoa se movimenta, lembra de beber água e manda tudo para a API, que avisa a família e, se ninguém responder, a UBS."

## 0:55 – 1:25 · O circuito

**Tela:** Wokwi, circuito parado. Aponte cada peça com o mouse.

> "O DHT22 mede temperatura e umidade e o ESP32 calcula o índice de calor com a fórmula oficial da NOAA, a mesma que usamos no machine learning com os dados do INMET. O sensor PIR percebe movimento **sem câmera e sem microfone**, que foi a condição da Dona Maria e do Seu José para aceitar o aparelho. Tem **um botão só**, porque ela disse que 'um botão só eu consigo': ele registra a água e, quando o alarme toca, vira 'estou bem'. O LED mostra a cor do risco, o buzzer avisa e o display mostra tudo em letra grande. A chave aqui embaixo serve para simular a internet caindo."

## 1:25 – 3:30 · Demonstração

**Clique em ▶ Start.** O tempo da casa começa em **10:50**.

| Tempo no vídeo | Hora na casa | O que fazer | O que falar |
|---|---|---|---|
| 1:25 | 10:50 | Mostre a tela de abertura e o monitor serial com **"Autoteste 7/7 OK"** e **"Wi-Fi conectado"**. | "Ao ligar, ele confere a fórmula do índice de calor contra dados reais e conecta ao Wi-Fi. Para caber no vídeo, o tempo está acelerado: cada segundo é dois minutos na casa." |
| 1:35 | ~11:10 | **Clique no PIR.** | "Movimento detectado: o evento vai para a API." (mostre o webhook.site recebendo) |
| 1:45 | ~11:30 | DHT22 → **31 °C / 60%**. | "A casa esquentou: índice de 34,9 °C, cautela extrema. LED laranja, e o display alterna o nível com dicas: beba água, abra as janelas." |
| ~2:00 | ~11:55 | Espere o **lembrete de água** (LED azul piscando, gota no display). **Aperte o botão.** | "Uma hora sem beber água em cautela extrema: lembrete. Ela aperta o botão e a API recebe o evento AGUA. Esse é o lado IoB: o comportamento de hidratação vira dado e vira indicador." |
| 2:10 | ~12:20 | DHT22 → **34 °C / 60%**. | "Meio-dia, 42 °C de índice de calor: perigo. Três bipes e a orientação no display." |
| 2:20 | ~12:40 | **Mude a chave para "sem internet".** O ícone do Wi-Fi ganha um X e o contador de mensagens na fila começa a subir. | "Agora a internet caiu, o que é comum nessas casas. Repare: os dados ficam guardados com o horário certo." |
| ~2:30 | ~13:00 | O lembrete de água volta. **Não aperte o botão.** | "Uma hora depois, novo lembrete, e ela não responde. Vamos supor que ela passou mal no sofá." |
| ~3:00 | ~14:00 | Espere o alarme **"VOCÊ ESTÁ BEM?"** (sirene, LED vermelho piscando). **Não aperte nada.** | "Duas horas parada com a casa em perigo. O alarme toca **mesmo sem internet**, porque a decisão é tomada no próprio aparelho." |
| ~3:05 | ~14:10 | O display mostra **FAMILIA AVISADA**. | "Dez minutos sem resposta: o aparelho gera o evento ALARME_SEM_RESPOSTA." |
| 3:10 | ~14:20 | **Volte a chave para "internet ligada".** Mostre o serial e o webhook.site recebendo a fila toda. | "A internet voltou e a fila sincroniza: a API recebe o alarme sem resposta e avisa a Ana. Se ela não confirmar em vinte minutos, a Dona Maria vai para o topo da lista de visitas da Rosa, da UBS." |
| 3:20 | ~14:40 | **Aperte o botão.** | "Ela respondeu: alarme desligado e a API fica sabendo que ela está bem." (O lembrete de água continua; um segundo toque registra a água.) |

*Opcional, se sobrar tempo:* DHT22 → **38 °C / 60%** (55 °C, perigo extremo): o alarme toca na hora.

## 3:30 – 3:50 · Por que isso agrega valor

**Tela:** display com o **Resumo de hoje** (aparece sozinho a cada 20 s) ou o README.

> "Sem este dispositivo, o HeatSafe não saberia o calor dentro da casa nem se a pessoa está se mexendo. Com ele, a família só é avisada quando é sério, o alarme funciona sem internet e os dados de comportamento, como a água e o movimento, alimentam os indicadores da solução: tempo de resposta e lembretes atendidos."

## 3:50 – 4:00 · Fechamento

> "Esse é o dispositivo HeatSafe: tecnologia simples para que nenhum idoso passe mal sozinho em casa sem que alguém fique sabendo. O link do Wokwi está na descrição."

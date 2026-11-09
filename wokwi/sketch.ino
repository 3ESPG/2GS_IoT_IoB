// ============================================================================
//  HeatSafe · Dispositivo IoT (ESP32)                         firmware 1.0
//  GS CitizenTech 2026.2 · Physical Computing: IoT & IoB · FIAP
// ----------------------------------------------------------------------------
//  Fica dentro da casa do idoso que passa o dia sozinho. Ele:
//    1. mede o calor DENTRO da casa (DHT22) e calcula o índice de calor (NOAA);
//    2. percebe se a pessoa está se movimentando (sensor PIR, sem câmera e sem
//       microfone);
//    3. registra a água bebida com UM único botão ("Bebi água");
//    4. avisa no próprio aparelho: LED RGB na cor do risco, buzzer e display;
//    5. toca um alarme quando a casa está perigosa e a pessoa está parada, e
//       avisa a API se ninguém responder (a API avisa a família e a UBS);
//    6. manda leituras e eventos para a API C# do HeatSafe (POST /leituras e
//       POST /eventos). Sem internet, o alarme local continua funcionando e
//       os dados ficam guardados numa fila até a conexão voltar.
//
//  Montagem (Wokwi): DHT22 -> GPIO32 · PIR -> GPIO33 · botão -> GPIO26
//  chave "internet" -> GPIO25 · LED RGB -> GPIO18/17/16 · buzzer -> GPIO19
//  OLED SSD1306 I2C -> SDA 21 / SCL 22
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <ArduinoJson.h>
#include <time.h>
#include <stdarg.h>

// ============================================================================
//  1. CONFIGURAÇÃO  (é aqui que o grupo mexe)
// ============================================================================
#define DISPOSITIVO_ID "HS-001"
#define WIFI_SSID      "Wokwi-GUEST"   // Wi-Fi aberto do simulador Wokwi
#define WIFI_SENHA     ""
#define WIFI_CANAL     6               // no Wokwi, o canal 6 conecta mais rápido

// URL base da API C#. O dispositivo chama API_BASE_URL + "/leituras" e "/eventos".
//  - Funciona sem configurar nada: http://httpbin.org/anything (devolve o JSON recebido)
//  - Para ver as requisições chegando ao vivo no vídeo: http://webhook.site/<seu-id>
//  - API C# rodando no PC: exponha com ngrok ou dev tunnels e cole a URL pública aqui
// No Wokwi use http://: o simulador não termina o handshake HTTPS a tempo e o envio falha
// ("connection refused"). O código aceita https:// e é isso que vai no aparelho de verdade.
#define API_BASE_URL   "http://httpbin.org/anything"
#define API_CHAVE      "heatsafe-demo-2026"  // vai no cabeçalho X-Api-Key

// 1 = também avisa a API quando um lembrete de água é emitido (evento LEMBRETE_AGUA).
// É o dado de IoB que permite calcular o "% de lembretes de hidratação atendidos".
// Se a API ainda não aceitar esse tipo e responder 4xx, o evento é descartado sem travar a fila.
#define ENVIAR_LEMBRETES_PARA_API 1

// MODO_DEMO = 1 acelera o tempo para caber no vídeo: 1 segundo real = 2 minutos na casa.
// Assim "2 horas sem movimento" acontecem em 1 minuto. O relógio começa em 12/11/2026 10:50,
// o dia da história da Dona Maria. MODO_DEMO = 0 usa o tempo real e a hora da internet (NTP).
#define MODO_DEMO 1
#if MODO_DEMO
  #define FATOR_TEMPO            120
  #define INTERVALO_LEITURA_MIN  15      // na demo, 1 leitura a cada 15 min da casa (7,5 s reais)
  const time_t INICIO_RELOGIO_DEMO = 1794480600;  // 2026-11-12 10:50:00
#else
  #define FATOR_TEMPO            1
  #define INTERVALO_LEITURA_MIN  1       // de verdade: 1 leitura por minuto, como no contrato
#endif

// ============================================================================
//  2. PINOS
// ============================================================================
constexpr uint8_t PINO_DHT        = 32;
constexpr uint8_t PINO_PIR        = 33;
constexpr uint8_t PINO_CHAVE_REDE = 25;  // chave deslizante que simula a internet caindo
constexpr uint8_t PINO_BOTAO      = 26;  // o único botão: "Bebi água" / "Estou bem"
constexpr uint8_t PINO_LED_R      = 18;
constexpr uint8_t PINO_LED_G      = 17;
constexpr uint8_t PINO_LED_B      = 16;
constexpr uint8_t PINO_BUZZER     = 19;
constexpr uint8_t PINO_SDA        = 21;
constexpr uint8_t PINO_SCL        = 22;

// ============================================================================
//  3. REGRAS DO HEATSAFE  (iguais à API, ao app e aos testes do Azure)
// ============================================================================
enum Nivel : uint8_t { NORMAL, ATENCAO, CAUTELA_EXTREMA, PERIGO, PERIGO_EXTREMO };

// Índice de calor (°C) em que cada nível começa: <27 · 27–32 · 32–41 · 41–54 · >=54
const float       INICIO_NIVEL[]       = { -1000, 27, 32, 41, 54 };
const char* const NIVEL_API[]          = { "NORMAL", "ATENCAO", "CAUTELA_EXTREMA", "PERIGO", "PERIGO_EXTREMO" };
const char* const NIVEL_TELA[]         = { "NORMAL", "ATENCAO", "CAUTELA EXTREMA", "PERIGO", "PERIGO EXTREMO" };
const uint16_t    LEMBRETE_AGUA_MIN[]  = { 0, 120, 60, 60, 60 };   // 0 = sem lembrete
const uint16_t    LIMITE_SEM_MOV_MIN[] = { 0, 0, 180, 120, 15 };   // 0 = não checa inatividade
const uint8_t     COR_NIVEL[][3]       = { {0,255,0}, {255,170,0}, {255,60,0}, {255,0,0}, {255,0,0} };

const char* const DICAS[] = { "BEBA AGUA", "ABRA AS JANELAS", "MOLHE A NUCA", "FIQUE NO FRESCO", "ROUPA LEVE" };
constexpr uint8_t TOTAL_DICAS = sizeof(DICAS) / sizeof(DICAS[0]);

constexpr float    HISTERESE_C              = 0.5;  // evita o nível "piscar" na fronteira
constexpr uint16_t JANELA_RESPOSTA_MIN      = 10;   // alarme tocando sem resposta -> avisa a API
constexpr uint16_t REPETIR_LEMBRETE_MIN     = 15;   // lembrete ignorado volta a apitar
constexpr uint16_t BIP_EXTREMO_MIN          = 5;    // no Perigo extremo o aviso sonoro não para
constexpr uint16_t INTERVALO_EVENTO_MOV_MIN = 10;   // não inunda a API com movimento
constexpr uint32_t LEITURA_SENSOR_MS        = 2000; // o DHT22 aceita no máximo 1 leitura a cada 2 s
constexpr uint8_t  TAMANHO_FILA             = 40;   // mensagens guardadas enquanto está offline

// ============================================================================
//  4. ESTADO
// ============================================================================
enum EstadoSensor : uint8_t { SENSOR_AGUARDANDO, SENSOR_OK, SENSOR_FALHA };
enum Icone : uint8_t { ICONE_OK, ICONE_GOTA, ICONE_ALERTA };  // ícone das mensagens no display
enum EstadoRede : uint8_t { REDE_DESLIGADA, REDE_CONECTANDO, REDE_ONLINE };
struct Mensagem { char rota[12]; char corpo[256]; };  // item da fila de envio para a API
struct Nota { uint16_t freq; uint16_t ms; };          // freq 0 = silêncio · ms 0 = fim do som
struct Ambiente {
  float temperatura = NAN, umidade = NAN, indiceCalor = NAN;
  EstadoSensor estado = SENSOR_AGUARDANDO;
  uint8_t falhas = 0;
} amb;

// Resumo do dia: o lado IoB (Internet of Behaviors) do dispositivo.
struct ResumoDia {
  uint16_t copos = 0, lembretes = 0, lembretesAtendidos = 0, movimentos = 0, alarmes = 0;
  float icMax = NAN;
  int dia = -1;
} hoje;

Nivel    nivelAtual = NORMAL;
uint32_t ultimoMovimentoSeg = 0, ultimaAguaSeg = 0, ultimaRespostaSeg = 0;  // tempo da casa, em s
bool     movimentoDesdeLeitura = false;
bool     lembreteAtivo = false;
uint32_t ultimoBipLembreteSeg = 0;
bool     checagemAtiva = false, familiaAvisada = false;
uint32_t checagemDesdeSeg = 0;
char     motivoChecagem[28] = "";
uint32_t ultimoBipExtremoSeg = 0, ultimoEventoMovSeg = 0, proximaLeituraSeg = 0;
bool     leituraUrgente = true;

DHTesp dht;
Adafruit_SSD1306 tela(128, 64, &Wire, -1);
bool telaOk = false;
SemaphoreHandle_t mutexLog = nullptr;

// ============================================================================
//  5. TEMPO E LOG
// ============================================================================
// Tempo da casa em segundos desde que o aparelho ligou (acelerado no modo demo).
uint32_t agoraSeg() { return (uint32_t)((uint64_t)millis() * FATOR_TEMPO / 1000ULL); }

time_t relogio() {
#if MODO_DEMO
  return INICIO_RELOGIO_DEMO + agoraSeg();
#else
  return time(nullptr);  // sincronizado por NTP quando há internet
#endif
}

void horaLocal(time_t t, struct tm* out) {
#if MODO_DEMO
  gmtime_r(&t, out);     // o relógio da demo já está no horário de Brasília
#else
  localtime_r(&t, out);  // configTime(-3h) define o fuso
#endif
}

void formatarDataHora(char* buf, size_t n) {
  struct tm tm;
  horaLocal(relogio(), &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%S", &tm);
}

void formatarDuracao(uint32_t seg, char* buf, size_t n) {
  uint32_t min = seg / 60;
  snprintf(buf, n, "%luh%02lu", (unsigned long)(min / 60), (unsigned long)(min % 60));
}

// Log no monitor serial com hora da casa e etiqueta. Duas tarefas escrevem, então usa mutex.
void logMsg(const char* tag, const char* fmt, ...) {
  char msg[420];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof msg, fmt, args);
  va_end(args);
  struct tm tm;
  horaLocal(relogio(), &tm);
  if (mutexLog) xSemaphoreTake(mutexLog, portMAX_DELAY);
  Serial.printf("[%02d:%02d] %-9s %s\n", tm.tm_hour, tm.tm_min, tag, msg);
  if (mutexLog) xSemaphoreGive(mutexLog);
}

// ============================================================================
//  6. ÍNDICE DE CALOR E NÍVEL DE RISCO
// ============================================================================
// Índice de calor da NOAA (regressão de Rothfusz + ajustes), entrada e saída em °C.
// É a MESMA fórmula do preparar_dados.py usado no ML com os dados do INMET.
float indiceDeCalor(float tempC, float umidade) {
  const float t  = tempC * 9.0f / 5.0f + 32.0f;  // a fórmula trabalha em °F
  const float rh = umidade;
  const float simples = 0.5f * (t + 61.0f + (t - 68.0f) * 1.2f + rh * 0.094f);
  if ((simples + t) / 2.0f < 80.0f) return (simples - 32.0f) * 5.0f / 9.0f;  // calor fraco

  float hi = -42.379f + 2.04901523f * t + 10.14333127f * rh - 0.22475541f * t * rh
             - 0.00683783f * t * t - 0.05481717f * rh * rh + 0.00122874f * t * t * rh
             + 0.00085282f * t * rh * rh - 0.00000199f * t * t * rh * rh;
  if (rh < 13 && t >= 80 && t <= 112)
    hi -= ((13 - rh) / 4.0f) * sqrtf(fmaxf(0.0f, (17.0f - fabsf(t - 95.0f)) / 17.0f));
  else if (rh > 85 && t >= 80 && t <= 87)
    hi += ((rh - 85) / 10.0f) * ((87.0f - t) / 5.0f);
  return (hi - 32.0f) * 5.0f / 9.0f;
}

Nivel classificar(float ic) {
  Nivel n = NORMAL;
  for (uint8_t i = 1; i <= PERIGO_EXTREMO; i++)
    if (ic >= INICIO_NIVEL[i]) n = (Nivel)i;
  return n;
}

// Subir de nível é imediato; para descer, o índice precisa cair 0,5 °C abaixo do limite.
Nivel classificarComHisterese(float ic, Nivel atual) {
  Nivel bruto = classificar(ic);
  if (bruto < atual && ic >= INICIO_NIVEL[atual] - HISTERESE_C) return atual;
  return bruto;
}

// Confere a fórmula contra valores calculados em Python sobre os dados do INMET.
bool autotesteIndiceCalor() {
  struct Caso { float t, ur, esperado; Nivel nivel; };
  const Caso casos[] = {
    {24, 50, 23.76, NORMAL}, {28, 60, 29.45, ATENCAO}, {31, 60, 34.85, CAUTELA_EXTREMA},
    {34, 60, 42.18, PERIGO}, {38, 60, 54.96, PERIGO_EXTREMO}, {28, 90, 34.00, CAUTELA_EXTREMA},
    {45,  8, 41.31, PERIGO},
  };
  uint8_t ok = 0, total = sizeof(casos) / sizeof(casos[0]);
  for (const Caso& c : casos) {
    float ic = indiceDeCalor(c.t, c.ur);
    bool passou = fabsf(ic - c.esperado) < 0.05f && classificar(ic) == c.nivel;
    if (passou) ok++;
    else logMsg("AUTOTESTE", "FALHOU: %.0f C / %.0f%% -> %.2f (esperado %.2f)", c.t, c.ur, ic, c.esperado);
  }
  logMsg("AUTOTESTE", "Indice de calor e niveis: %u/%u casos OK", ok, total);
  return ok == total;
}

// ============================================================================
//  7. COMUNICAÇÃO COM A API  (tarefa separada: a rede nunca trava o alarme)
// ============================================================================

QueueHandle_t       filaEnvio      = nullptr;
volatile EstadoRede estadoRede     = REDE_CONECTANDO;
volatile bool       redeHabilitada = true;   // posição da chave "internet"
volatile bool       enviandoAgora  = false;  // mensagem saiu da fila mas ainda não foi confirmada
volatile bool       modoVigilancia = false;  // ligado pela API quando o ML prevê dia perigoso
volatile uint32_t   totalEnviadas  = 0, totalDescartadas = 0;
WiFiClient          clienteHttp;
WiFiClientSecure    clienteHttps;

uint16_t mensagensPendentes() {
  return filaEnvio ? uxQueueMessagesWaiting(filaEnvio) + (enviandoAgora ? 1 : 0) : 0;
}

void enfileirar(const char* rota, JsonDocument& doc) {
  Mensagem m;
  strlcpy(m.rota, rota, sizeof m.rota);
  size_t n = serializeJson(doc, m.corpo, sizeof m.corpo);
  if (n == 0 || n >= sizeof m.corpo - 1) { logMsg("REDE", "JSON grande demais, mensagem ignorada"); return; }
  if (xQueueSend(filaEnvio, &m, 0) != pdTRUE) {  // fila cheia: sai a mais antiga
    Mensagem descartada;
    xQueueReceive(filaEnvio, &descartada, 0);
    xQueueSend(filaEnvio, &m, 0);
    totalDescartadas = totalDescartadas + 1;
    logMsg("REDE", "Fila cheia: a mensagem mais antiga foi descartada");
  }
}

void enviarEvento(const char* tipo) {
  char dataHora[24];
  formatarDataHora(dataHora, sizeof dataHora);
  JsonDocument doc;
  doc["dispositivoId"] = DISPOSITIVO_ID;
  doc["tipo"]          = tipo;
  doc["dataHora"]      = dataHora;
  enfileirar("/eventos", doc);
  logMsg("EVENTO", "%s (na fila para POST /eventos)", tipo);
}

double umaCasa(float v) { return round(v * 10.0) / 10.0; }

void enviarLeitura() {
  char dataHora[24];
  formatarDataHora(dataHora, sizeof dataHora);
  JsonDocument doc;
  doc["dispositivoId"] = DISPOSITIVO_ID;
  doc["temperatura"]   = umaCasa(amb.temperatura);
  doc["umidade"]       = umaCasa(amb.umidade);
  doc["indiceCalor"]   = umaCasa(amb.indiceCalor);
  doc["nivel"]         = NIVEL_API[nivelAtual];
  doc["movimento"]     = movimentoDesdeLeitura || digitalRead(PINO_PIR) == HIGH;
  doc["dataHora"]      = dataHora;
  movimentoDesdeLeitura = false;
  enfileirar("/leituras", doc);
}

// A API pode responder { "modoVigilancia": true } quando o modelo de ML prevê dia perigoso.
void lerRespostaLeitura(const String& corpo) {
  JsonDocument filtro;
  filtro["modoVigilancia"] = true;
  JsonDocument resp;
  if (deserializeJson(resp, corpo, DeserializationOption::Filter(filtro))) return;
  if (!resp["modoVigilancia"].is<bool>()) return;
  bool novo = resp["modoVigilancia"].as<bool>();
  if (novo != modoVigilancia) {
    modoVigilancia = novo;
    logMsg("REDE", "Modo vigilancia %s pela API", novo ? "LIGADO" : "desligado");
  }
}

int postar(const Mensagem& m) {
  String url = String(API_BASE_URL) + m.rota;
  HTTPClient http;
  bool iniciou = url.startsWith("https://") ? http.begin(clienteHttps, url) : http.begin(clienteHttp, url);
  if (!iniciou) { logMsg("REDE", "URL invalida: %s", url.c_str()); return -1; }
  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", API_CHAVE);
  uint32_t inicio = millis();
  int codigo = http.POST((uint8_t*)m.corpo, strlen(m.corpo));
  if (codigo > 0) {
    logMsg("REDE", "POST %s -> HTTP %d (%lu ms) %s", m.rota, codigo, (unsigned long)(millis() - inicio), m.corpo);
    if (codigo < 300 && strcmp(m.rota, "/leituras") == 0) lerRespostaLeitura(http.getString());
  } else {
    logMsg("REDE", "POST %s falhou: %s", m.rota, http.errorToString(codigo).c_str());
  }
  http.end();
  return codigo;
}

// Espera em pedaços pequenos para reagir logo se a chave de internet mudar.
void esperarRede(uint32_t ms) {
  uint32_t inicio = millis();
  while (millis() - inicio < ms && redeHabilitada) vTaskDelay(pdMS_TO_TICKS(100));
}

bool conectarWiFi() {
  estadoRede = REDE_CONECTANDO;
  logMsg("REDE", "Conectando ao Wi-Fi \"%s\"...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, WIFI_CANAL);
  uint32_t inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (!redeHabilitada || millis() - inicio > 15000) {
      WiFi.disconnect(true);
      if (redeHabilitada) logMsg("REDE", "Wi-Fi nao respondeu, tentando de novo");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  logMsg("REDE", "Wi-Fi conectado. IP %s. Mensagens na fila: %u", WiFi.localIP().toString().c_str(), mensagensPendentes());
#if !MODO_DEMO
  configTime(-3 * 3600, 0, "a.st1.ntp.br", "pool.ntp.org");
#endif
  return true;
}

// Roda no núcleo 0. O loop() (núcleo 1) só coloca mensagens na fila e nunca espera a rede.
void tarefaRede(void*) {
  clienteHttps.setInsecure();  // protótipo: sem validar o certificado (ver README, "Segurança")
  Mensagem msg;
  uint8_t tentativas = 0;
  bool estavaOffline = false;
  for (;;) {
    if (!redeHabilitada) {
      if (!estavaOffline) {
        WiFi.disconnect(true);
        estadoRede = REDE_DESLIGADA;
        estavaOffline = true;
        logMsg("REDE", "SEM INTERNET. O alarme local continua; os dados ficam guardados na fila.");
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    if (estavaOffline) {
      estavaOffline = false;
      logMsg("REDE", "Internet de volta. %u mensagens para sincronizar.", mensagensPendentes());
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (!conectarWiFi()) esperarRede(3000);
      continue;
    }
    estadoRede = REDE_ONLINE;
    if (!enviandoAgora) {
      if (xQueueReceive(filaEnvio, &msg, pdMS_TO_TICKS(200)) != pdTRUE) continue;
      enviandoAgora = true;
    }
    int codigo = postar(msg);
    if (codigo >= 200 && codigo < 300) {           // entregue
      enviandoAgora = false; tentativas = 0; totalEnviadas = totalEnviadas + 1;
    } else if (codigo >= 400 && codigo < 500) {    // a API recusou: reenviar não adianta
      enviandoAgora = false; tentativas = 0; totalDescartadas = totalDescartadas + 1;
      logMsg("REDE", "API recusou (HTTP %d). Mensagem descartada para nao travar a fila.", codigo);
    } else {                                       // falha de rede ou 5xx: tenta de novo
      tentativas++;
      uint32_t espera = min<uint32_t>(30000, 1000UL << min<uint8_t>(tentativas, 5));
      logMsg("REDE", "Nova tentativa em %lu s (a mensagem continua guardada)", (unsigned long)(espera / 1000));
      esperarRede(espera);
    }
  }
}

// ============================================================================
//  8. BUZZER  (sons sem delay(): o resto do programa continua rodando)
// ============================================================================
const Nota SOM_INICIO[]   = { {784, 100}, {1047, 160}, {0, 0} };
const Nota SOM_BIP[]      = { {2093, 80}, {0, 0} };
const Nota SOM_LEMBRETE[] = { {1319, 140}, {0, 90}, {1568, 200}, {0, 0} };
const Nota SOM_PERIGO[]   = { {1760, 220}, {0, 140}, {1760, 220}, {0, 140}, {1760, 220}, {0, 0} };
const Nota SOM_SIRENE[]   = { {988, 350}, {740, 350}, {0, 0} };
const Nota SOM_OK[]       = { {1047, 90}, {1319, 90}, {1568, 180}, {0, 0} };

const Nota* somAtual = nullptr;
uint8_t     notaAtual = 0;
uint32_t    notaInicioMs = 0;
bool        somEmLoop = false;

void aplicarNota() {
  uint16_t f = somAtual[notaAtual].freq;
  if (f) tone(PINO_BUZZER, f); else noTone(PINO_BUZZER);
}

void tocar(const Nota* som, bool emLoop = false) {
  if (somAtual == SOM_SIRENE && som != SOM_SIRENE) return;  // a sirene tem prioridade
  somAtual = som; somEmLoop = emLoop; notaAtual = 0; notaInicioMs = millis();
  aplicarNota();
}

void pararSom() { somAtual = nullptr; noTone(PINO_BUZZER); }

void atualizarBuzzer() {
  if (checagemAtiva && somAtual != SOM_SIRENE) tocar(SOM_SIRENE, true);
  if (!checagemAtiva && somAtual == SOM_SIRENE) pararSom();
  if (!somAtual || millis() - notaInicioMs < somAtual[notaAtual].ms) return;
  notaAtual++;
  notaInicioMs = millis();
  if (somAtual[notaAtual].ms == 0) {
    if (!somEmLoop) { pararSom(); return; }
    notaAtual = 0;
  }
  aplicarNota();
}

// ============================================================================
//  9. LED RGB  (a cor é o nível; piscar chama atenção)
// ============================================================================
void corLed(uint8_t r, uint8_t g, uint8_t b) {
  static int16_t ultR = -1, ultG = -1, ultB = -1;
  if (r == ultR && g == ultG && b == ultB) return;
  analogWrite(PINO_LED_R, r); analogWrite(PINO_LED_G, g); analogWrite(PINO_LED_B, b);
  ultR = r; ultG = g; ultB = b;
}

void atualizarLed() {
  uint32_t ms = millis();
  if (amb.estado == SENSOR_AGUARDANDO) { corLed(0, 0, 0); return; }
  if (amb.estado == SENSOR_FALHA)      { (ms / 500) % 2 ? corLed(160, 0, 255) : corLed(0, 0, 0); return; }
  if (checagemAtiva)                   { (ms / 200) % 2 ? corLed(255, 0, 0) : corLed(0, 0, 0); return; }
  if (nivelAtual == PERIGO_EXTREMO)    { (ms / 500) % 2 ? corLed(255, 0, 0) : corLed(40, 0, 0); return; }
  if (lembreteAtivo && (ms / 700) % 2) { corLed(0, 90, 255); return; }  // azul = água
  const uint8_t* c = COR_NIVEL[nivelAtual];
  corLed(c[0], c[1], c[2]);
}

// ============================================================================
//  10. DISPLAY OLED  (ícones desenhados, texto curto e grande)
// ============================================================================
constexpr uint16_t BRANCO = SSD1306_WHITE;
constexpr uint16_t PRETO  = SSD1306_BLACK;

struct MensagemTela { Icone icone; char titulo[11]; char linha1[22]; char linha2[22]; uint32_t ateMs; } msgTela = {};

void mostrarMensagem(Icone icone, const char* titulo, const char* l1, const char* l2, uint32_t duracaoMs) {
  msgTela.icone = icone;
  strlcpy(msgTela.titulo, titulo, sizeof msgTela.titulo);
  strlcpy(msgTela.linha1, l1, sizeof msgTela.linha1);
  strlcpy(msgTela.linha2, l2, sizeof msgTela.linha2);
  msgTela.ateMs = millis() + duracaoMs;
}

void textoCentralizado(const char* s, int16_t y, uint8_t tamanho, uint16_t cor = BRANCO) {
  int16_t x1, y1; uint16_t w, h;
  tela.setTextSize(tamanho);
  tela.setTextColor(cor);
  tela.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  tela.setCursor((128 - (int16_t)w) / 2, y);
  tela.print(s);
}

void iconeGota(int16_t x, int16_t y, uint16_t cor = BRANCO) {           // 7x9
  tela.fillTriangle(x + 3, y, x, y + 5, x + 6, y + 5, cor);
  tela.fillCircle(x + 3, y + 5, 3, cor);
}

void iconeGotaGrande(int16_t cx, int16_t cy, int16_t r) {
  tela.fillTriangle(cx, cy - 2 * r, cx - r, cy - 2, cx + r, cy - 2, BRANCO);
  tela.fillCircle(cx, cy, r, BRANCO);
  tela.fillCircle(cx - r / 2, cy + 1, 2, PRETO);                     // brilho
}

void iconePessoa(int16_t x, int16_t y, uint16_t cor = BRANCO) {          // 7x9
  tela.fillCircle(x + 3, y + 1, 1, cor);
  tela.drawLine(x + 3, y + 3, x + 3, y + 6, cor);
  tela.drawLine(x + 1, y + 4, x + 5, y + 4, cor);
  tela.drawLine(x + 3, y + 6, x + 1, y + 8, cor);
  tela.drawLine(x + 3, y + 6, x + 5, y + 8, cor);
}

void iconeTermometro(int16_t x, int16_t y, uint16_t cor = BRANCO) {      // 5x9
  tela.drawRoundRect(x + 1, y, 3, 7, 1, cor);
  tela.fillCircle(x + 2, y + 7, 2, cor);
}

void iconeAlerta(int16_t cx, int16_t y, uint16_t cor = BRANCO) {         // 21x18
  tela.drawTriangle(cx, y, cx - 10, y + 17, cx + 10, y + 17, cor);
  tela.drawTriangle(cx, y + 2, cx - 8, y + 16, cx + 8, y + 16, cor);
  tela.fillRect(cx - 1, y + 6, 2, 6, cor);
  tela.fillRect(cx - 1, y + 13, 2, 2, cor);
}

void iconeOk(int16_t cx, int16_t y, uint16_t cor = BRANCO) {             // 21x18
  tela.drawCircle(cx, y + 9, 9, cor);
  for (int8_t d = 0; d < 2; d++) {
    tela.drawLine(cx - 5, y + 9 + d, cx - 2, y + 12 + d, cor);
    tela.drawLine(cx - 2, y + 12 + d, cx + 5, y + 5 + d, cor);
  }
}

void iconeWifi(int16_t x, int16_t y) {                                   // 11x8
  EstadoRede e = estadoRede;
  uint8_t barras = e == REDE_ONLINE ? 4 : (e == REDE_CONECTANDO ? (millis() / 300) % 5 : 0);
  for (uint8_t i = 0; i < 4; i++) {
    int16_t h = 2 + 2 * i;
    if (i < barras) tela.fillRect(x + i * 3, y + 8 - h, 2, h, BRANCO);
    else            tela.drawPixel(x + i * 3, y + 7, BRANCO);
  }
  if (e == REDE_DESLIGADA) {
    tela.drawLine(x + 1, y, x + 6, y + 5, BRANCO);
    tela.drawLine(x + 6, y, x + 1, y + 5, BRANCO);
  }
}

void barraStatus() {
  struct tm tm;
  horaLocal(relogio(), &tm);
  char hora[6];
  snprintf(hora, sizeof hora, "%02d:%02d", tm.tm_hour, tm.tm_min);
  tela.setTextSize(1);
  tela.setTextColor(BRANCO);
  tela.setCursor(0, 0);
  tela.print(hora);
  if (modoVigilancia) {
    tela.fillRoundRect(34, 0, 32, 9, 2, BRANCO);
    tela.setTextColor(PRETO);
    tela.setCursor(37, 1);
    tela.print("VIGIL");
    tela.setTextColor(BRANCO);
  }
  uint16_t pendentes = mensagensPendentes();
  if (pendentes > 0) {  // seta + número = mensagens esperando para subir
    char txt[6];
    snprintf(txt, sizeof txt, "%u", pendentes);
    int16_t x = 113 - 6 * strlen(txt);
    tela.fillTriangle(x - 5, 3, x - 8, 6, x - 2, 6, BRANCO);
    tela.drawLine(x - 5, 6, x - 5, 8, BRANCO);
    tela.setCursor(x, 1);
    tela.print(txt);
  }
  iconeWifi(117, 0);
  tela.drawFastHLine(0, 10, 128, BRANCO);
}

void telaInicial(const char* status) {
  if (!telaOk) return;
  tela.clearDisplay();
  tela.drawRoundRect(8, 10, 7, 24, 3, BRANCO);    // termômetro
  tela.fillRect(10, 18, 3, 14, BRANCO);
  tela.fillCircle(11, 36, 5, BRANCO);
  tela.setTextSize(2);
  tela.setTextColor(BRANCO);
  tela.setCursor(24, 12);
  tela.print("HeatSafe");
  tela.setTextSize(1);
  tela.setCursor(24, 31);
  tela.print("monitor de calor");
  tela.drawFastHLine(0, 47, 128, BRANCO);
  textoCentralizado(status, 53, 1);
  tela.display();
}

void telaPrincipal(uint32_t agora) {
  barraStatus();
  // Índice de calor (sensação térmica) em destaque
  char num[8];
  if (amb.indiceCalor < 99.95f) snprintf(num, sizeof num, "%.1f", amb.indiceCalor);
  else                          snprintf(num, sizeof num, "%.0f", amb.indiceCalor);
  tela.setTextSize(3);
  tela.setTextColor(BRANCO);
  tela.setCursor(0, 14);
  tela.print(num);
  int16_t xg = tela.getCursorX();
  tela.drawCircle(xg + 3, 16, 2, BRANCO);
  tela.setTextSize(2);
  tela.setCursor(xg + 7, 14);
  tela.print("C");
  // Temperatura e umidade medidas
  char txt[12];
  tela.setTextSize(1);
  iconeTermometro(97, 14);
  snprintf(txt, sizeof txt, "%.1f", amb.temperatura);
  tela.setCursor(104, 15);
  tela.print(txt);
  iconeGota(96, 26);
  snprintf(txt, sizeof txt, "%.0f%%", amb.umidade);
  tela.setCursor(104, 28);
  tela.print(txt);
  // Faixa do nível; acima de Normal, alterna com a dica do momento
  const char* faixa = NIVEL_TELA[nivelAtual];
  if (nivelAtual >= ATENCAO && (millis() / 2500) % 2) faixa = DICAS[(millis() / 5000) % TOTAL_DICAS];
  if (nivelAtual == NORMAL) {
    tela.drawRoundRect(0, 40, 128, 12, 3, BRANCO);
    textoCentralizado(faixa, 42, 1, BRANCO);
  } else {
    tela.fillRoundRect(0, 40, 128, 12, 3, BRANCO);
    textoCentralizado(faixa, 42, 1, PRETO);
  }
  // Rodapé: há quanto tempo sem movimento e sem água
  char dur[10];
  tela.setTextColor(BRANCO);
  iconePessoa(0, 55);
  formatarDuracao(agora - ultimoMovimentoSeg, dur, sizeof dur);
  tela.setCursor(10, 56);
  tela.print(dur);
  iconeGota(48, 55);
  formatarDuracao(agora - ultimaAguaSeg, dur, sizeof dur);
  tela.setCursor(58, 56);
  tela.print(dur);
  snprintf(txt, sizeof txt, "%ux", hoje.copos);  // copos de água hoje
  tela.setCursor(122 - 6 * strlen(txt), 56);
  tela.print(txt);
  iconeGota(121, 55);
}

void telaResumo() {
  tela.fillRect(0, 0, 128, 11, BRANCO);
  textoCentralizado("RESUMO DE HOJE", 2, 1, PRETO);
  char txt[32];
  tela.setTextSize(1);
  tela.setTextColor(BRANCO);
  iconeGota(1, 14);
  snprintf(txt, sizeof txt, "Agua: %u copo%s", hoje.copos, hoje.copos == 1 ? "" : "s");
  tela.setCursor(13, 15); tela.print(txt);
  tela.drawCircle(4, 30, 3, BRANCO); tela.drawPixel(4, 30, BRANCO);
  snprintf(txt, sizeof txt, "Lembretes: %u de %u", hoje.lembretesAtendidos, hoje.lembretes);
  tela.setCursor(13, 27); tela.print(txt);
  iconePessoa(1, 38);
  snprintf(txt, sizeof txt, "Movimentos: %u", hoje.movimentos);
  tela.setCursor(13, 39); tela.print(txt);
  iconeTermometro(2, 50);
  if (isnan(hoje.icMax)) snprintf(txt, sizeof txt, "Pior calor: --");
  else                   snprintf(txt, sizeof txt, "Pior calor: %.1f C", hoje.icMax);
  tela.setCursor(13, 51); tela.print(txt);
}

void telaLembrete() {
  bool pulso = (millis() / 500) % 2;
  iconeGotaGrande(22, 38, pulso ? 13 : 12);
  tela.setTextColor(BRANCO);
  tela.setTextSize(1);
  tela.setCursor(46, 6);  tela.print("HORA DE");
  tela.setTextSize(2);
  tela.setCursor(46, 18); tela.print("BEBER");
  tela.setCursor(46, 36); tela.print("AGUA");
  tela.setTextSize(1);
  tela.setCursor(44, 56); tela.print("aperte o botao");
}

void telaChecagem(uint32_t agora) {
  bool invertido = (millis() / 500) % 2;
  uint16_t frente = invertido ? PRETO : BRANCO;
  if (invertido) tela.fillScreen(BRANCO);
  textoCentralizado(motivoChecagem, 2, 1, frente);
  textoCentralizado("VOCE ESTA", 16, 2, frente);
  textoCentralizado("BEM?", 34, 2, frente);
  if (!familiaAvisada) {
    char txt[22];
    uint32_t passou = agora - checagemDesdeSeg, janela = JANELA_RESPOSTA_MIN * 60UL;
    uint32_t faltaMin = passou >= janela ? 0 : (janela - passou + 59) / 60;
    snprintf(txt, sizeof txt, (millis() / 2000) % 2 ? "Aperte o botao" : "Aviso familia: %lumin", (unsigned long)faltaMin);
    textoCentralizado(txt, 55, 1, frente);
  } else {
    textoCentralizado((millis() / 2000) % 2 ? "Aperte o botao" : "FAMILIA AVISADA", 55, 1, frente);
  }
}

void telaMensagem() {
  if (msgTela.icone == ICONE_OK)          iconeOk(64, 1);
  else if (msgTela.icone == ICONE_ALERTA) iconeAlerta(64, 1);
  else                                    iconeGotaGrande(64, 14, 6);
  textoCentralizado(msgTela.titulo, 23, 2);
  textoCentralizado(msgTela.linha1, 44, 1);
  textoCentralizado(msgTela.linha2, 55, 1);
}

void telaErroSensor() {
  barraStatus();
  iconeAlerta(64, 13);
  textoCentralizado("SENSOR SEM LEITURA", 37, 1);
  textoCentralizado("Verifique o DHT22", 48, 1);
}

void atualizarTela(uint32_t agora) {
  static uint32_t ultimaMs = 0;
  if (!telaOk || millis() - ultimaMs < 100) return;
  ultimaMs = millis();
  if (amb.estado == SENSOR_AGUARDANDO) return;  // a tela de abertura continua
  tela.clearDisplay();
  uint32_t s = millis() / 1000;
  if ((int32_t)(msgTela.ateMs - millis()) > 0)       telaMensagem();
  else if (checagemAtiva)                            telaChecagem(agora);
  else if (lembreteAtivo && s % 6 < 4)               telaLembrete();
  else if (amb.estado == SENSOR_FALHA)               telaErroSensor();
  else if (!lembreteAtivo && s % 20 >= 16)           telaResumo();
  else                                               telaPrincipal(agora);
  tela.display();
}

// ============================================================================
//  11. REGRAS: lembrete de água, checagem de inatividade e escalonamento
// ============================================================================
void iniciarChecagem(uint32_t agora, const char* motivo) {
  checagemAtiva = true;
  familiaAvisada = false;
  checagemDesdeSeg = agora;
  strlcpy(motivoChecagem, motivo, sizeof motivoChecagem);
  hoje.alarmes++;
  logMsg("ALERTA", "%s | indice de calor %.1f C (%s). Alarme tocando, aguardando resposta por %u min.",
         motivo, amb.indiceCalor, NIVEL_TELA[nivelAtual], JANELA_RESPOSTA_MIN);
}

void resolverChecagem(uint32_t agora, const char* origem) {
  checagemAtiva = false;
  ultimaRespostaSeg = ultimoMovimentoSeg = ultimoEventoMovSeg = agora;
  movimentoDesdeLeitura = true;
  logMsg("ALERTA", "A pessoa respondeu (%s). Alarme desligado.", origem);
  enviarEvento("MOVIMENTO");
  pararSom();
  tocar(SOM_OK);
  mostrarMensagem(ICONE_OK, "QUE BOM!", "Alarme desligado",
                  familiaAvisada ? "Familia sera avisada" : "Beba um copo de agua", 3500);
}

void mudarNivel(Nivel novo, uint32_t agora) {
  Nivel antigo = nivelAtual;
  nivelAtual = novo;
  leituraUrgente = true;  // a API fica sabendo na hora, sem esperar o próximo ciclo
  logMsg("NIVEL", "%s -> %s (indice de calor %.1f C)", NIVEL_TELA[antigo], NIVEL_TELA[novo], amb.indiceCalor);
  if (novo > antigo) {
    if (novo == CAUTELA_EXTREMA) {
      tocar(SOM_BIP);
      mostrarMensagem(ICONE_ALERTA, "ATENCAO", "A casa esquentou", "Beba agua sempre", 3000);
    } else if (novo == PERIGO) {
      tocar(SOM_PERIGO);
      mostrarMensagem(ICONE_ALERTA, "CUIDADO!", "Casa muito quente", "Beba agua agora", 3500);
    } else if (novo == PERIGO_EXTREMO && !checagemAtiva) {
      iniciarChecagem(agora, "CALOR EXTREMO");
    }
  } else if (novo < CAUTELA_EXTREMA && checagemAtiva) {
    checagemAtiva = false;
    logMsg("ALERTA", "O calor baixou para %s: checagem encerrada.", NIVEL_TELA[novo]);
  }
}

void avaliarRegras(uint32_t agora) {
  if (amb.estado != SENSOR_OK) return;

  Nivel novo = classificarComHisterese(amb.indiceCalor, nivelAtual);
  if (novo != nivelAtual) mudarNivel(novo, agora);

  // Lembrete de água: 2h em Atenção, 1h em Cautela extrema ou pior (1h também em Atenção
  // quando a API liga o modo vigilância).
  uint16_t intervalo = LEMBRETE_AGUA_MIN[nivelAtual];
  if (modoVigilancia && nivelAtual == ATENCAO) intervalo = 60;
  if (intervalo == 0) {
    lembreteAtivo = false;
  } else if (!lembreteAtivo && agora - ultimaAguaSeg >= intervalo * 60UL) {
    lembreteAtivo = true;
    ultimoBipLembreteSeg = agora;
    hoje.lembretes++;
    tocar(SOM_LEMBRETE);
    char dur[10];
    formatarDuracao(agora - ultimaAguaSeg, dur, sizeof dur);
    logMsg("AGUA", "Lembrete de agua: %s sem beber em %s.", dur, NIVEL_TELA[nivelAtual]);
#if ENVIAR_LEMBRETES_PARA_API
    enviarEvento("LEMBRETE_AGUA");
#endif
  } else if (lembreteAtivo && agora - ultimoBipLembreteSeg >= REPETIR_LEMBRETE_MIN * 60UL) {
    ultimoBipLembreteSeg = agora;
    tocar(SOM_LEMBRETE);
  }

  // Checagem de inatividade: 3h parado em Cautela extrema, 2h em Perigo.
  uint16_t limite = LIMITE_SEM_MOV_MIN[nivelAtual];
  uint32_t ultimaAtividade = max(ultimoMovimentoSeg, ultimaRespostaSeg);
  if (!checagemAtiva && limite && agora - ultimaAtividade >= limite * 60UL) {
    char dur[10], motivo[28];
    formatarDuracao(agora - ultimoMovimentoSeg, dur, sizeof dur);
    snprintf(motivo, sizeof motivo, "Sem movimento ha %s", dur);
    iniciarChecagem(agora, motivo);
  }

  // Escalonamento: alarme tocando e ninguém respondeu -> a API avisa a família (e depois a UBS).
  if (checagemAtiva && !familiaAvisada && agora - checagemDesdeSeg >= JANELA_RESPOSTA_MIN * 60UL) {
    familiaAvisada = true;
    leituraUrgente = true;
    logMsg("ALERTA", "Ninguem respondeu em %u min. Avisando a API (familia; sem confirmacao, UBS).", JANELA_RESPOSTA_MIN);
    enviarEvento("ALARME_SEM_RESPOSTA");
  }

  // Perigo extremo: mesmo depois da resposta, o aviso sonoro volta a cada 5 min.
  if (nivelAtual == PERIGO_EXTREMO && !checagemAtiva && agora - ultimoBipExtremoSeg >= BIP_EXTREMO_MIN * 60UL) {
    ultimoBipExtremoSeg = agora;
    tocar(SOM_PERIGO);
  }
}

// ============================================================================
//  12. ENTRADAS: sensor, presença, botão e chave de internet
// ============================================================================
void lerSensorAmbiente() {
  static uint32_t ultimaMs = 0;
  if (ultimaMs != 0 && millis() - ultimaMs < LEITURA_SENSOR_MS) return;
  ultimaMs = millis() | 1;
  TempAndHumidity d = dht.getTempAndHumidity();
  bool valido = dht.getStatus() == DHTesp::ERROR_NONE && !isnan(d.temperature) && !isnan(d.humidity)
                && d.temperature >= -40 && d.temperature <= 80 && d.humidity >= 0 && d.humidity <= 100;
  if (!valido) {
    if (++amb.falhas >= 3 && amb.estado != SENSOR_FALHA) {
      amb.estado = SENSOR_FALHA;
      logMsg("SENSOR", "DHT22 sem leitura valida (%u tentativas). Leituras suspensas.", amb.falhas);
    }
    return;
  }
  bool mudou = amb.estado != SENSOR_OK || fabsf(d.temperature - amb.temperatura) >= 0.1f
               || fabsf(d.humidity - amb.umidade) >= 0.5f;
  amb.falhas = 0;
  amb.estado = SENSOR_OK;
  amb.temperatura = d.temperature;
  amb.umidade = d.humidity;
  amb.indiceCalor = indiceDeCalor(d.temperature, d.humidity);
  if (isnan(hoje.icMax) || amb.indiceCalor > hoje.icMax) hoje.icMax = amb.indiceCalor;
  if (mudou)
    logMsg("SENSOR", "Temperatura %.1f C | Umidade %.0f%% | Indice de calor %.1f C",
           amb.temperatura, amb.umidade, amb.indiceCalor);
}

void registrarMovimento(uint32_t agora) {
  hoje.movimentos++;
  if (checagemAtiva) { resolverChecagem(agora, "movimento"); return; }
  char dur[10];
  formatarDuracao(agora - ultimoMovimentoSeg, dur, sizeof dur);
  logMsg("PRESENCA", "Movimento detectado (parado ha %s).", dur);
  ultimoMovimentoSeg = agora;
  movimentoDesdeLeitura = true;
  if (ultimoEventoMovSeg == 0 || agora - ultimoEventoMovSeg >= INTERVALO_EVENTO_MOV_MIN * 60UL) {
    ultimoEventoMovSeg = agora;
    enviarEvento("MOVIMENTO");
  }
}

void lerPresenca(uint32_t agora) {
  static bool anterior = false;
  bool atual = digitalRead(PINO_PIR) == HIGH;
  if (atual && !anterior) registrarMovimento(agora);
  anterior = atual;
}

// Um botão só (pedido da Dona Maria): com alarme tocando ele é "Estou bem";
// no resto do tempo ele é "Bebi água".
void aoApertarBotao(uint32_t agora) {
  if (checagemAtiva) { resolverChecagem(agora, "botao"); return; }
  bool eraLembrete = lembreteAtivo;
  ultimaAguaSeg = ultimoMovimentoSeg = agora;  // apertar o botão também prova presença
  movimentoDesdeLeitura = true;
  lembreteAtivo = false;
  hoje.copos++;
  if (eraLembrete) hoje.lembretesAtendidos++;
  logMsg("AGUA", "Botao \"Bebi agua\": %u copo(s) hoje, %u de %u lembretes atendidos.",
         hoje.copos, hoje.lembretesAtendidos, hoje.lembretes);
  enviarEvento("AGUA");
  tocar(SOM_OK);
  mostrarMensagem(ICONE_GOTA, "OBRIGADO!", "Agua registrada",
                  eraLembrete ? "Lembrete atendido" : "Continue assim", 2500);
}

void lerBotao(uint32_t agora) {
  static bool estavel = HIGH, ultimoLido = HIGH;
  static uint32_t mudouMs = 0;
  bool lido = digitalRead(PINO_BOTAO);
  if (lido != ultimoLido) { ultimoLido = lido; mudouMs = millis(); }
  if (millis() - mudouMs > 40 && lido != estavel) {  // debounce de 40 ms
    estavel = lido;
    if (estavel == LOW) aoApertarBotao(agora);
  }
}

void lerChaveRede() {
  redeHabilitada = digitalRead(PINO_CHAVE_REDE) == HIGH;
}

void enviarLeituraSeHora(uint32_t agora) {
  if (amb.estado != SENSOR_OK) return;
  if (!leituraUrgente && agora < proximaLeituraSeg) return;
  leituraUrgente = false;
  proximaLeituraSeg = agora + INTERVALO_LEITURA_MIN * 60UL;
  enviarLeitura();
}

// Zera o resumo à meia-noite e registra o comportamento a cada hora (dados de IoB).
void cuidarDoCalendario() {
  static int ultimaHora = -1;
  struct tm tm;
  horaLocal(relogio(), &tm);
  if (tm.tm_yday != hoje.dia) {
    hoje = ResumoDia();
    hoje.dia = tm.tm_yday;
  }
  if (tm.tm_hour != ultimaHora) {
    if (ultimaHora >= 0)
      logMsg("IoB", "Resumo: %u copo(s) | lembretes atendidos %u/%u | %u movimentos | %u alarme(s) | pior IC %.1f C | API: %lu enviadas",
             hoje.copos, hoje.lembretesAtendidos, hoje.lembretes, hoje.movimentos, hoje.alarmes,
             isnan(hoje.icMax) ? 0.0f : hoje.icMax, (unsigned long)totalEnviadas);
    ultimaHora = tm.tm_hour;
  }
}

// ============================================================================
//  13. SETUP E LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  mutexLog = xSemaphoreCreateMutex();

  pinMode(PINO_PIR, INPUT);
  pinMode(PINO_BOTAO, INPUT_PULLUP);
  pinMode(PINO_CHAVE_REDE, INPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  corLed(0, 0, 0);

  Wire.begin(PINO_SDA, PINO_SCL);
  telaOk = tela.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  telaInicial("Iniciando...");

  Serial.println();
  Serial.println(F("=================================================================="));
  Serial.println(F("  HeatSafe | dispositivo " DISPOSITIVO_ID " | firmware 1.0"));
  Serial.println(F("  Calor dentro de casa + presenca + hidratacao, sem camera e sem microfone"));
#if MODO_DEMO
  Serial.println(F("  MODO DEMO: 1 s real = 2 min na casa. Relogio comeca em 12/11/2026 10:50"));
#endif
  Serial.println(F("  API: " API_BASE_URL));
  Serial.println(F("=================================================================="));
  if (!telaOk) logMsg("TELA", "OLED nao encontrado; o resto do dispositivo funciona normalmente.");

  dht.setup(PINO_DHT, DHTesp::DHT22);
  bool autotesteOk = autotesteIndiceCalor();
  telaInicial(autotesteOk ? "Autoteste OK" : "Autoteste FALHOU");

  filaEnvio = xQueueCreate(TAMANHO_FILA, sizeof(Mensagem));
  redeHabilitada = digitalRead(PINO_CHAVE_REDE) == HIGH;
  // Prioridade 0 (a mesma da tarefa ociosa) de propósito: o handshake HTTPS é pesado e, no
  // simulador, ocupa o núcleo 0 por vários segundos. Dividindo o núcleo com a tarefa ociosa,
  // o watchdog continua sendo alimentado e o ESP32 não reinicia no meio do envio.
  xTaskCreatePinnedToCore(tarefaRede, "rede", 12288, nullptr, 0, nullptr, 0);
  delay(1500);  // deixa a tela de abertura visível enquanto o Wi-Fi conecta

  ultimoMovimentoSeg = ultimaAguaSeg = ultimaRespostaSeg = agoraSeg();
  tocar(SOM_INICIO);
}

void loop() {
  uint32_t agora = agoraSeg();
  cuidarDoCalendario();
  lerChaveRede();
  lerSensorAmbiente();
  lerPresenca(agora);
  lerBotao(agora);
  avaliarRegras(agora);
  enviarLeituraSeHora(agora);
  atualizarLed();
  atualizarBuzzer();
  atualizarTela(agora);
  delay(10);
}

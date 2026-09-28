#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Adafruit_Fingerprint.h>

// =============================================================
//               CONFIGURAÇÕES E BANCO DE DADOS
// =============================================================

const char* ssid = "NOME_DA_SUA_REDE";
const char* password = "SUA_SENHA_AQUI";
String GOOGLE_SCRIPT_ID = "URL_DO_SEU_SCRIPT";

// --- PINOS ---
const int btnCadastro = 5;  
const int ledStatus   = 2;  

// --- SENSOR (Pinos 32 e 33) ---
#define PINO_RX_DO_ESP 32  
#define PINO_TX_DO_ESP 33  

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

int idParaCadastro = 1; 

// --- ESTRUTURA PARA LEMBRAR DOS NOMES (MEMÓRIA RAM) ---
struct Aluno {
  String nome;
  String matricula;
  bool cadastrado;
};

// Cria espaço para lembrar de até 127 alunos (limite do sensor)
Aluno bancoDeDados[128]; 

// Variáveis temporárias
String nomeTemp = "";
String matTemp = "";

// =============================================================
//                     SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(btnCadastro, INPUT_PULLUP);
  pinMode(ledStatus, OUTPUT);

  imprimirCabecalho();

  // 1. WiFi
  Serial.print("📡 Conectando WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK! ✅");

  // 2. Sensor
  mySerial.begin(57600, SERIAL_8N1, PINO_RX_DO_ESP, PINO_TX_DO_ESP);
  finger.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println("✅ Sensor Online!");
  } else {
    mySerial.begin(9600, SERIAL_8N1, PINO_RX_DO_ESP, PINO_TX_DO_ESP);
    finger.begin(9600);
    if (!finger.verifyPassword()) {
       Serial.println("❌ Sensor não encontrado.");
       while (1);
    }
  }

  // Atualiza ID
  finger.getTemplateCount();
  if (finger.templateCount > 0) {
    idParaCadastro = finger.templateCount + 1;
  }
  
  Serial.print("📊 Digitais no sensor: "); Serial.println(finger.templateCount);
  Serial.println("🚀 SISTEMA PRONTO!");
  Serial.println("💡 DICA: Digite 'LIMPAR' para formatar.");
  linhaSeparadora();
}

// =============================================================
//                        LOOP
// =============================================================
void loop() {
  verificarComandoSecreto();

  if (digitalRead(btnCadastro) == LOW) {
    modoCadastramentoInterativo();
  }

  verificarPresenca();
  delay(50);
}

// =============================================================
//              CADASTRO COMPLETO (Corrige Prob 1 e 2)
// =============================================================
void modoCadastramentoInterativo() {
  linhaSeparadora();
  Serial.println("📝 NOVO REGISTRO");
  
  while(Serial.available()) Serial.read(); // Limpa sujeira do teclado

  // 1. Pega NOME
  Serial.println("👉 Digite o NOME e dê Enter:");
  while (Serial.available() == 0) { delay(100); }
  nomeTemp = Serial.readStringUntil('\n');
  nomeTemp.trim();
  Serial.print("   Nome: "); Serial.println(nomeTemp);

  // 2. Pega MATRÍCULA
  Serial.println("👉 Digite a MATRÍCULA e dê Enter:");
  while (Serial.available() == 0) { delay(100); }
  matTemp = Serial.readStringUntil('\n');
  matTemp.trim();
  Serial.print("   Matrícula: "); Serial.println(matTemp);

  // 3. Grava Digital
  Serial.print("\n🔒 Gravando no ID #"); Serial.println(idParaCadastro);
  while (!getFingerprintEnroll(idParaCadastro)); 
  
  // 4. SALVA NA MEMÓRIA DO ESP32 (Para mostrar na presença depois)
  if (idParaCadastro < 128) {
    bancoDeDados[idParaCadastro].nome = nomeTemp;
    bancoDeDados[idParaCadastro].matricula = matTemp;
    bancoDeDados[idParaCadastro].cadastrado = true;
  }

  // 5. Envia para o Google (Tentativa Robusta)
  enviarDadosCompletos(idParaCadastro, nomeTemp, matTemp);
  
  idParaCadastro++; 
  Serial.println("✅ Cadastro Finalizado!");
  linhaSeparadora();
  delay(2000);
}

// =============================================================
//              PRESENÇA INTELIGENTE (Corrige Prob 2)
// =============================================================
void verificarPresenca() {
  int idEncontrado = getFingerprintID();
  
  if (idEncontrado > 0) {
    // AQUI ESTÁ A MÁGICA: Recupera o nome da memória RAM
    String nomeExibicao = "Aluno_ID_" + String(idEncontrado);
    String matriculaExibicao = "-";

    // Se tivermos esse aluno na memória RAM, usamos os dados dele
    if (idEncontrado < 128 && bancoDeDados[idEncontrado].cadastrado == true) {
      nomeExibicao = bancoDeDados[idEncontrado].nome;
      matriculaExibicao = bancoDeDados[idEncontrado].matricula;
    }

    linhaSeparadora();
    Serial.println("✅ PRESENÇA CONFIRMADA!");
    Serial.print("👤 Nome: "); Serial.println(nomeExibicao);
    Serial.print("🔢 Matrícula: "); Serial.println(matriculaExibicao);
    Serial.print("🆔 ID Sensor: #"); Serial.println(idEncontrado);
    
    digitalWrite(ledStatus, HIGH);
    
    // Envia o nome real e a matrícula real para a planilha de novo (Log de Presença)
    enviarDadosCompletos(idEncontrado, nomeExibicao, matriculaExibicao);
    
    delay(1500);
    digitalWrite(ledStatus, LOW);
    linhaSeparadora();
  
  } else if (idEncontrado == -2) {
    Serial.println("🚨 ACESSO NEGADO (Digital não cadastrada)");
    for(int i=0; i<5; i++){ digitalWrite(ledStatus, HIGH); delay(50); digitalWrite(ledStatus, LOW); delay(50); }
    // enviarDadosCompletos(0, "ACESSO_NEGADO", "000"); // Opcional
    delay(2000);
  }
}

// =============================================================
//              FUNÇÃO DE ENVIO (Corrige Prob 1)
// =============================================================
void enviarDadosCompletos(int id, String nome, String matricula) {
  if(WiFi.status() == WL_CONNECTED){
    WiFiClientSecure client;
    client.setInsecure(); // Ignora SSL
    HTTPClient http;
    
    // Tratamento de espaços para URL (Carlos Silva -> Carlos%20Silva)
    nome.replace(" ", "%20");
    matricula.replace(" ", "%20");

    // Montagem da URL - Verifique se está igual ao Script
    String url = GOOGLE_SCRIPT_ID + "?id=" + String(id) + "&nome=" + nome + "&matricula=" + matricula;
    
    Serial.print("☁️ Enviando para Planilha... ");
    // Serial.println(url); // Descomente se quiser ver a URL gerada
    
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = http.GET();
    
    if(httpCode == 200) {
      Serial.println("Registrado! ✅");
    } else {
      Serial.print("Falha HTTP: "); Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("❌ Erro: Sem WiFi.");
  }
}

// --- Driver Sensor (Padrão) ---
int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) return finger.fingerID;
  if (p == FINGERPRINT_NOTFOUND) return -2;
  return -1;
}

uint8_t getFingerprintEnroll(int id) {
  int p = -1;
  Serial.print("👉 Coloque o dedo");
  while (p != FINGERPRINT_OK) { p = finger.getImage(); delay(100); }
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return false;
  Serial.println(" -> Tire");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) { p = finger.getImage(); }
  Serial.print("👉 Mesmo dedo de novo");
  p = -1;
  while (p != FINGERPRINT_OK) { p = finger.getImage(); delay(100); }
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) return false;
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    p = finger.storeModel(id);
    if (p == FINGERPRINT_OK) { Serial.println(" -> OK! 💾"); return true; }
  }
  return false;
}

void verificarComandoSecreto() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.equalsIgnoreCase("LIMPAR")) {
      Serial.println("🗑️ FORMATANDO...");
      finger.emptyDatabase();
      // Limpa também a memória RAM
      for(int i=0; i<128; i++) bancoDeDados[i].cadastrado = false;
      idParaCadastro = 1;
      Serial.println("✅ FEITO.");
    }
  }
}

void linhaSeparadora() { Serial.println("---------------------------------"); }
void imprimirCabecalho() { Serial.println("\n### SISTEMA DE CHAMADA INTELIGENTE ###"); }
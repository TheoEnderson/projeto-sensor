#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <vector>
#include <string>
#include <Preferences.h>
#include "FS.h"
#include "LittleFS.h"
#include "rgb_lcd.h"
#include "site.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


using namespace std;

// --- CONFIGURAÇÕES ---
const char* ssid = "NOBODY";     // COLOQUE SEU WI-FI AQUI
const char* password = "100Pedro"; // COLOQUE SUA SENHA AQUI

// Google Script URL (Verifique se está correta)
String googleScriptURL = "https://script.google.com/macros/s/AKfycbxmB83yvBEI1IZteZOsjc6PiDAnDBOf8QeyuoACO5qaEZON1Uag6hcQ3FSwZmseK9sSYw/exec";


#define SDA 21
#define SCL 22
WebServer server(80);
rgb_lcd lcd;
Preferences preferences;
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);


struct Aluno {
  char nome[80];
  int matricula;           
}; 

vector<Aluno> lista; 
int idAtual = 1; 

// Variáveis de tempo
unsigned long tempoAnterior = 0;  
unsigned long intervalo = 10000; 

// --- PROTÓTIPOS (Para organizar) ---
int getIdByMatricula(int matricula);
void enviarPresencaGoogle(int id);

void salvarBancoVector() {
  File f = LittleFS.open("/banco_alunos.dat", "w");
  if (f) {
    for (const auto& aluno : lista) {
      f.write((uint8_t*)&aluno, sizeof(Aluno));
    }
    f.close();
    Serial.println("Banco salvo na Flash!");
  }
}

void carregarBancoVector() {
  File f = LittleFS.open("/banco_alunos.dat", "r");
  if (f) {
    lista.clear();
    Aluno aluno;
    while (f.read((uint8_t*)&aluno, sizeof(Aluno))) {
      lista.push_back(aluno);
    }
    f.close();
    Serial.println("Banco carregado! Total: " + String(lista.size()));
  }
}

void apagarBancoVector() {
  if(LittleFS.remove("/banco_alunos.dat")) {
    Serial.println("Arquivo apagado com sucesso!");
  } else {
    Serial.println("Falha ao apagar ou arquivo não existia.");
  }
  
  lista.clear();
  
  preferences.begin("sistema", false);
  preferences.putInt("proximo_id", 1);
  idAtual = 1;
  preferences.end();
  Serial.println("Sistema resetado para ID 1");
}

int getIdByMatricula(int matricula) {
  for (int i = 0; i < lista.size(); i++) {
    if (lista[i].matricula == matricula) {
      return i + 1; // O ID do sensor geralmente é index + 1
    }
  }
  return -1;
}

// ==========================================
//           FUNÇÕES DE REDE (GOOGLE)
// ==========================================

void enviarCadastroGoogle(String nome, int matricula, int id) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // aceita certificado HTTPS

  HTTPClient http;

  String url = googleScriptURL + "?acao=cadastro";
  url += "&id=" + String(id);
  url += "&nome=" + nome;
  url += "&matricula=" + String(matricula);

  url.replace(" ", "%20");

  http.begin(client, url);  // 👈 MUDANÇA AQUI
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.println("Cadastro enviado ao Google! Code: " + String(httpCode));
  } else {
    Serial.println("Erro ao enviar cadastro: " + http.errorToString(httpCode));
  }

  http.end();
}

void enviarPresencaGoogle(int id) {
  if(WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  // A ação é 'presenca' e mandamos o ID
  String url = googleScriptURL + "?acao=presenca&id=" + String(id);
  
  http.begin(url);
  int httpCode = http.GET(); // A URL já tem os parametros
  
  if (httpCode > 0) {
    Serial.println("Presença online registrada! ID: " + String(id));
  } else {
    Serial.println("Falha ao enviar presença online.");
  }
  http.end();
}

// ==========================================
//           SISTEMA OFFLINE
// ==========================================

void registrarOffline(int id) {
  File f = LittleFS.open("/registro_offline.dat", "a"); // 'a' de append (adicionar)
  if(f) {
    // Salvamos apenas o ID e Matrícula para facilitar
    Aluno alunoTemp;
    if(id > 0 && id <= lista.size()){
        alunoTemp = lista[id-1];
        f.write((uint8_t*)&alunoTemp, sizeof(Aluno));
        Serial.println("Sem Wi-Fi -> Salvo no Offline: " + String(alunoTemp.nome));
    }
    f.close();
  }
}

// Função para processar os dados offline quando a internet volta
void processarFilaOffline() {
  if(WiFi.status() != WL_CONNECTED) return;

  File f = LittleFS.open("/registro_offline.dat", "r");
  if(!f) return; // Se não tem arquivo, não faz nada

  Serial.println("Processando fila offline...");
  
  // Criamos um arquivo temporário para os que falharem (se houver)
  // Mas para simplificar, vamos ler tudo, tentar enviar, e apagar o arquivo original
  
  Aluno alunoTemp;
  bool sucessoTotal = true;

  while(f.read((uint8_t*)&alunoTemp, sizeof(Aluno))) {
    // Tenta encontrar o ID baseado na matricula recuperada
    int idRecuperado = getIdByMatricula(alunoTemp.matricula);
    
    if(idRecuperado != -1) {
        Serial.print("Enviando offline recuperado: "); Serial.println(alunoTemp.nome);
        enviarPresencaGoogle(idRecuperado);
        delay(500); // Pausa para não bloquear o Google
    }
  }
  f.close();
  
  // Se terminou de ler, apaga o arquivo de pendências
  LittleFS.remove("/registro_offline.dat");
  Serial.println("Fila offline processada e limpa.");
}


// ==========================================
//           CADASTRO WEB
// ==========================================

void handleCadastro() {
  String nome = server.arg("nome");
  String matricula = server.arg("matricula");

  Serial.println("Site -> Nome: " + nome);
  
  lcd.clear();
  lcd.print("Cadastrando...");
  lcd.setCursor(0, 1);
  lcd.print(nome);

  server.send(200, "text/html", "<h1>Olhe para o sensor!</h1><p>Siga as instrucoes no LCD.</p>");

  int p = -1; 
  Serial.println("Coloque o dedo (1/2)");
  
  // Espera Dedo 1
  while((p = finger.getImage()) != FINGERPRINT_OK) { delay(10); }
  finger.image2Tz(1);
  
  Serial.println("Tire o dedo");
  lcd.clear(); lcd.print("Tire o dedo");
  delay(1000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) { delay(10); }

  Serial.println("Coloque o dedo (2/2)");
  lcd.clear(); lcd.print("Confirme o dedo");
  
  // Espera Dedo 2
  while ((p = finger.getImage()) != FINGERPRINT_OK) { delay(10); }
  finger.image2Tz(2);
  
  if (finger.createModel() == FINGERPRINT_OK) {
    if (finger.storeModel(idAtual) == FINGERPRINT_OK) {
      
      lcd.clear(); lcd.print("Sucesso ID: "); lcd.print(idAtual);
      Serial.println("Salvo com sucesso!");

      // 1. Salva na Memória do ESP
      Aluno novoAluno;
      strcpy(novoAluno.nome, nome.c_str()); 
      novoAluno.matricula = matricula.toInt();
      lista.push_back(novoAluno);
      salvarBancoVector();

      // 2. Envia para o Google (Cadastro)
    
      enviarCadastroGoogle(nome, matricula.toInt(), idAtual);


      // 3. Atualiza ID
      idAtual++;
      preferences.begin("sistema", false);
      preferences.putInt("proximo_id", idAtual);
      preferences.end();
      
    } else {
      Serial.println("Erro ao salvar ID no sensor");
      lcd.clear(); lcd.print("Erro Sensor");
    }
  } else {
    lcd.clear(); lcd.print("Digitais Diferentes");
    Serial.println("Erro: Modelagem falhou");
  }
  
  delay(2000);
  lcd.clear(); lcd.print("IP: "); lcd.print(WiFi.localIP());
}


// ==========================================
//           SETUP E LOOP
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  if(!LittleFS.begin(true)){
    Serial.println("Erro LittleFS");
    return;
  }
  
  Wire.begin(SDA, SCL);
  lcd.begin(16, 2);
  lcd.setRGB(0, 255, 0);

  mySerial.begin(57600, SERIAL_8N1, 16, 17); 
  finger.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println("Sensor OK!");
  } else {
    Serial.println("Sensor NAO encontrado!");
    lcd.print("Erro Sensor");
    while(1);
  }

  // Carrega ID atual
  preferences.begin("sistema", false);
  idAtual = preferences.getInt("proximo_id", 1); 
  preferences.end();
  
  // Carrega Lista de Alunos
  carregarBancoVector();

  // Conecta Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.clear(); lcd.print("Conectando Wi-Fi");
  
  }
  
  delay(2000);
  Serial.println("Wi-Fi OK");
  Serial.println(WiFi.localIP()); 
  
  lcd.clear();
  if(WiFi.status() == WL_CONNECTED) {
      lcd.print("Wi-Fi OK!");
      lcd.setCursor(0,1);
      lcd.print(WiFi.localIP());
  } else {
      lcd.print("Modo Offline");
  }

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });
  server.on("/cadastrar", HTTP_POST, handleCadastro);
  server.begin();
  Serial.println("Servidor Web Iniciado");
}

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

void loop() {
  server.handleClient();
  
  // Timer para verificar fila offline (envia dados acumulados quando internet volta)
  unsigned long tempoAtual = millis();
  if((tempoAtual - tempoAnterior) >= intervalo) { 
    tempoAnterior = tempoAtual;
    
    // Se tem internet e tem arquivo offline, processa
    if(WiFi.status() == WL_CONNECTED && LittleFS.exists("/registro_offline.dat")) {
       processarFilaOffline();
    }
  }

  // Leitura do Sensor
  int id = getFingerprintID();

  if(id == -2) {
    Serial.println("Digital desconhecida.");
    lcd.clear(); lcd.print("Nao Cadastrado");
    delay(1000);
    lcd.clear(); lcd.print("IP: "); lcd.print(WiFi.localIP());
    
  } else if(id != -1){
    // ID Válido encontrado!
    Serial.println("ID Identificado: " + String(id));

    // Verifica se o ID é seguro para ler da lista
    if (id > 0 && id <= lista.size()) {
        String nomeAluno = lista[id-1].nome;
        
        lcd.clear();
        lcd.print("Ola, ");
        lcd.setCursor(0,1);
        lcd.print(nomeAluno);
        
        if(WiFi.status() == WL_CONNECTED) {
            Serial.println("Online: Enviando presenca...");
            enviarPresencaGoogle(id);
        } else {
            Serial.println("Offline: Salvando na fila...");
            registrarOffline(id);
        }
    } else {
        Serial.println("Erro: ID existe no sensor mas nao na lista.");
    }
    
    delay(2000); // Pausa para não registrar mil vezes
    lcd.clear(); lcd.print("IP: "); lcd.print(WiFi.localIP());
  }
}
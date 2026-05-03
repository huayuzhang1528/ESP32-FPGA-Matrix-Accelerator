#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <SPI.h>

const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define LED_PIN   2
#define CS_PIN    5
#define DONE_PIN  4

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
unsigned long lastTimeBotRan;
const int botRequestDelay = 500;

bool fpga_2x2_multiply(uint8_t a[2][2], uint8_t b[2][2], uint32_t result[2][2]) {
    digitalWrite(CS_PIN, LOW);
    delayMicroseconds(10);
    SPI.transfer(a[0][0]); SPI.transfer(a[0][1]);
    SPI.transfer(a[1][0]); SPI.transfer(a[1][1]);
    SPI.transfer(b[0][0]); SPI.transfer(b[0][1]);
    SPI.transfer(b[1][0]); SPI.transfer(b[1][1]);
    digitalWrite(CS_PIN, HIGH);

    uint32_t timeout = 0;
    while (!digitalRead(DONE_PIN)) {
        if (++timeout > 1000000) return false;
    }

    digitalWrite(CS_PIN, LOW);
    delayMicroseconds(10);
    result[0][0] += ((uint16_t)SPI.transfer(0x00) << 8) | SPI.transfer(0x00);
    result[0][1] += ((uint16_t)SPI.transfer(0x00) << 8) | SPI.transfer(0x00);
    result[1][0] += ((uint16_t)SPI.transfer(0x00) << 8) | SPI.transfer(0x00);
    result[1][1] += ((uint16_t)SPI.transfer(0x00) << 8) | SPI.transfer(0x00);
    digitalWrite(CS_PIN, HIGH);
    return true;
}

bool multiplyNxN(uint8_t* A, uint8_t* B, uint32_t* C, int n) {
    int blocks = n / 2;
    for (int i = 0; i < n * n; i++) C[i] = 0;

    for (int bi = 0; bi < blocks; bi++) {
        for (int bj = 0; bj < blocks; bj++) {
            for (int bk = 0; bk < blocks; bk++) {
                uint8_t a[2][2] = {
                    {A[(bi*2)*n + bk*2],   A[(bi*2)*n + bk*2+1]},
                    {A[(bi*2+1)*n + bk*2], A[(bi*2+1)*n + bk*2+1]}
                };
                uint8_t b[2][2] = {
                    {B[(bk*2)*n + bj*2],   B[(bk*2)*n + bj*2+1]},
                    {B[(bk*2+1)*n + bj*2], B[(bk*2+1)*n + bj*2+1]}
                };
                uint32_t tmp[2][2] = {{0,0},{0,0}};
                if (!fpga_2x2_multiply(a, b, tmp)) return false;
                C[(bi*2)*n + bj*2]     += tmp[0][0];
                C[(bi*2)*n + bj*2+1]   += tmp[0][1];
                C[(bi*2+1)*n + bj*2]   += tmp[1][0];
                C[(bi*2+1)*n + bj*2+1] += tmp[1][1];
            }
        }
    }
    return true;
}

bool parseMatrix(String s, uint8_t* vals, int count) {
    s.trim();
    int idx = 0;
    while (idx < count) {
        int space = s.indexOf(' ');
        String token = (space == -1) ? s : s.substring(0, space);
        vals[idx++] = (uint8_t)token.toInt();
        if (space == -1) break;
        s = s.substring(space + 1);
        s.trim();
    }
    return idx == count;
}

void sendMatrixResult(String chat_id, uint32_t* C, int n) {
    String reply = "Result C = A*B:\n<pre>";
    for (int r = 0; r < n; r++) {
        reply += "|";
        for (int c = 0; c < n; c++) {
            String num = String(C[r*n+c]);
            while (num.length() < 4) num = " " + num;
            reply += num + " ";
        }
        reply += "|\n";
    }
    reply += "</pre>";
    bot.sendMessage(chat_id, reply, "HTML");
}

void handleMultiply(String chat_id, String text, String cmd, int n) {
    int total = n * n * 2;
    uint8_t* vals = new uint8_t[total];
    if (!parseMatrix(text.substring(cmd.length()+1), vals, total)) {
        bot.sendMessage(chat_id, "Error: need " + String(total) + " numbers. Format: " + cmd + " [" + String(n*n) + " numbers for A] [" + String(n*n) + " numbers for B]", "");
        delete[] vals;
        return;
    }

    int fpga_calls = (n/2)*(n/2)*(n/2)*2;
    bot.sendMessage(chat_id, "Computing " + String(n) + "x" + String(n) + " matrix multiply, calling FPGA " + String(fpga_calls) + " times...", "");

    int np = (n % 2 == 0) ? n : n + 1;
    uint8_t* A = new uint8_t[np*np]();
    uint8_t* B = new uint8_t[np*np]();
    uint32_t* C = new uint32_t[np*np]();

    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++) {
            A[r*np+c] = vals[r*n+c];
            B[r*np+c] = vals[n*n+r*n+c];
        }

    if (!multiplyNxN(A, B, C, np)) {
        bot.sendMessage(chat_id, "FPGA timeout, please retry.", "");
    } else {
        uint32_t* Cn = new uint32_t[n*n];
        for (int r = 0; r < n; r++)
            for (int c = 0; c < n; c++)
                Cn[r*n+c] = C[r*np+c];
        sendMatrixResult(chat_id, Cn, n);
        delete[] Cn;
    }

    delete[] vals;
    delete[] A;
    delete[] B;
    delete[] C;
}

void handleNewMessages(int numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
        String chat_id = bot.messages[i].chat_id;
        String text    = bot.messages[i].text;

        if (text == "/led_on") {
            digitalWrite(LED_PIN, HIGH);
            bot.sendMessage(chat_id, "LED is ON", "");
        }
        else if (text == "/led_off") {
            digitalWrite(LED_PIN, LOW);
            bot.sendMessage(chat_id, "LED is OFF", "");
        }
        else if (text == "/status") {
            String state = digitalRead(LED_PIN) ? "ON" : "OFF";
            bot.sendMessage(chat_id, "LED status: " + state, "");
        }
        else {
            bool matched = false;
            for (int n = 2; n <= 8; n++) {
                String cmd = "/multiply" + String(n) + " ";
                if (text.startsWith(cmd)) {
                    handleMultiply(chat_id, text, "/multiply" + String(n), n);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                bot.sendMessage(chat_id, "Available commands:\n/led_on\n/led_off\n/status\n/multiplyN [N*N numbers for A] [N*N numbers for B]\nN can be 2 to 8", "");
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    pinMode(CS_PIN, OUTPUT);
    pinMode(DONE_PIN, INPUT);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(CS_PIN, HIGH);

    SPI.begin(18, 19, 23, CS_PIN);
    SPI.setFrequency(100000);
    SPI.setDataMode(SPI_MODE0);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    client.setInsecure();
}

void loop() {
    if (millis() - lastTimeBotRan > botRequestDelay) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        while (numNewMessages) {
            handleNewMessages(numNewMessages);
            numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
        lastTimeBotRan = millis();
    }
}

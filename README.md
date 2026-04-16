# Epson Projector Controller — ESP32-S3-Zero

Controlador para projetor **Epson E24** via RS232 usando um **Waveshare ESP32-S3-Zero** com módulo **MAX3232**.

Acessa o projetor via protocolo **ESC/VP21** (serial RS232 9600 8N1) e expõe:
- **Dashboard web** com botões de controle
- **Receptor UDP** para automação
- **OTA** para atualização de firmware via browser
- **Provisionamento WiFi** via hotspot no primeiro boot

---

## Hardware

| Componente | Descrição |
|---|---|
| ESP32-S3-Zero (Waveshare) | Microcontrolador principal |
| MAX3232 | Conversor de nível RS232 ↔ 3.3V |
| Cabo DB9 | Conexão com a porta RS232 do projetor |

### Conexão MAX3232 ↔ ESP32-S3-Zero

| ESP32-S3-Zero | MAX3232 | RS232 (DB9) |
|---|---|---|
| GPIO5 (TX) | T1IN | Pin 2 → Projetor |
| GPIO6 (RX) | R1OUT | Pin 3 ← Projetor |
| 3.3V | VCC | — |
| GND | GND | — |

> Os pinos TX/RX podem ser alterados via interface web em `/config`.

---

## Funcionalidades

### Provisionamento WiFi (primeiro boot)
1. O ESP32 cria um hotspot: **SSID `Epson-Control`**, senha `12345678`
2. Conecte e acesse **`http://192.168.4.1`**
3. Preencha: SSID da rede, senha, GPIO TX, GPIO RX, porta UDP
4. Clique em **Salvar** — o dispositivo reinicia e conecta ao WiFi

Se a conexão falhar, o ESP32 volta ao modo AP automaticamente.

### Dashboard Web (`http://<ip>/`)

| Botão | Comando ESC/VP21 |
|---|---|
| Ligar | `PWR ON` |
| Desligar | `PWR OFF` |
| HDMI 1 | `SOURCE 30` |
| HDMI 2 | `SOURCE A0` |
| VGA | `SOURCE 11` |
| Componente | `SOURCE 14` |
| Vol + / Vol − | `VOL INC` / `VOL DEC` |
| Mudo ON/OFF | `MUTE ON` / `MUTE OFF` |
| Blank ON/OFF | `MSEL ON` / `MSEL OFF` |
| Atualizar Status | queries `PWR?` `SOURCE?` `LAMP?` |

### API HTTP

```
POST /cmd        {"cmd": "PWR ON"}
                 → {"status": "ok"}

GET  /status     → {"pwr": "01", "source": "30", "lamp": "1234"}

GET  /config     Página de configuração
POST /config     {"ssid":"...","pass":"...","tx":5,"rx":6,"udp":4210}

GET  /ota        Página de upload OTA
POST /ota        Upload binário .bin
```

### Servidor UDP

Porta padrão: **4210** (configurável).

```bash
# Enviar comando
echo "PWR ON" | nc -u <ip-do-esp32> 4210

# Resposta
OK
```

Comandos suportados via UDP:
`PWR ON`, `PWR OFF`, `SOURCE HDMI1`, `SOURCE HDMI2`, `SOURCE VGA`, `SOURCE COMP`,
`VOL+`, `VOL-`, `MUTE ON`, `MUTE OFF`, `BLANK ON`, `BLANK OFF`

### OTA (atualização de firmware)

1. Acesse `http://<ip>/ota`
2. Selecione o arquivo `build/epson-control.bin`
3. Clique em **Enviar Firmware**
4. O ESP32 reinicia automaticamente com o novo firmware

---

## Build e Flash

### Pré-requisitos
- [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)
- Target: `esp32s3`

### Comandos

```bash
# Configurar target (apenas na primeira vez)
idf.py set-target esp32s3

# Compilar
idf.py build

# Flash + monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

O binário gerado é `build/epson-control.bin`.

### Tabela de partições

| Nome | Tipo | Tamanho |
|---|---|---|
| nvs | data/nvs | 24 KB |
| phy_init | data/phy | 4 KB |
| factory | app/factory | 1.5 MB |
| ota_0 | app/ota_0 | 1.5 MB |
| ota_1 | app/ota_1 | 1.5 MB |
| ota_data | data/ota | 8 KB |

---

## Estrutura do projeto

```
├── main/
│   ├── main.c              # Orquestração: boot flow AP/STA
│   ├── nvs_storage.c/.h    # Persistência de configurações (NVS)
│   ├── wifi_manager.c/.h   # AP mode (provisioning) + STA mode
│   ├── uart_rs232.c/.h     # Driver UART 9600 8N1
│   ├── epson_protocol.c/.h # Protocolo ESC/VP21 Epson
│   ├── web_server.c/.h     # Servidor HTTP (dashboard + OTA + config)
│   ├── web_pages.h         # HTML embutido como strings C
│   ├── udp_server.c/.h     # Receptor UDP
│   └── CMakeLists.txt
├── partitions.csv          # Tabela de partições com suporte a OTA
├── CMakeLists.txt
└── sdkconfig
```

---

## Redefinir configurações

Para voltar ao modo de configuração inicial, apague a partição NVS:

```bash
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash
```

---

## Licença

MIT

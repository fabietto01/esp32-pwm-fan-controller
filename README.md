# ESP32 PWM Fan Controller

Gestore automatico per ventole PWM a 4 pin progettato per la ventilazione di un armadio rack.  
Il sistema utilizza due sensori di temperatura:

- **DS18B20** — sensore primario collegato alla sorgente di calore da controllare; guida direttamente la curva di velocità della ventola
- **DHT11** — sensore ambientale (temperatura + umidità dell'ambiente circostante, solo monitoraggio)

I dati di entrambi i sensori sono esposti in tempo reale via HTTP (JSON e Prometheus).

Basato sull'articolo: **[ESP32 PWM Fan Controller — DroneBot Workshop](https://dronebotworkshop.com/esp32-pwm-fan/)**

---

## Caratteristiche

- Controllo automatico della velocità tramite curva di temperatura a 4 zone (basata su DS18B20)
- Velocità minima garantita (la ventola non si avvia mai da 0 RPM → riduce lo stress meccanico)
- Slew rate limiter: la velocità scende lentamente (~8 s da 100% a 0%) ma sale subito al calore
- Lettura RPM reale via pin TACH con debounce software
- Web server integrato con due endpoint HTTP:
  - `GET /data` — risposta JSON con temperature, umidità, velocità e RPM
  - `GET /metrics` — metriche in formato Prometheus (scrape-ready)
- Credenziali WiFi separate dal codice sorgente (mai committate in git)

---

## Hardware utilizzato

| Componente | Note |
|---|---|
| **Ventola Noctua NF-F12 5V PWM** (120 mm, 4 pin, 1500 RPM max) | Ventola controllata |
| **Sensore DS18B20** — temperatura | Sonda di controllo (montata sulla sorgente di calore) |
| **Sensore DHT11** — temperatura + umidità | Sensore ambientale |
| **ESP32** (WROOM-32D) | Microcontrollore principale |
| **Connettore PWM 4 pin** | Adattatore ventola |

---

## Schema di collegamento

Il file [docs/wiring.drawio](docs/wiring.drawio) contiene lo schema completo apribile con [draw.io](https://app.diagrams.net) (gratuito, web) o direttamente su GitHub.

**Alimentatore 5V:**

| Da | A | Colore cavo |
|---|---|---|
| PSU +5V | ESP32 pin 5V | Rosso |
| PSU GND | ESP32 pin GND | Nero |
| PSU +5V | Ventola pin 2 (+5V) | Rosso |
| PSU GND | Ventola pin 1 (GND) | Nero |

> La ventola è alimentata direttamente dal PSU, non dall'ESP32. L'ESP32 fornisce solo i segnali di controllo.

**Ventola 4-pin → ESP32:**

| Pin ventola | Colore | GPIO ESP32 | Note |
|---|---|---|---|
| Pin 1 — GND | Nero | PSU GND | |
| Pin 2 — +5V | Giallo | PSU +5V | |
| Pin 3 — TACH | Verde | GPIO 2 | Pull-up interno, nessun resistore esterno |
| Pin 4 — PWM | Blu | GPIO 4 | |

**Sensore DS18B20 → ESP32 (sonda di controllo):**

| Pin DS18B20 | ESP32 | Note |
|---|---|---|
| VCC | 3.3V | |
| GND | GND | |
| DATA | GPIO 5 | Resistore pull-up 4.7 kΩ tra DATA e VCC |

> Il DS18B20 richiede un resistore pull-up da **4.7 kΩ** tra il pin DATA e il pin VCC.

**Sensore DHT11 → ESP32 (sensore ambientale):**

| Pin DHT11 | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO 18 |

---

## Come funziona

### Ruolo dei sensori

| Sensore | Ruolo | Dati forniti |
|---|---|---|
| **DS18B20** | Sonda di controllo (montata sulla sorgente di calore) | Temperatura → guida la curva ventole |
| **DHT11** | Sensore ambientale (aria dell'ambiente/rack) | Temperatura + umidità → solo monitoraggio |

### Curva di temperatura (4 zone)

La velocità della ventola è determinata esclusivamente dalla temperatura letta dal **DS18B20**:

```
RPM
 │
MAX ┤                              ┌────────────────
    │                             /
    │                            /
    │                           /
MIN ┤          ┌───────────────/
    │          │
  0 ┤──────────┘
    └──────────┬──────────────┬──────────────┬──── °C
             21°C           25°C           35°C
            IDLE            MIN            MAX
```

| Zona | Temperatura DS18B20 | Comportamento |
|---|---|---|
| **OFF** | ≤ 21 °C | Ventola spenta |
| **IDLE** | 21–25 °C | Gira al minimo (~31%) — mantiene l'aria in movimento |
| **RAMP** | 25–35 °C | Rampa lineare da 31% a 100% |
| **FULL** | ≥ 35 °C | Velocità massima |

### Slew rate (gradualità della variazione)

La velocità non cambia bruscamente. È applicato un limite di variazione massima per ciclo:

- **Salita** — istantanea (il calore richiede reazione immediata)
- **Discesa** — graduale, impiega ~8 s per scendere da 100% a 0%. Piccole fluttuazioni di temperatura non causano variazioni di velocità percepibili.

### Lettura RPM (TACH)

Il pin TACH della ventola è un'uscita open-collector che emette **2 impulsi per giro**. L'ESP32 conta gli impulsi tramite interrupt hardware ogni 2 secondi e calcola gli RPM con la formula:

$$\text{RPM} = \frac{\text{impulsi} \times 30000}{\text{tempo campione [ms]}}$$

Un debounce software da 5 ms filtra i falsi impulsi causati dal rumore elettromagnetico del segnale PWM a 25 kHz.

### Web server

Il loop principale chiama `server.handleClient()` ad ogni iterazione, garantendo che il server risponda anche durante i cicli di lettura sensori. Le letture dei sensori avvengono ogni 2 secondi tramite `millis()` (non-bloccante).

**`GET /data`** — JSON:
```json
{
  "control_temperature_c": 42.1,
  "ambient_temperature_c": 27.3,
  "humidity_pct": 45.0,
  "fan_speed_pct": 52,
  "fan_rpm": 820
}
```

**`GET /metrics`** — Prometheus text exposition:
```
# HELP fan_control_temperature_celsius Temperature from DS18B20 sensor (used for fan control)
# TYPE fan_control_temperature_celsius gauge
fan_control_temperature_celsius 42.10
# HELP fan_ambient_temperature_celsius Ambient temperature from DHT11 sensor
# TYPE fan_ambient_temperature_celsius gauge
fan_ambient_temperature_celsius 27.30
# HELP fan_humidity_percent Relative humidity from DHT11 sensor
# TYPE fan_humidity_percent gauge
fan_humidity_percent 45.00
# HELP fan_speed_percent Fan PWM duty cycle percentage (0-100)
# TYPE fan_speed_percent gauge
fan_speed_percent 52
# HELP fan_rpm Fan rotational speed in RPM measured via tachometer
# TYPE fan_rpm gauge
fan_rpm 820
```

---

## Setup e configurazione

### 1. Credenziali WiFi

Le credenziali **non sono nel codice sorgente** e non vengono mai committate in git.

```bash
cp secrets.ini.example secrets.ini
```

Modifica `secrets.ini`:
```ini
[env]
build_flags =
    -D WIFI_SSID=\"NomeRete\"
    -D WIFI_PASSWORD=\"PasswordRete\"
```

### 2. Parametri configurabili

Tutti i parametri sono raggruppati all'inizio di `src/main.cpp`:

| Costante | Default | Descrizione |
|---|---|---|
| `TEMP_IDLE` | 21 °C | Temperatura sotto cui la ventola si spegne |
| `TEMP_MIN` | 25 °C | Inizio della rampa |
| `TEMP_MAX` | 35 °C | Fine della rampa (velocità massima) |
| `PWM_MIN` | 80 | Duty cycle minimo mentre gira (~31%) |
| `PWM_MAX` | 255 | Duty cycle massimo (100%) |
| `SLEW_RATE_UP` | 255 | Max variazione PWM per ciclo in salita |
| `SLEW_RATE_DOWN` | 70 | Max variazione PWM per ciclo in discesa |

### 3. Flash

Aprire il progetto con PlatformIO (VS Code) e usare il task **Upload**, oppure:

```bash
pio run --target upload
```

L'IP assegnato viene stampato sul monitor seriale a 115200 baud all'avvio.

### 4. Integrazione Prometheus

Aggiungere al proprio `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: rack_fan
    static_configs:
      - targets: ['<ip-esp32>:80']
    metrics_path: /metrics
```

---

## Struttura del progetto

```
esp32-pwm-fan-controller/
├── docs/
│   └── wiring.drawio         # Schema di collegamento (draw.io)
├── src/
│   └── main.cpp              # Codice principale
├── platformio.ini            # Configurazione PlatformIO
├── secrets.ini               # Credenziali WiFi (gitignored)
├── secrets.ini.example       # Template credenziali (committato)
└── .gitignore
```

---

## Licenza

Progetto personale basato sul lavoro open di [DroneBot Workshop](https://dronebotworkshop.com/esp32-pwm-fan/).

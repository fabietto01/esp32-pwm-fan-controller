# ESP32 PWM Fan Controller

Gestore automatico per ventole PWM a 4 pin progettato per la ventilazione di un armadio rack.  
Il sistema misura la temperatura e l'umidità interna e regola la velocità della ventola di conseguenza, esponendo i dati in tempo reale via HTTP (JSON e Prometheus).

Basato sull'articolo: **[ESP32 PWM Fan Controller — DroneBot Workshop](https://dronebotworkshop.com/esp32-pwm-fan/)**

---

## Caratteristiche

- Controllo automatico della velocità tramite curva di temperatura a 4 zone
- Velocità minima garantita (la ventola non si avvia mai da 0 RPM → riduce lo stress meccanico)
- Slew rate limiter: la velocità scende lentamente (~8 s da 100% a 0%) ma sale subito al calore
- Lettura RPM reale via pin TACH con debounce software
- Web server integrato con due endpoint HTTP:
  - `GET /data` — risposta JSON con temperatura, umidità, velocità e RPM
  - `GET /metrics` — metriche in formato Prometheus (scrape-ready)
- Credenziali WiFi separate dal codice sorgente (mai committate in git)

---

## Hardware utilizzato

| Componente | Link |
|---|---|
| **Ventola Noctua NF-F12 5V PWM** (120 mm, 4 pin, 1500 RPM max) | [noctua.at](https://www.noctua.at/en/products/nf-f12-5v-pwm) |
| **Sensore DHT11** — temperatura e umidità | [Amazon IT](https://www.amazon.it/AZDelivery-KY-015-Modulo-Sensore-Temperatura/dp/B089W8DB5P/ref=sr_1_1?__mk_it_IT=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=1TPLK7NXI5YHG&dib=eyJ2IjoiMSJ9.lr_G49eRqK5CO8F4-w72oZbW53UfqxqrdN-tFYIcEd0q3wFEQPycQaOcpsG7LjezCNhH6FU8lDYv1NDsxG5lo0aiRLs2MXUHIou2oNHuKzz9JZkyt8RpSYIXoITvScs-e1eOzhwZz20AtzbRC-f8zPdXuFfNb8ScxgveXnjGO8XKvRWk_hNrVU6VVOZkYQdLr2pYcDeDadRxa1xZz6e20j_xxx6FYsESK0_tPaBhFInqJSxwTKs1gEtghlgVnXW3M6I17qUZ0RCi49GMRSN3v6tRPZKLMlgRSn2zPTh4Cq0.IOJ5xdiwod1Wt8F-Ny1B_HXInLleaRHtOk3GG5xJC-g&dib_tag=se&keywords=modulo+sensore+dht1&qid=1777125294&sprefix=modulo+sensore+dht1%2Caps%2C256&sr=8-1) |
| **ESP32** (WROOM-32D) | [Amazon IT](https://www.amazon.it/DUBEUYEW-ESP32-WROOM-32D-Dual-Mode-Microcontroller-Combination/dp/B0CRK52429/ref=sr_1_6?__mk_it_IT=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=3V3OGPM0HX9XU&dib=eyJ2IjoiMSJ9.RnGe_58KQV7wsu9IESMjhamvA4ELDQHUef1802dCbq9B2WeBO8JtWCKyt0M-2iz4R9yASxHbPE_C5-I1F7RZYotAyM98zClp5FkQdVOcDi5Uf-Ksn-iojZ78Y_20w7h3ByktOnDG-ekJxyLqtp3N0t0npoT2YXDNW7Z2fumr09e9uaWYDU1ZdniiD37gmCMZ4n3TMWIhqJyMCMUifodJnT6ab0kvOpCsVj6Nh3bFbY1BAECgJ_9M2noQNFNd8GxFr3-ra_6n9jfFMK3VyMQQ3MHvuS6T_MDevswvC5b66Sk.QLtWRiELfGocMABfP16av5aE08aMXFb4nUbGIThH18Y&dib_tag=se&keywords=esp32&qid=1777125317&sprefix=esp3%2Caps%2C259&sr=8-6) |
| **Connettore PWM 4 pin** (adattatore ventola) | [Amazon IT](https://www.amazon.it/Cable-Matters-Adattatore-connettore-ventola/dp/B0CP9X42LM/ref=sr_1_2_sspa?__mk_it_IT=%C3%85M%C3%85%C5%BD%C3%95%C3%91&crid=3GKN0AKE31CT3&dib=eyJ2IjoiMSJ9.9bc-KQM8N27eSZ69FZFURv3u6jDqkyY2lxTJ4RU3hRY3VnONPnIbb3NE3GIfpBqm5s1E_aSfTbn5BboLLaDhEcl0aiWwM1HWZoC7bwxqE_7qdGtCPDDBYKiz54EaFeHVXVasF_epZ6oKeLQJ8tct5A3Is8-SVuXnp4H49at1BulALZGYl8BUHPpEjPV4DpM0-Ze2ZYraBEMHgQCFx5zX-rGdgo3dEKmE2ykNmeEOnRBWuDZHUSXNJg7HZGTSRG5f6zTrDQDaxgX9EFEBvjnEtVDyKtIozvZ-Ea87kvrSuhw.357EJdgZOGolv57GZDptiH0UZaI7FC5tvQGHPNQ_Ba0&dib_tag=se&keywords=connettore+pwm&qid=1777125336&sprefix=conetore+pvm%2Caps%2C250&sr=8-2-spons&aref=unPYCQWG7H&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1) |

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

**Sensore DHT11 → ESP32:**

| Pin DHT11 | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| DATA | GPIO 18 |

---

## Come funziona

### Curva di temperatura (4 zone)

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

| Zona | Temperatura | Comportamento |
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

Il loop principale chiama `server.handleClient()` ad ogni iterazione, garantendo che il server risponda anche durante i cicli di lettura sensori. La lettura DHT11 avviene ogni 2 secondi tramite `millis()` (non-bloccante).

**`GET /data`** — JSON:
```json
{
  "temperature_c": 27.3,
  "humidity_pct": 45.0,
  "fan_speed_pct": 52,
  "fan_rpm": 820
}
```

**`GET /metrics`** — Prometheus text exposition:
```
# HELP fan_temperature_celsius Temperature reading from DHT11 sensor
# TYPE fan_temperature_celsius gauge
fan_temperature_celsius 27.30
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

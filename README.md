# Proyecto CORRIENTE\_MQTT

Este proyecto muestra un ejemplo completo de lectura de corriente vía UART en un ESP32 y publicación segura (MQTTS) a un broker MQTT con TLS.

---

## 📁 Estructura del repositorio

```
CORRIENTE_MQTT/
├─ .gitignore                # Archivos y carpetas ignoradas
├─ CMakeLists.txt            # CMake principal
├─ README.md                 # Documentación del proyecto
├─ dependencies.lock         # Lockfile para scripts Python
├─ pytest_mqtt_ssl.py        # Script de prueba MQTT/TLS en Python
├─ sdkconfig.ci              # Configuración para CI (ignorable en local)
├─ main/                     # Componente principal de firmware
│  ├─ app_main.c             # Código fuente principal
│  ├─ CMakeLists.txt         # Declaración de componente ESP-IDF
│  ├─ Kconfig                # Definición de opciones de menuconfig
│  └─ hivemq_root_ca.pem     # Certificado CA público para TLS
└─ build/                    # Carpeta de compilación (no versionar)
```

---

## ⚙️ Prerrequisitos

* **ESP‑IDF v5.4.1** configurado en tu máquina.
* **Python 3.11** (usa el entorno virtual que crea ESP‑IDF).
* Conexión de red y acceso al broker MQTT.

---

## 📑 .gitignore

Incluye al repositorio:

```gitignore
# Artefactos de compilación
/build/
/out/
*.elf
*.bin

# Configuraciones autogeneradas
sdkconfig
sdkconfig.old

# IDEs
.vscode/

# Logs y temporales
*.log
```

> **Nota:** `sdkconfig` no se versiona: cada entorno tiene sus propias credenciales.

---

## 🔧 Configuración inicial

1. **Clonar el repo**:

   ```bash
   git clone https://github.com/tu_usuario/CORRIENTE_MQTT.git
   cd CORRIENTE_MQTT
   ```

2. **Setear el target de ESP-IDF** (solo la primera vez):

   ```bash
   idf.py set-target esp32
   ```

3. **Definir opciones de configuración** en `main/Kconfig`:

   ```kconfig
   menu "Example Configuration"

   config BROKER_URI
       string "Broker URL"
       default "mqtts://mqtt.eclipseprojects.io:8883"

   config MQTT_USERNAME
       string "MQTT Username"
       default ""
       help
         Ingresá tu usuario MQTT mediante menuconfig.

   config MQTT_PASSWORD
       string "MQTT Password"
       default ""
       help
         Ingresá tu contraseña MQTT mediante menuconfig.

   endmenu
   ```

4. **Actualizar configuración de CMake** (no suele cambiar si ya está):

   ```bash
   idf.py reconfigure
   ```

5. **Ejecutar menuconfig** y completar valores:

   ```bash
   idf.py menuconfig
   ```

   * Navegá hasta **Example Configuration**.
   * Completá **Broker URL**, **MQTT Username** y **MQTT Password**.
   * Guardá y salí.

6. **Compilar y grabar el firmware**:

   ```bash
   idf.py build
   idf.py flash monitor
   ```

   * `build/` queda local y no se sube a Git.

---

## 🔐 Manejo de credenciales

* **`main/Kconfig`** contiene placeholders, sin exponer secretos.
* Cada colaborador ejecuta `menuconfig` y establece **su** URI, usuario y contraseña en su propio `sdkconfig`.
* En CI, podés usar un archivo `sdkconfig.ci` con variables predefinidas (nunca versionar uno con secretos reales).

---

## 🧪 Script de prueba en Python

El archivo `pytest_mqtt_ssl.py` usa las mismas credenciales para verificar la conexión TLS/MQTT y publicar/recibir mensajes. Ajustalo si cambian URI o credenciales.

---

### 🎯 Flujo completo

```bash
git clone ...
cd CORRIENTE_MQTT
idf.py set-target esp32         # si aún no lo hiciste
idf.py reconfigure
idf.py menuconfig               # completar BROKER_URI, user, pass
idf.py build
idf.py flash monitor
```

¡Y listo! Tu ESP32 empezará a leer corriente por UART y a publicarla en el broker seguro.
